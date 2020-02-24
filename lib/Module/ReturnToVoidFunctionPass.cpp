//===-- ReturnToVoidFunctionPass.cpp --------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Passes.h"

#include "klee/Internal/Support/Debug.h"
#include "klee/Internal/Support/ErrorHandling.h"
#include "klee/Internal/Analysis/Keeper.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/CFG.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/InstVisitor.h"
#include "llvm/DebugInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace llvm;
using namespace std;

char klee::ReturnToVoidFunctionPass::ID = 0;

bool klee::ReturnToVoidFunctionPass::runOnFunction(Function &f, Module &module) {
  DEBUG_WITH_TYPE(DEBUG_SIGNATURES, klee_warning("BEGIN ReturnToVoidFunctionPass.runOnFunction(%s)", f.getName().str().c_str()));
  // don't check void functions
  if (f.getReturnType()->isVoidTy()) {
    return false;
  }
  
  bool changed = false;
  for (std::vector<Interpreter::SkippedFunctionOption>::const_iterator i = skippedFunctions.begin(); i != skippedFunctions.end(); i++) {
    if (string("__wrap_") + f.getName().str() == i->name) {
      assert((skipMode != Interpreter::CHOP_KEEP || i->lines.empty()) && "TODO: treat the case where lines aren't empty");
      if(skipMode == Interpreter::CHOP_KEEP && f.isVarArg()) { // JOR: TODO: this should be for any skipMode, do it when the variadic stuff is clean
        replaceVariadicCalls(&f, i->lines, module);
      }
      else {
        Function *wrapper = createWrapperFunction(f, module);
        replaceCalls(&f, wrapper, i->lines);
      }
      changed = true;
    }
  }

  return changed;
}

/// We replace a returning function f with a void __wrap_f function that:
///  1- takes as first argument a variable __result that will contain the result
///  2- calls f and stores the return value in __result
Function *klee::ReturnToVoidFunctionPass::createWrapperFunction(Function &f, Module &module) {
  // create new function parameters: *return_var + original function's parameters
  vector<Type *> paramTypes;
  Type *returnType = f.getReturnType();
  
  assert(!returnType->isVoidTy() && "Can't create a wrapper for a void type");
  paramTypes.push_back(PointerType::get(returnType, 0));
  paramTypes.insert(paramTypes.end(), f.getFunctionType()->param_begin(), f.getFunctionType()->param_end());
  
  // create new void function
  FunctionType *newFunctionType = FunctionType::get(Type::getVoidTy(getGlobalContext()), makeArrayRef(paramTypes), f.isVarArg());
  string wrappedName = string("__wrap_") + f.getName().str();
  Function *wrapper = cast<Function>(module.getOrInsertFunction(wrappedName, newFunctionType));

  // set the arguments' name: __result + original parameters' name
  vector<Value *> argsForCall;
  Function::arg_iterator i = wrapper->arg_begin();
  Value *resultArg = i++;
  resultArg->setName("__result");
  for (Function::arg_iterator j = f.arg_begin(); j != f.arg_end(); j++) {
    Value *origArg = j;
    Value *arg = i++;
    arg->setName(origArg->getName());
    argsForCall.push_back(arg);
  }

  // create basic block 'entry' in the new function
  BasicBlock *block = BasicBlock::Create(getGlobalContext(), "entry", wrapper);
  IRBuilder<> builder(block);

  // insert call to the original function
  #ifdef DANIELS_WAY
  if (f.isVarArg()) {
    Type *VAListTy = StructType::create(
      {builder.getInt32Ty(), builder.getInt32Ty(), builder.getInt8PtrTy(), builder.getInt8PtrTy});
    Function *VAStart = Intrinsic::getDeclaration(f.getParent(), Intrinsic::vastart, {builder.getInt8PtrTy()});

    Value *VAListTag = builder.CreateAlloca(VAListTy, builder.getInt32(1), "va_list_tag");
    Value *DecayedVAListtag = builder.CreateBitCast(VAListTag, builder.getInt8PtrTy());
    builder.CreateCall(VAStart, {DecayedVAListtag});
    // Load the second argument call it n
    // Call va_arg n times and push the returned value into argsForCall
    // Call va_end with VAListTag

    // TODO chqnge where it is called to include __vaargs_count as a second argu;ent
  }
  #endif
  Value *callInst = builder.CreateCall(&f, makeArrayRef(argsForCall), "__call");
  // insert store for the return value to __result parameter
  builder.CreateStore(callInst, resultArg);
  // terminate function with void return
  builder.CreateRetVoid();

  return wrapper;
}

/// Replaces calls to f with the wrapper function __wrap_f
/// The replacement will occur at all call sites only if the user has not specified a given line in the '-skip-functions' options
void klee::ReturnToVoidFunctionPass::replaceCalls(Function *f, Function *wrapper, const vector<unsigned int> &lines) {
  vector<CallInst*> to_remove;
  for (auto ui = f->use_begin(), ue = f->use_end(); ui != ue; ui++) {
    if (Instruction *inst = dyn_cast<Instruction>(*ui)) {
      if (inst->getParent()->getParent() == wrapper) {
        continue;
      }

      if (!lines.empty()) {
        if (MDNode *N = inst->getMetadata("dbg")) {
          DILocation Loc(N);
          if (find(lines.begin(), lines.end(), Loc.getLineNumber()) == lines.end()) {
            continue;
          }
        }
      }

      if (CallInst *call = dyn_cast<CallInst>(inst)) {
        if(replaceCall(call, f, wrapper) == 0)
          to_remove.push_back(call);
      }
    }
  }
  for (auto ci = to_remove.begin(), ce = to_remove.end(); ci != ce; ci++) {
    (*ci)->eraseFromParent();
  }
}

/// variadic variant
void klee::ReturnToVoidFunctionPass::replaceVariadicCalls(Function *f, const vector<unsigned int> &lines, Module &module) {
  vector<CallInst*> to_remove;
  for (auto ui = f->use_begin(), ue = f->use_end(); ui != ue; ui++) {
    if (Instruction *inst = dyn_cast<Instruction>(*ui)) {
      if (isMatchingWrapper(inst->getParent()->getParent(), f)) {
        continue;
      }

      if (!lines.empty()) {
        if (MDNode *N = inst->getMetadata("dbg")) {
          DILocation Loc(N);
          if (find(lines.begin(), lines.end(), Loc.getLineNumber()) == lines.end()) {
            continue;
          }
        }
      }

      if (CallInst *call = dyn_cast<CallInst>(inst)) {
        Function* wrapper = getOrMakeWrapper(*f, call, module);
        if(replaceCall(call, f, wrapper) == 0)
          to_remove.push_back(call);
      }
    }
  }
  for (auto ci = to_remove.begin(), ce = to_remove.end(); ci != ce; ci++) {
    (*ci)->eraseFromParent();
  }
}

bool klee::ReturnToVoidFunctionPass::isMatchingWrapper(llvm::Function* wrapper, Function* wrappee) {
  return wrapper->getName().startswith(llvm::StringRef("__wrap_" + wrappee->getName().str()));
}

// JOR: this is only called for variadic functions now
// JOR: TODO merge with createWrapperFunction
// JOR: TODO fix code around that uses == __wrap_ + f
// JOR: TODO build a table of wrappers and check that it is not in there before making one
Function * klee::ReturnToVoidFunctionPass::getOrMakeWrapper(Function& f, CallInst* call, Module &module) {
  assert(f.isVarArg());
  const int totalArgCount = call->getNumArgOperands();
  const int totalStaticArgCount = f.getFunctionType()->getNumParams();

  // JOR: TODO add the attributes to argsForCall
  // JOR: TODO add the attributes to wrapper paramTypes
  
  // create new function parameters: *return_var + original function's parameters + variadic parameters
  vector<Type *> paramTypes;
  Type *returnType = f.getReturnType();
  
  assert(!returnType->isVoidTy() && "Can't create a wrapper for a void type");
  paramTypes.push_back(PointerType::get(returnType, 0));
  paramTypes.insert(paramTypes.end(), f.getFunctionType()->param_begin(), f.getFunctionType()->param_end());

  // add variadic parameters types, from call
  for (int argi = totalStaticArgCount; argi < totalArgCount; argi++) {
    DEBUG_WITH_TYPE("variadic", klee_message("\t- Argument %d is: %s (type %d)", argi, 
      call->getArgOperand(argi)->getName().str().c_str(), call->getArgOperand(argi)->getType()->getTypeID()));
    paramTypes.push_back(call->getArgOperand(argi)->getType());
  }
  
  // create new void function, that is NOT variadic
  FunctionType *newFunctionType = FunctionType::get(Type::getVoidTy(getGlobalContext()), makeArrayRef(paramTypes), false);
  string wrappedName = string("__wrap_") + f.getName().str() + "_vaarg" + std::to_string(totalArgCount);
  Function *wrapper = cast<Function>(module.getOrInsertFunction(wrappedName, newFunctionType));
  DEBUG_WITH_TYPE("variadic", klee_message("Making wrapper '%s'", wrappedName.c_str()));

  // set the arguments' name: __result + original parameters' name + __vaarg_{i}
  vector<Value *> argsForCall;
  Function::arg_iterator i = wrapper->arg_begin();
  Value *resultArg = i++;
  resultArg->setName("__result");
  for (Function::arg_iterator j = f.arg_begin(); j != f.arg_end(); j++) {
    Value *origArg = j;
    Value *arg = i++;
    arg->setName(origArg->getName());
    argsForCall.push_back(arg);
  }
  for (int argi = 0; i != wrapper->arg_end(); argi++) {
    Value *arg = i++;
    arg->setName(std::string("__vaarg_") + std::to_string(argi));
    DEBUG_WITH_TYPE("variadic", klee_message("Added parameter %s", arg->getName().str().c_str()));
    argsForCall.push_back(arg);
  }

  // create basic block 'entry' in the new function
  BasicBlock *block = BasicBlock::Create(getGlobalContext(), "entry", wrapper);
  IRBuilder<> builder(block);

  // insert call to the original function
  Value *callInst = builder.CreateCall(&f, makeArrayRef(argsForCall), "__call");
  // insert store for the return value to __result parameter
  builder.CreateStore(callInst, resultArg);
  // terminate function with void return
  builder.CreateRetVoid();

  DEBUG_WITH_TYPE("variadic", wrapper->dump());

  return wrapper;
}

/// We replace a given CallInst to f with a new CallInst to __wrap_f
/// If the original return value was used in a StoreInst, we use directly such variable, instead of creating a new one
/// JOR: now returns non-zero if failure
int klee::ReturnToVoidFunctionPass::replaceCall(CallInst *origCallInst, Function *f, Function *wrapper) {
  Value *allocaInst = NULL;
  StoreInst *prevStoreInst = NULL;
  bool hasPhiUse = false;

  // We can perform this optimization only when the return value is stored, and
  // that is the _only_ use
  if (origCallInst->getNumUses() == 1) {
    for (auto ui = origCallInst->use_begin(), ue = origCallInst->use_end();
         ui != ue; ui++) {
      if (StoreInst *storeInst = dyn_cast<StoreInst>(*ui)) {
        if (storeInst->getOperand(0) == origCallInst &&
            isa<AllocaInst>(storeInst->getOperand(1))) {
          allocaInst = storeInst->getOperand(1);
          prevStoreInst = storeInst;
        }
      }
    }
  }

  /* check if we have a PHI use */
  for (auto ui = origCallInst->use_begin(), ue = origCallInst->use_end(); ui != ue; ui++) {
    if (isa<PHINode>(*ui)) {
      hasPhiUse = true;
    }
  }

  IRBuilder<> builder(origCallInst);
  // insert alloca for return value
  if (!allocaInst)
    allocaInst = builder.CreateAlloca(f->getReturnType());

  // insert call for the wrapper function
  vector<Value *> argsForCall;
  argsForCall.push_back(allocaInst);
  for (unsigned int i = 0; i < origCallInst->getNumArgOperands(); i++) {
    argsForCall.push_back(origCallInst->getArgOperand(i));

    // JOR
    std::string str;
    llvm::raw_string_ostream ss(str);
    ss << *origCallInst->getArgOperand(i);
    ss << "(TYPE=" << *origCallInst->getArgOperand(i)->getType() << ")";
    ss << "(TID=" << (origCallInst->getArgOperand(i)->getType()->getTypeID()) << ")";
    DEBUG_WITH_TYPE(DEBUG_SIGNATURES, klee_warning("\t- originalCall.argoperand[%d] = %s", i, ss.str().c_str()));
    if(f->isVarArg())
      DEBUG_WITH_TYPE("variadic", klee_warning("\t- originalCall.argoperand[%d] = %s", i, ss.str().c_str()));
  }
  DEBUG_WITH_TYPE(DEBUG_SIGNATURES, klee_warning("CreateCall! wrapper=%s, origCallInst->getNumArgOperands()=%d, ", wrapper->getName().str().c_str(), origCallInst->getNumArgOperands())); 
  FunctionType *FTy = cast<FunctionType>(cast<PointerType>(wrapper->getType())->getElementType());
  const unsigned wrapper_num_params = FTy->getNumParams();
  DEBUG_WITH_TYPE(DEBUG_SIGNATURES, klee_warning("getNumParams of wrapper= %d, type = %d", wrapper_num_params, wrapper->getType()->getTypeID()));
  DEBUG_WITH_TYPE(DEBUG_SIGNATURES, klee_warning("f.CONV:%d, wrapper.CONV:%d", f->getCallingConv(), wrapper->getCallingConv()));

  // JOR dodge LLVM assert....
  llvm::ArrayRef<Value*> Args = makeArrayRef(argsForCall);
  if(!(Args.size() == wrapper_num_params || (FTy->isVarArg() && Args.size() > wrapper_num_params)))
  {
    klee_warning("\e[1;35mWrapper has bad signature: '%s'! LLVM refuses to create call. Args.size()=%d, wrapper_num_params=%d, FTY->isVarArg()=%d",
      f->getName().str().c_str(), (int)Args.size(), wrapper_num_params, (FTy->isVarArg()));
    return 1;
  }
  for (unsigned i = 0; i != Args.size(); ++i)
  {
    if(! (i >= wrapper_num_params || FTy->getParamType(i) == Args[i]->getType()) )
    {
      klee_warning("\e[1;35mWrapper has bad signature: '%s'! bad type of Args[%d].type=%d =/= wrapper.type=%d LLVM refuses to create call.\e[0;m",
        f->getName().str().c_str(), i, Args[i]->getType()->getTypeID(), FTy->getParamType(i)->getTypeID());
      return 1;
    }
  }

  CallInst *callInst = builder.CreateCall(wrapper, Args);
  callInst->setDebugLoc(origCallInst->getDebugLoc());

  // if there was a StoreInst, we remove it
  if (prevStoreInst) {
    prevStoreInst->eraseFromParent();
  } else {
    // otherwise, we create a LoadInst for the return value at each use
    if (hasPhiUse) {
      // FIXME: phi nodes are not easy to handle: 1) we can't add the load as
      // first instruction of the basic block, 2) we need to find a
      // precedessor which dominates all the uses.
      // now relying on unoptimized creation of load
      Value *load = builder.CreateLoad(allocaInst);
      origCallInst->replaceAllUsesWith(load);
    } else {
      while (origCallInst->getNumUses() > 0) {
        llvm::Instruction *II = cast<llvm::Instruction>(*origCallInst->use_begin());
        IRBuilder<> builder_use(II);
        Value *load = builder_use.CreateLoad(allocaInst);
        II->replaceUsesOfWith(origCallInst, load);
      }
    }
  }
  return 0;
}

bool klee::ReturnToVoidFunctionPass::runOnModule(Module &module) {
  // we assume to have everything linked inside the single .bc file
  bool dirty = false;
  for (Module::iterator f = module.begin(), fe = module.end(); f != fe; ++f)
    dirty |= runOnFunction(*f, module);

  return dirty;
}
