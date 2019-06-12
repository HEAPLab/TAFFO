#include "TaffoMLFeaturesAnalysis.h"

using namespace llvm;


char TaffoMLFeatureAnalysisPass::ID = 0;


void TaffoMLFeatureAnalysisPass::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.addRequiredTransitive<LoopInfoWrapperPass>();
}


bool TaffoMLFeatureAnalysisPass::runOnFunction(Function &F)
{
  LoopInfo& li = this->getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  for (Loop *l: li.getLoopsInPreorder()) {
    l->dump();
  }
  return false;
}

