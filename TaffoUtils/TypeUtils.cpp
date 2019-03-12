#include "TypeUtils.h"


using namespace taffo;
using namespace llvm;


Type *taffo::fullyUnwrapPointerOrArrayType(Type *srct)
{
  if (srct->isPointerTy()) {
    return fullyUnwrapPointerOrArrayType(srct->getPointerElementType());
  } else if (srct->isArrayTy()) {
    return fullyUnwrapPointerOrArrayType(srct->getArrayElementType());
  }
  return srct;
}


bool taffo::isFloatType(Type *srct)
{
  return fullyUnwrapPointerOrArrayType(srct)->isFloatingPointTy();
}

