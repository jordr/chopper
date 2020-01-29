#ifndef KEEPER_H
#define KEEPER_H

#include <vector>
#include <string>
#include "llvm/IR/Module.h"
#include "llvm/Analysis/CallGraph.h"
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
  Keeper(llvm::Module *module, klee::Interpreter::SkipMode skipMode, std::vector<klee::Interpreter::SkippedFunctionOption>& selectedFunctions, bool autoKeep, llvm::raw_ostream &debugs)
        : module(module), skipMode(skipMode), selectedFunctions(selectedFunctions), autoKeep(autoKeep), debugs(debugs) {}

  void run();
  inline const std::vector<std::string>& getSkippedTargets() const
    { return skippedTargets; }
  inline klee::Interpreter::SkipMode getSkipMode() const
    { return skipMode; }
  inline bool isSkipping() const
    { return skipMode != klee::Interpreter::CHOP_NONE; }
  inline bool isFunctionToSkip(llvm::StringRef fname) const
    { return std::find(skippedTargets.begin(), skippedTargets.end(), fname.str()) != skippedTargets.end(); }
    // JOR: TODO: this should work with AutoChopper when it will be discriminating calls per fun:lines
    // JOR: at this point, skippedTargets will be a SkippedFunctionOption
    // JOR: for now, we restrict it to Legacy mode to avoid errors
  inline const std::vector<klee::Interpreter::SkippedFunctionOption>& getLegacySelectedFunctions() const
    { assert(skipMode == klee::Interpreter::CHOP_LEGACY); return selectedFunctions; }

private:
  void generateAncestors(std::set<const llvm::Function*>& ancestors);
  void generateSkippedTargets(const std::set<const llvm::Function*>& ancestors);
  static std::string prettifyFileName(llvm::StringRef filename);

  std::vector<std::string> skippedTargets; // semantics: actually skipped functions

  llvm::Module *module;
  klee::Interpreter::SkipMode skipMode;
  std::vector<klee::Interpreter::SkippedFunctionOption>& selectedFunctions; // comes from interpreterOpts.selectedFunctions
  bool autoKeep;
  llvm::raw_ostream &debugs;
};

#endif /* KEEPER_H */
