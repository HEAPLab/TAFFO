//===-- Metadata.h - Metadata Utilities -------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Declarations of utility functions that handle metadata in TAFFO.
///
//===----------------------------------------------------------------------===//

#ifndef ERRORPROPAGATOR_METADATA_H
#define ERRORPROPAGATOR_METADATA_H

#include <memory>
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Function.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/GlobalObject.h"
#include "llvm/ADT/Optional.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Constants.h"
#include "InputInfo.h"

#define INPUT_INFO_METADATA    "taffo.info"
#define FUNCTION_ARGS_METADATA "taffo.funinfo"
#define COMP_ERROR_METADATA    "taffo.abserror"
#define WRONG_CMP_METADATA     "taffo.wrongcmptol"
#define MAX_REC_METADATA       "taffo.maxrec"
#define UNROLL_COUNT_METADATA  "taffo.unroll"
#define START_FUN_METADATA     "taffo.start"
#define TARGET_METADATA        "taffo.target"
#define ORIGINAL_FUN_METADATA  "taffo.originalCall"
#define CLONED_FUN_METADATA    "taffo.equivalentChild"
#define SOURCE_FUN_METADATA    "taffo.sourceFunction"

namespace mdutils {

using namespace llvm;

/// Class that converts LLVM Metadata into the in.memory representation.
/// It caches internally the converted data structures
/// to reduce memory consumption and conversion overhead.
/// The returned pointers have the same lifetime as the MetadataManager instance.
class MetadataManager {
public:
  static MetadataManager& getMetadataManager();

  /// Get the Input Info (Type, Range, Initial Error) attached to I.
  InputInfo* retrieveInputInfo(const Instruction &I);

  /// Get the Input Info (Type, Range, Initial Error) attached to Globel Variable V.
  InputInfo* retrieveInputInfo(const GlobalObject &V);

  /// Fill vector ResII with the InputInfo for F's parameters retrieved from F's metadata.
  void retrieveArgumentInputInfo(const Function &F,
				 SmallVectorImpl<InputInfo *> &ResII);

  /// Attach to Instruction I an input info metadata node
  /// containing Type info T, Range, and initial Error.
  static void setInputInfoMetadata(Instruction &I, const InputInfo &IInfo);

  /// Attach Input Info metadata to global object V.
  /// IInfo contains pointers to type, range and initial error
  /// to be attached to global object V as metadata.
  static void setInputInfoMetadata(GlobalObject &V, const InputInfo &IInfo);

  /// Attach metadata containing types, ranges and initial absolute errors
  /// for each argument of the given function.
  /// AInfo is an array/vector of InputInfo object containing type,
  /// range and initial error of each formal parameter of F.
  /// Each InputInfo object refers to the function parameter with the same index.
  static void setArgumentInputInfoMetadata(Function &F,
					   const ArrayRef<InputInfo *> AInfo);

  /// Attach MaxRecursionCount to the given function.
  /// Attaches metadata containing the maximum number of recursive calls
  /// that is allowed for function F.
  static void
  setMaxRecursionCountMetadata(Function &F, unsigned MaxRecursionCount);

  /// Read the MaxRecursionCount from metadata attached to function F.
  /// Returns 0 if no metadata have been found.
  static unsigned retrieveMaxRecursionCount(const Function &F);

  /// Attach unroll count metadata to loop L.
  /// Attaches UnrollCount as the number of times to unroll loop L
  /// as metadata to the terminator instruction of the loop header.
  static void
  setLoopUnrollCountMetadata(Loop &L, unsigned UnrollCount);

  /// Attach loop unroll metadata to Function F.
  /// Loop unroll counts must be provided in Loop preorder.
  static void
  setLoopUnrollCountMetadata(Function &F, const SmallVectorImpl<Optional<unsigned> > &LUCs);

  /// Read loop unroll count from metadata attached to the header of L.
  static Optional<unsigned> retrieveLoopUnrollCount(const Loop &L, LoopInfo *LI = nullptr);

  /// Attach metadata containing the computed error to the given instruction.
  static void setErrorMetadata(Instruction &I, double Error);

  /// Get the error propagated for I from metadata.
  static double retrieveErrorMetadata(const Instruction &I);

  /// Attach maximum error tolerance to Cmp instruction.
  /// The metadata are attached only if the comparison may be wrong.
  static void setCmpErrorMetadata(Instruction &I, const CmpErrorInfo &CEI);

  /// Get the computed comparison error info from metadata attached to I.
  static std::unique_ptr<CmpErrorInfo> retrieveCmpError(const Instruction &I);

  /// Set this function as a starting point for error analysis.
  static void setStartingPoint(Function &F);

  /// Returns true if F has been marked as a starting point for error analysis.
  static bool isStartingPoint(const Function &F);

  /// Mark instruction/global variable I/V as a target with name `Name'.
  static void setTargetMetadata(Instruction &I, StringRef Name);
  static void setTargetMetadata(GlobalObject &V, StringRef Name);

  /// Get the name of the target of this instruction/global variable,
  /// if it is a target. Returns an empty Optional if it is not a target.
  static Optional<StringRef> retrieveTargetMetadata(const Instruction &I);
  static Optional<StringRef> retrieveTargetMetadata(const GlobalObject &V);

protected:
  DenseMap<MDNode *, std::unique_ptr<TType> > TTypes;
  DenseMap<MDNode *, std::unique_ptr<Range> > Ranges;
  DenseMap<MDNode *, std::unique_ptr<double> > IErrors;
  DenseMap<MDNode *, std::unique_ptr<InputInfo> > IInfos;

  TType *retrieveTType(MDNode *MDN);
  Range *retrieveRange(MDNode *MDN);
  double *retrieveError(MDNode *MDN);
  InputInfo *retrieveInputInfo(MDNode *MDN);

  std::unique_ptr<InputInfo> createInputInfoFromMetadata(MDNode *MDN);

  static Optional<unsigned> retrieveLUCFromHeaderMD(const Loop &L);
  static Optional<unsigned> retrieveLUCFromFunctionMD(const Loop &L, LoopInfo &LI);
  static unsigned getLoopIndex(const Loop &L, LoopInfo &LI);
  static SmallVector<Optional<unsigned>, 4U> retrieveLUCListFromFunctionMD(Function &F);

private:
  static MetadataManager Instance;

  MetadataManager() = default;
  MetadataManager(const MetadataManager &) = delete;
};

}

#endif
