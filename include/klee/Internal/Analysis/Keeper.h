#ifndef KEEPER_H
#define KEEPER_H

#include <vector>
#include <string>
#include "llvm/IR/Module.h"
#include "llvm/Analysis/CallGraph.h"
#include "klee/Interpreter.h"
#include "klee/TimerStatIncrementer.h" // chopstats
#include "klee/ExecutionState.h" // recovery info
// namespace klee {
//   class RecoveryInfo;
// }

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
      : module(module), skipMode(skipMode), selectedFunctions(selectedFunctions), userWhitelist(whitelist), autoKeep(autoKeep) {}
  void run();

  inline klee::Interpreter::SkipMode getSkipMode() const
    { return skipMode; }
  inline bool isSkipping() const
    { return skipMode != klee::Interpreter::CHOP_NONE; }
  const std::vector<klee::Interpreter::SkippedFunctionOption>& getSkippedFunctions() const // this is the same as getSkippedTargets, but with line info
    { return skippedFunctions; }
  inline const std::vector<std::string>& getSkippedTargets() const
    { return skippedTargets; }
  inline const std::vector<std::string>& getDynamicWhitelist() const
    { return dynamicWhitelist; }
  // @brief check is a function should be skipped, updates whitelist
  bool isFunctionToSkip(llvm::Function* f) ;
  // @brief return true if whitelist was updated
  bool updateWhiteList(llvm::Function* f);
  void skippingRiskyFunction(llvm::Function* f);
  void recoveringFunction(klee::ref<klee::RecoveryInfo> ri);
  void recoveredFunction(llvm::Function* f);
    
private:
  void generateAncestors(std::set<const llvm::Function*>& ancestors);
  void generateSkippedTargets(const std::set<const llvm::Function*>& ancestors);
  llvm::StringRef getFilenameOfFunction(llvm::Function* f);
  static std::string prettifyFilename(llvm::StringRef filename);

  llvm::Module *module;

  // @brief Actually skipped targets
  std::vector<std::string> skippedTargets;
  // @brief Actually skipped functions (redundancy with skippedTargets, adds line info)
  std::vector<klee::Interpreter::SkippedFunctionOption> skippedFunctions;
  // @brief dynamically built whitelist of functions, only used for restarting for now
  std::vector<std::string> dynamicWhitelist;

  // below are inputs
  // @brief Chopper mode (legacy, keep, or none)
  klee::Interpreter::SkipMode skipMode;
  // @brief functions selected in interpreterOpts.selectedFunctions
  std::vector<klee::Interpreter::SkippedFunctionOption>& selectedFunctions;
  // @brief whitelist of functions to keep, comes from interpreterOpts as well
  std::vector<klee::Interpreter::SkippedFunctionOption>& userWhitelist;
  // @brief Whether autokeep is activated or not (whitelists bad functions, looks for ancestors)
  bool autoKeep;

  // below is for skipping heuristics 
  class ChopperStats {
  public:
    int numSkips;
    int numRecoveries;
    uint64_t totalRecoveryTime;
    klee::WallTimer* recoveryTimer;
    int recoveryStackCount; // JOR: TODO: we should get rid of this hack

    ChopperStats() : numSkips(0), numRecoveries(0), totalRecoveryTime(0), recoveryTimer(0x0), recoveryStackCount(0) { }
    bool adviseWhitelisting() const;
  };
  std::map<llvm::Function*, ChopperStats> chopstats;

  // @brief fetches chopstats of f, creates one if there are none
  ChopperStats& getOrInsertChopstats(llvm::Function* f) {
    if(chopstats.find(f) == chopstats.end())
      chopstats.insert(std::pair<llvm::Function*, ChopperStats>(f, ChopperStats()));
    return chopstats[f];
  }

public:
  // @brief fetches chopstats of f, assumes it exists
  int getRecoveriesCount(llvm::Function*f) {
    return chopstats[f].numRecoveries;
  }
};

#endif /* KEEPER_H */
