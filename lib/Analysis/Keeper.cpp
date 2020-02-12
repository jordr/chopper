#include "llvm/DebugInfo.h"
#include "llvm/PassManager.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "klee/Interpreter.h"
#include "klee/Internal/Analysis/Keeper.h"
#include "klee/Internal/Support/ErrorHandling.h"
#include "../Core/SpecialFunctionHandler.h"
#include <vector>

using klee::Interpreter;
using llvm::Function;

// JOR: TODO: make this a pass

struct FunctionClass {
  enum {
    INVALID=-1,
    SKIP=0,
    AUTOKEEP=1,
    SELECTED=2,
    ANCESTOR=3,
  };
  int type;
  enum {
    NONE=0,
    // this will be the sorting order
    SPECIAL,
    LIBC,
    INTRINSIC,
    HIDDEN_VISIBILITY,
    KLEE,
    SYSTEM,
    INTERNAL,
    OTHER,
  };
  int autokeepReason;
  std::string key;
  llvm::StringRef filename;
  bool isVoidFunction; // the only use of this is to append __wrap_

  FunctionClass() : type(INVALID), autokeepReason(NONE) { }
  bool operator<(const FunctionClass& fc) {
    if(this->type < fc.type)
      return true;
    if(this->type > fc.type)
      return false;
    if(this->type == AUTOKEEP) {
      if(this->autokeepReason < fc.autokeepReason)
        return true;
      if(this->autokeepReason > fc.autokeepReason)
        return false;
    }
    return this->key < fc.key;
  }
};

void Keeper::run() {
  if (skipMode == Interpreter::CHOP_LEGACY) {
    skippedFunctions = selectedFunctions;
    // JOR: TODO: get rid of this ---v
    for (auto i = selectedFunctions.begin(), e = selectedFunctions.end(); i != e; i++)
      skippedTargets.push_back(i->name);
  }
  else if(skipMode == Interpreter::CHOP_KEEP) {
    std::set<const Function*> ancestors;
    generateAncestors(ancestors);
    generateSkippedTargets(ancestors);
    for (auto i = skippedTargets.begin(), e = skippedTargets.end(); i != e; i++) {
      std::vector<unsigned int> empty_lines;
      skippedFunctions.push_back(Interpreter::SkippedFunctionOption(*i, empty_lines));
    }
  }
  else {
    // do nothing
  }
}

void Keeper::generateAncestors(std::set<const Function*>& ancestors) {
  /* Initialize the callgraph */
  klee::klee_message("Building callgraph...");
  llvm::PassManager pm;
  llvm::CallGraph* CG = new llvm::CallGraph();
  pm.add(CG);
  CG->runOnModule(*module); // JOR: TODO: pm.run() doesn't do anything, but without pm.add, we get "UNREACHABLE executed"
  // pm.run(*module);

  klee::klee_message("Seeking ancestors of selected functions...");
  // for each f in the symbol table
  for(llvm::ValueSymbolTable::iterator i = module->getValueSymbolTable().begin(); i != module->getValueSymbolTable().end(); i++) {
    llvm::Value* v_fun = (*i).getValue();
    const llvm::StringRef k_fun = (*i).getKey();
    Function* f = llvm::dyn_cast_or_null<Function>((v_fun));
    if(!f)
      continue;

    for (auto i = selectedFunctions.begin(), e = selectedFunctions.end(); i != e; i++) {
      // for each manually selected function
      if(i->name == k_fun) {
        // if it matches f, add all the ancestors of f
        const std::set<const llvm::Function*>& ancestorsOfF = ReverseReachability::buildReverseReachabilityMap(*CG, f);
        for(auto ci = ancestorsOfF.begin(); ci != ancestorsOfF.end(); ci++) {
          ancestors.insert(*ci);
        }
      }
    }
  }
}

// JOR:
void Keeper::generateSkippedTargets(const std::set<const Function*>& ancestors) {  
  klee::klee_message("Building target list of skipped functions...");
  std::vector<FunctionClass> classifiedFunctions;
  for(llvm::ValueSymbolTable::iterator i = module->getValueSymbolTable().begin(); i != module->getValueSymbolTable().end(); i++) {
    llvm::Value* v_fun = (*i).getValue();
    Function* f = llvm::dyn_cast_or_null<Function>(/* cast_or_null<GlobalValue> */(v_fun));
    if(!f)
        continue;

    FunctionClass funClass;
    funClass.type = FunctionClass::SKIP;
    funClass.key = (*i).getKey();
    funClass.isVoidFunction = f->getReturnType()->isVoidTy();

    for (auto i = selectedFunctions.begin(), e = selectedFunctions.end(); i != e; i++) {
      if(i->name == funClass.key) {
        funClass.type = FunctionClass::SELECTED;
        break;
      }
    }

    { // JOR: getting the filename
      llvm::DebugInfoFinder Finder;
      Finder.processModule(*f->getParent());
      for (llvm::DebugInfoFinder::iterator Iter = Finder.subprogram_begin(), End = Finder.subprogram_end(); Iter != End; ++Iter) {
        const llvm::MDNode* node = *Iter;
        llvm::DISubprogram SP(node);
        if (SP.describes(f)) {
          funClass.filename = SP.getFilename(); // JOR:SP.getFlags() could also be interesting?
          break;
        }
      }
    }

    if(autoKeep && funClass.type == FunctionClass::SKIP) {
      // autokeep includes ancestor lookup for now
      if(ancestors.find(f) != ancestors.end()) {
        funClass.type = FunctionClass::ANCESTOR;
      }
      else if(f->isIntrinsic()) {
        funClass.type = FunctionClass::AUTOKEEP;
        funClass.autokeepReason = FunctionClass::INTRINSIC;
      }
      else if(f->hasHiddenVisibility()) {
        funClass.type = FunctionClass::AUTOKEEP;
        funClass.autokeepReason = FunctionClass::HIDDEN_VISIBILITY;
      }
      else if(!funClass.type && funClass.filename.startswith(llvm::StringRef("libc/"))) {
        funClass.type = FunctionClass::AUTOKEEP;
        funClass.autokeepReason = FunctionClass::LIBC;
      }
      // JOR: TODO: this is hacky, can we use the above + something for klee_init_env and others?
      else if(f->getName().startswith(llvm::StringRef("klee_")) || f->getName().str() == "__fd_stat") {
        funClass.type = FunctionClass::AUTOKEEP;
        funClass.autokeepReason = FunctionClass::KLEE;
      }
      else if(f->getName().str() == "syscall" || f->getName().str() == "open" || f->getName().str() == "close") {
        funClass.type = FunctionClass::AUTOKEEP;
        funClass.autokeepReason = FunctionClass::SYSTEM;
      }
      // else if (f->hasInternalLinkage()) {
      //   funClass.type = FunctionClass::AUTOKEEP;
      //   funClass.autokeepReason = FunctionClass::OTHER;
      // }
      //else if(kmodule->internalFunctions.count(f)) {
      // weak linkage
      // internal linkage
      // hasDLLExportLinkage || hasDLLImportLinkage

      for(klee::SpecialFunctionHandler::const_iterator sf = klee::SpecialFunctionHandler::begin(), se = klee::SpecialFunctionHandler::end(); sf != se; ++sf) {
        if(strcmp(funClass.key.c_str(), sf->name) == 0) {
          // klee::klee_warning("Special function scanned: '%s', doesNotReturn=%d, hasReturnValue=%d", sf->name, sf->doesNotReturn, sf->hasReturnValue);
          funClass.type = FunctionClass::AUTOKEEP;
          funClass.autokeepReason = FunctionClass::SPECIAL;
        }
      }
    }
    classifiedFunctions.push_back(funClass);
  }

  /* Display and write functions */
  sort(classifiedFunctions.begin(), classifiedFunctions.end()); // sort for display
  std::string legacyEquivalentCmd;
  for(auto fi = classifiedFunctions.begin(); fi != classifiedFunctions.end(); fi++) {
    const char* reasonStr = "";

    if(fi->type == FunctionClass::AUTOKEEP)
      switch(fi->autokeepReason) {
        case FunctionClass::SPECIAL:
          reasonStr = "(special)";
          break;
        case FunctionClass::LIBC:
          reasonStr = "(libc)";
          break;
        case FunctionClass::HIDDEN_VISIBILITY:
          reasonStr = "(hidden)";
          break;
        case FunctionClass::INTRINSIC:
          reasonStr = "(intrinsic)";
          break;
        case FunctionClass::KLEE:
          reasonStr = "(KLEE)";
          break;
        case FunctionClass::SYSTEM:
          reasonStr = "(system)";
          break;
        case FunctionClass::INTERNAL:
          reasonStr = "(internal linkage)";
          break;
        case FunctionClass::OTHER:
        default:
          reasonStr = "(other/invalid)";
          break;
      }
    
    if(fi->type == FunctionClass::SKIP) {
      if(legacyEquivalentCmd.empty())
        legacyEquivalentCmd += fi->key;
      else
        legacyEquivalentCmd += "," + fi->key;
    }

    DEBUG_WITH_TYPE("chop", klee::klee_message("\e[49m%s\e[0;m|%s %s %s",
      prettifyFileName(fi->filename).c_str(),
        (fi->type == FunctionClass::ANCESTOR ? "ANCESTOR|\e[0;32m" : 
        (fi->type == FunctionClass::AUTOKEEP ? "AUTOKEEP|\e[0;33m" : 
        (fi->type == FunctionClass::SKIP ? "SKIP    |\e[0;31m" : "KEEP    |\e[0;92m"))),
      fi->key.c_str(),
      reasonStr));

    if(fi->type == FunctionClass::SKIP) {
      // if this was CHOP_LEGACY mode, the __wrap_ prefix would already be put in main.cpp, but with CHOP_KEEP we only put it now
      std::string fname = fi->isVoidFunction ? fi->key : std::string("__wrap_") + fi->key;
      skippedTargets.push_back(fname);
    }
    else if(fi->type == FunctionClass::AUTOKEEP || fi->type == FunctionClass::ANCESTOR) {
      // autokeep: we have to add it to the selected functions vectors
      std::vector<unsigned int> empty_lines;
      selectedFunctions.push_back(Interpreter::SkippedFunctionOption(fi->key, empty_lines));
    }
  }

  klee::klee_message("Equivalent legacy command:  --skip-functions-legacy=%s", legacyEquivalentCmd.c_str());
  	
  // Check that __user_main is kept
  if(std::find(skippedTargets.begin(), skippedTargets.end(), "__user_main") != skippedTargets.end())
    klee::klee_error("\e[1;35mRoot function __user_main is skipped!\e[0;m");
}

std::string Keeper::prettifyFileName(llvm::StringRef filename) {
    const char* sep = "/";//"\u25B6";
    std::string filenamePretty = filename;
    if(filename.find("/") != filename.npos)
    {
      const std::string rootStr = filename.str().substr(0, filename.find("/"));
      std::string remainderStr = filename.substr(filename.find("/")+1);
      const int totalSize = rootStr.size() + remainderStr.size();
      if(totalSize > 30)
        remainderStr = "..." + remainderStr.substr(totalSize - 27);
      filenamePretty = "\e[0;34m\e[7m" + rootStr + "\e[0;34m\e[49m" + sep + "\e[27m" + remainderStr;
      if(totalSize < 30)
        filenamePretty.insert(filenamePretty.end(), 30 - (totalSize), ' ');
    }
    else if(filename.size() < 30)
    {
      filenamePretty.insert(filenamePretty.end(), 31 - filenamePretty.size(), ' ');
      filenamePretty = "\e[0;34m" + filenamePretty;
    }

    filenamePretty += "\e[27m\e[0;44m";
    return filenamePretty;
}

/* reverse reachability stuff */ 
// JOR: TODO: This working list algorithm could be improved, it parses the same element in the working list several times
std::set<const llvm::Function*> 
Keeper::ReverseReachability::buildReverseReachabilityMap(llvm::CallGraph & CG, Function* F) {
  // SmallVector<Function *, 40> Ancestors;
  std::set<const Function*> Ancestors;
  llvm::SmallVector<const Function*, 20> wl;
  // start off with {F}
  wl.push_back(F);
  while(! wl.empty()) {
    // for each fun in the working list
    bool isComplete;
    const Function* fun = wl.pop_back_val();
    const llvm::SmallVector<const Function *, 20>& callers = createCallerTable(CG, fun, isComplete);
 
    // for each caller of fun
    for(auto ci = callers.begin(); ci != callers.end(); ci++) {
      if(*ci != F) { // useless check?
        // if we do not already know about this caller
        if(Ancestors.find(*ci) == Ancestors.end())
        {
          DEBUG_WITH_TYPE("chop", klee::klee_message("\t-Ancestor: '%s' (calls '%s')", (*ci)->getName().str().c_str(), fun->getName().str().c_str()));
          wl.push_back(*ci); // add it to the wl
        }
      }
    }

    // add the callers to the ancestors table
    Ancestors.insert(callers.begin(), callers.end());
    
    // assert(isComplete); //TODO: JOR: investigate?
  }

  return Ancestors;
}

// GlobalVariable* 
llvm::SmallVector<const Function *, 20>
Keeper::ReverseReachability::createCallerTable (llvm::CallGraph & CG, const Function* F, bool &isComplete) {
  llvm::SmallVector<const Function *, 20> Callers;
  // CallGraph & CG = getAnalysis<CallGraph>();
  // llvm::CallGraphNode * CGN = CG[F];
  // Get the call graph node for the function containing the call.
  isComplete = true;

  for(auto cgni = CG.begin(); cgni != CG.end(); cgni++) {
    const Function* Caller = (*cgni).first;
    llvm::CallGraphNode *CGN = (*cgni).second;
    if(Caller == F)
      continue;

    // If there is no called function, then this call can call code external
    // to the module.  In that case, mark the call as incomplete.
    if (!Caller) {
      isComplete = false;
      continue;
    }

    // Iterate through all of the target call nodes and add them to the list of
    // targets to use in the global variable.
    // PointerType * VoidPtrType = ReverseReachability::getVoidPtrType(F->getContext());
    for (llvm::CallGraphNode::iterator ti = CGN->begin(); ti != CGN->end(); ++ti) {
      // See if this call record corresponds to the call site in question.
      llvm::CallGraphNode * CalleeNode = ti->second;
      Function * Target = CalleeNode->getFunction();

      if (Target != F)
        continue;

      //if(Target) klee::klee_message("\t Target = %s, Source = %s, F = %s", Target->getName().str().c_str(), Caller->getName().str().c_str(), F->getName().str().c_str());

      // Do not include intrinsic functions or functions that do not get
      // emitted into the final executable as targets.
      if ((Caller->isIntrinsic()) ||
          (Caller->hasAvailableExternallyLinkage())) {
        continue;
      }

      Callers.push_back(Caller); // JOR: why would I cast it to a void pointer?

      // Add the target to the set of targets.  Cast it to a void pointer first.
      // Targets.push_back (ConstantExpr::getZExtOrBitCast (Target, VoidPtrType));
    }
  }
  return Callers;
  /*
  // Truncate the list with a null pointer.
  Targets.push_back(ConstantPointerNull::get (VoidPtrType));

  // Create the constant array initializer containing all of the targets.
  ArrayType * AT = ArrayType::get (VoidPtrType, Targets.size());
  Constant * CallerArray = ConstantArray::get (AT, Targets);
  return new GlobalVariable (*(F->getParent()),
                             AT,
                             true,
                             GlobalValue::InternalLinkage,
                             CallerArray,
                             "CallerList");
  //*/
}