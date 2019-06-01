#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/CallSite.h"
#include "InputInfo.h"


#ifndef TAFFOUTILS_TYPEUTILS_H
#define TAFFOUTILS_TYPEUTILS_H


namespace taffo {

bool isFloatType(llvm::Type *srct);
llvm::Type *fullyUnwrapPointerOrArrayType(llvm::Type *srct);

enum class FixedPointTypeGenError {
  NoError = 0,
  InvalidRange,
  UnboundedRange,
  NotEnoughFracBits,
  NotEnoughIntAndFracBits
};

mdutils::FPType fixedPointTypeFromRange(
  const mdutils::Range& range,
  FixedPointTypeGenError *outerr=nullptr,
  int totalBits=32,
  int fracThreshold=3,
  int maxTotalBits=64,
  int totalBitsIncrement=64);

}


#endif // TAFFOUTILS_TYPEUTILS_H
