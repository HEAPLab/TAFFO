#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/CallSite.h"


#ifndef TAFFOUTILS_TYPEUTILS_H
#define TAFFOUTILS_TYPEUTILS_H


namespace taffo {

bool isFloatType(llvm::Type *srct);
llvm::Type *fullyUnwrapPointerOrArrayType(llvm::Type *srct);

}


#endif // TAFFOUTILS_TYPEUTILS_H
