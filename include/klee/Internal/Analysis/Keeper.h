#ifndef KEEPER_H
#define KEEPER_H

#include <vector>
#include <string>
#include "llvm/IR/Module.h"
#include "llvm/Analysis/CallGraph.h"
#include "klee/Interpreter.h"

#include "ReachabilityAnalysis.h"

class Keeper {
  class ReverseReachability {
  public:
    /* reverse reachability stuff */
    ReverseReachability() = delete;
    static std::set<const llvm::Function*> buildReverseReachabilityMap(llvm::CallGraph & CG, llvm::Function* F);
    static llvm::SmallVector<const llvm::Function *, 20> createCallerTable (llvm::CallGraph & CG, const llvm::Function* F, bool &isComplete);
  };
  
public:
  Keeper(llvm::Module *module, ReachabilityAnalysis *ra, klee::Interpreter::SkipMode skipMode, std::vector<klee::Interpreter::SkippedFunctionOption>& selectedFunctions, bool autoKeep, llvm::raw_ostream &debugs)
        : module(module), ra(ra), skipMode(skipMode), selectedFunctions(selectedFunctions), autoKeep(autoKeep), debugs(debugs) {}
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
    { return std::find(skippedTargets.begin(), skippedTargets.end(), fname.str()) != skippedTargets.end(); }
    
private:
  void generateAncestors(std::set<const llvm::Function*>& ancestors);
  void generateSkippedTargets(const std::set<const llvm::Function*>& ancestors);
  static std::string prettifyFileName(llvm::StringRef filename);

  std::vector<std::string> skippedTargets; // semantics: actually skipped functions
  std::vector<klee::Interpreter::SkippedFunctionOption> skippedFunctions;

  llvm::Module *module;
  ReachabilityAnalysis *ra;
  klee::Interpreter::SkipMode skipMode;
  std::vector<klee::Interpreter::SkippedFunctionOption>& selectedFunctions; // comes from interpreterOpts.selectedFunctions
  bool autoKeep;
  llvm::raw_ostream &debugs;
};

#endif /* KEEPER_H */
