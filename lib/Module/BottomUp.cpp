//===- BottomUpPass.cpp - Insert indirect function call checks --------------- --//
// 
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This pass instruments indirect function calls to ensure that control-flow
// integrity is preserved at run-time.
//
//===----------------------------------------------------------------------===//

#include <set>
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"
#include "llvm/PassSupport.h"

#include "klee/Internal/Analysis/BottomUp.h"
#include "klee/Internal/Support/ErrorHandling.h"

using namespace llvm;

char BottomUpPass::ID = 0;

static RegisterPass<BottomUpPass>
X ("bottomup", "Look for all possible paths from the root to a function");

std::set<const llvm::Function*> BottomUpPass::buildReverseReachabilityMap(CallGraph & CG, Function* F) {
  // SmallVector<Function *, 40> Ancestors;
  std::set<const Function*> Ancestors;
  SmallVector<const Function*, 20> wl;
  wl.push_back(F);
  while(! wl.empty()) {
    bool isComplete;
    const Function* fun = wl.pop_back_val();
    const llvm::SmallVector<const Function *, 20>& callers = createCallerTable(CG, fun, isComplete);
    for(auto ci = callers.begin(); ci != callers.end(); ci++) {
      if(*ci != F) {
        if(Ancestors.find(*ci) == Ancestors.end()) // we do not already know about this parent
          wl.push_back(*ci);
        Ancestors.insert(callers.begin(), callers.end());
      }
    }
    // assert(isComplete); //TODO: JOR: investigate?
  }

  return Ancestors;
}

// GlobalVariable* 
llvm::SmallVector<const Function *, 20>
BottomUpPass::createCallerTable (CallGraph & CG, const Function* F, bool &isComplete) {
  SmallVector<const Function *, 20> Callers;
  // CallGraph & CG = getAnalysis<CallGraph>();
  // CallGraphNode * CGN = CG[F];
  // Get the call graph node for the function containing the call.
  isComplete = true;

  for(auto cgni = CG.begin(); cgni != CG.end(); cgni++) {
    const Function* Caller = (*cgni).first;
    CallGraphNode *CGN = (*cgni).second;
    if(Caller == F)
      continue;

    // Iterate through all of the target call nodes and add them to the list of
    // targets to use in the global variable.
    // PointerType * VoidPtrType = BottomUpPass::getVoidPtrType(F->getContext());
    for (CallGraphNode::iterator ti = CGN->begin(); ti != CGN->end(); ++ti) {
      // See if this call record corresponds to the call site in question.
      CallGraphNode * CalleeNode = ti->second;
      Function * Target = CalleeNode->getFunction();

      if (Target != F)
        continue;

      // If there is no called function, then this call can call code external
      // to the module.  In that case, mark the call as incomplete.
      if (!Caller) {
        isComplete = false;
        continue;
      }
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

#if 0
// Pass Statistics
namespace {
  STATISTIC (Checks, "CFI Checks Added");
}
//
// Method: createTargetTable()
//
// Description:
//  Create a global variable that contains the targets of the specified
//  function call.
//
// Inputs:
//  CI - A call instruction.
//
// Outputs:
//  isComplete - Flag indicating whether all targets of the call are known.
//
// Return value:
//  A global variable pointing to an array of call targets.
//
GlobalVariable *
BottomUpPass::createTargetTable (CallInst & CI, bool & isComplete) {
  // Get the call graph.
  CallGraph & CG = getAnalysis<CallGraph>();
  // Get the call graph node for the function containing the call.
  CallGraphNode * CGN = CG[CI.getParent()->getParent()];

  // Iterate through all of the target call nodes and add them to the list of
  // targets to use in the global variable.
  isComplete = false;
  PointerType * VoidPtrType = getVoidPtrType(CI.getContext());
  SmallVector<Constant *, 20> Targets;
  for (CallGraphNode::iterator ti = CGN->begin(); ti != CGN->end(); ++ti) {
    // See if this call record corresponds to the call site in question.
    const Value * V = ti->first;
    if (V != &CI)
      continue;

    // Get the node in the graph corresponding to the callee.
    CallGraphNode * CalleeNode = ti->second;

    // Get the target function(s).  If we have a normal callee node as the
    // target, then just fetch the function it represents out of the callee
    // node.  Otherwise, this callee node represents external code that could
    // call any address-taken function.  In that case, we'll have to do extra
    // work to get the potential targets.
    if (CalleeNode == CG.getCallsExternalNode()) {
      // Assume that the check will be incomplete.
      isComplete = false;

      // Get the call graph node that represents external code that calls
      // *into* the program.  Get the list of callees of this node and make
      // them targets.
      CallGraphNode * ExternalNode = CG.getExternalCallingNode();
      for (CallGraphNode::iterator ei = ExternalNode->begin();
                                   ei != ExternalNode->end(); ++ei) {
        if (Function * Target = ei->second->getFunction()) {
          // Do not include intrinsic functions or functions that do not get
          // emitted into the executable in the list of targets.
          if ((Target->isIntrinsic()) ||
              (Target->hasAvailableExternallyLinkage())) {
            continue;
          }

          // Do not include functions with available externally linkage.  These
          // functions are never emitted into the final executable.
          Targets.push_back (ConstantExpr::getZExtOrBitCast (Target,
                                                             VoidPtrType));
        }
      }
    } else {
      // Get the function target out of the node.
      Function * Target = CalleeNode->getFunction();

      // If there is no target function, then this call can call code external
      // to the module.  In that case, mark the call as incomplete.
      if (!Target) {
        isComplete = false;
        continue;
      }

      // Do not include intrinsic functions or functions that do not get
      // emitted into the final executable as targets.
      if ((Target->isIntrinsic()) ||
          (Target->hasAvailableExternallyLinkage())) {
        continue;
      }

      // Add the target to the set of targets.  Cast it to a void pointer
      // first.
      Targets.push_back (ConstantExpr::getZExtOrBitCast (Target, VoidPtrType));
    }
  }

  // Truncate the list with a null pointer.
  Targets.push_back(ConstantPointerNull::get (VoidPtrType));

  // Create the constant array initializer containing all of the targets.
  ArrayType * AT = ArrayType::get (VoidPtrType, Targets.size());
  Constant * TargetArray = ConstantArray::get (AT, Targets);
  return new GlobalVariable (*(CI.getParent()->getParent()->getParent()),
                             AT,
                             true,
                             GlobalValue::InternalLinkage,
                             TargetArray,
                             "TargetList");
}

//
// Method: visitCallInst()
//
// Description:
//  Place a run-time check on a call instruction.
//
void
BottomUpPass::visitCallInst (CallInst & CI) {
  // If the call is inline assembly code or a direct call, then don't insert a
  // check.
  Value * CalledValue = CI.getCalledValue()->stripPointerCasts();
  if ((isa<Function>(CalledValue)) || (isa<InlineAsm>(CalledValue)))
    return;

  // Create the call to the run-time check.
  // The first argument is the function pointer used in the call.
  // The second argument is the pointer to check.
  Value * args[2];
  LLVMContext & Context = CI.getContext();
  bool isComplete = false;
  GlobalVariable * Targets = createTargetTable (CI, isComplete);
  args[0] = castTo (CI.getCalledValue(), getVoidPtrType(Context), &CI);
  args[1] = castTo (Targets, getVoidPtrType(Context), &CI);
  CallInst * Check = CallInst::Create (FunctionCheckUI, args, "", &CI);

  // If there's debug information on the load instruction, add it to the
  // run-time check.
  if (MDNode * MD = CI.getMetadata ("dbg"))
    Check->setMetadata ("dbg", MD);

  // Update the statistics.
  ++Checks;
  return;
}

bool
BottomUpPass::runOnModule (Module & M) {
  // Create a function prototype for the function that performs incomplete
  // function call checks.
  Type *VoidTy    = Type::getVoidTy (M.getContext());
  Type *VoidPtrTy = getVoidPtrType (M.getContext());
  FunctionCheckUI = cast<Function>(M.getOrInsertFunction ("funccheckui",
                                                          VoidTy,
                                                          VoidPtrTy,
                                                          VoidPtrTy,
                                                          NULL));
  assert (FunctionCheckUI && "Function Check function has disappeared!\n");
  FunctionCheckUI->addFnAttr (Attributes::ReadNone);

  // Visit all of the instructions in the function.
  visit (M);
  return true;
}
#endif 

bool
BottomUpPass::runOnModule (Module & M) {
  return true;
}

// } // llvm namespace