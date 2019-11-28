#ifndef BOTTOM_UP_H
#define BOTTOM_UP_H

// #include "MemoryModel/PointerAnalysis.h"
// #include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Pass.h>
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Analysis/CallGraph.h"

class BottomUpPass : public llvm::ModulePass {

public:
  static char ID;

  BottomUpPass()
      : llvm::ModulePass(ID) {}

  virtual bool runOnModule(llvm::Module &module);
  
  virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const {
    // Required passes
    AU.addRequired<llvm::CallGraph>();

    // Preserved passes
    AU.setPreservesCFG(); // JOR: not sure about that one...
  };

  virtual inline const char *getPassName() const { return "BottomUp Pass"; }

  static std::set<const llvm::Function*> buildReverseReachabilityMap(llvm::CallGraph & CG, llvm::Function* F);

  // virtual llvm::GlobalVariable* createCallerTable (llvm::Function* f, bool &isComplete);
  static llvm::SmallVector<const llvm::Function *, 20> createCallerTable (llvm::CallGraph & CG, const llvm::Function* f, bool &isComplete);

  // virtual llvm::GlobalVariable* createTargetTable (llvm::CallInst & CI, bool &isComplete);

private:
  //
  // Function: getVoidPtrType()
  //
  // Description:
  //  Return a pointer to the LLVM type for a void pointer.
  //
  // Return value:
  //  A pointer to an LLVM type for the void pointer.
  //
  static inline
  llvm::PointerType * getVoidPtrType (llvm::LLVMContext & C) {
    llvm::Type * Int8Type  = llvm::IntegerType::getInt8Ty(C);
    return llvm::PointerType::getUnqual(Int8Type);
  }
};

#endif /* AAPASS_H */
