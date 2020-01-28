#ifndef KEEPER_H
#define KEEPER_H

#include <vector>
#include <string>
#include <llvm/IR/Module.h>
#include "klee/Interpreter.h"

class Keeper {
  class ReverseReachability {
  public:
    /* reverse reachability stuff */
    ReverseReachability() = delete;
    static std::set<const llvm::Function*> buildReverseReachabilityMap(llvm::CallGraph & CG, llvm::Function* F);
    static llvm::SmallVector<const llvm::Function *, 20> createCallerTable (llvm::CallGraph & CG, const llvm::Function* F, bool &isComplete);
  };
  
public:
  Keeper(llvm::Module *module, klee::Interpreter::SkipMode skipMode, std::vector<klee::Interpreter::SkippedFunctionOption>& functionsToKeep, bool autoKeep, llvm::raw_ostream &debugs)
        : module(module), skipMode(skipMode), functionsToKeep(functionsToKeep), autoKeep(autoKeep), debugs(debugs) {}

  void run();
  inline std::vector<std::string>& getSkippedTargets() 
    { return skippedTargets; }
  inline bool isFunctionToSkip(llvm::StringRef fname) const
    { return std::find(skippedTargets.begin(), skippedTargets.end(), fname.str()) != skippedTargets.end(); }

private:
  void generateAncestors(std::set<const llvm::Function*>& ancestors);
  void generateSkippedTargets(const std::set<const llvm::Function*>& ancestors);
  static std::string prettifyFileName(llvm::StringRef filename);

  std::vector<std::string> skippedTargets; // semantics: actually skipped functions

  llvm::Module *module;
  klee::Interpreter::SkipMode skipMode;
  std::vector<klee::Interpreter::SkippedFunctionOption>& functionsToKeep; // comes from interpreterOpts.selectedFunctions
  bool autoKeep;
  llvm::raw_ostream &debugs;
};

#endif /* KEEPER_H */
