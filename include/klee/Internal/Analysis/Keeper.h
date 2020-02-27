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
  Keeper(llvm::Module *module,
    klee::Interpreter::SkipMode skipMode,
    std::vector<klee::Interpreter::SkippedFunctionOption>& selectedFunctions,
    std::vector<klee::Interpreter::SkippedFunctionOption>& whitelist,
    bool autoKeep)
      : module(module), skipMode(skipMode), selectedFunctions(selectedFunctions), whitelist(whitelist), autoKeep(autoKeep) {}
  void run();

  inline klee::Interpreter::SkipMode getSkipMode() const
    { return skipMode; }
  inline bool isSkipping() const
    { return skipMode != klee::Interpreter::CHOP_NONE; }
  const std::vector<klee::Interpreter::SkippedFunctionOption>& getSkippedFunctions() const // this is the same as getSkippedTargets, but with line info
    { return skippedFunctions; }
  inline const std::vector<std::string>& getSkippedTargets() const
    { return skippedTargets; }
  inline bool isFunctionToSkip(llvm::StringRef fname) const
   { return fname.startswith(llvm::StringRef("__wrap_")) // for variadics
   || std::find(skippedTargets.begin(), skippedTargets.end(), fname.str()) != skippedTargets.end(); }
  // JOR: TODO: make isFunctionToSkip pretty again
    
private:
  void generateAncestors(std::set<const llvm::Function*>& ancestors);
  void generateSkippedTargets(const std::set<const llvm::Function*>& ancestors);
  llvm::StringRef getFilenameOfFunction(llvm::Function* f);
  static std::string prettifyFilename(llvm::StringRef filename);

  // @brief Actually skipped targets
  std::vector<std::string> skippedTargets;
  // @brief Actually skipped functions (redundancy with skippedTargets, adds line info)
  std::vector<klee::Interpreter::SkippedFunctionOption> skippedFunctions;

  llvm::Module *module;
  // @brief Chopper mode (legacy, keep, or none)
  klee::Interpreter::SkipMode skipMode;
  // @brief functions selected in interpreterOpts.selectedFunctions
  std::vector<klee::Interpreter::SkippedFunctionOption>& selectedFunctions;
  // @brief whitelist of functions to keep, comes from interpreterOpts as well
  std::vector<klee::Interpreter::SkippedFunctionOption>& whitelist;
  // @brief Whether autokeep is activated or not (whitelists bad functions, looks for ancestors)
  bool autoKeep;
};

#endif /* KEEPER_H */
