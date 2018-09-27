//===-- InputInfo.h - Data Structures for Input Info Metadata ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Various data structures that support in-memory representation of
/// input info metadata.
///
//===----------------------------------------------------------------------===//

#ifndef TAFFO_INPUT_INFO_H
#define TAFFO_INPUT_INFO_H

#include <memory>
#include "llvm/Support/Casting.h"
#include "llvm/IR/Metadata.h"

namespace mdutils {
using namespace llvm;

#define FIXP_TYPE_FLAG "fixp"

/// Info about a data type for numerical computations.
class TType {
public:
  enum TTypeKind { K_FPType };

  TType(TTypeKind K) : Kind(K) {}

  virtual double getRoundingError() const = 0;

  /// Safe approximation of the minimum value representable with this Type.
  virtual double getMinValueBound() const = 0;
  /// Safe approximation of the maximum value representable with this Type.
  virtual double getMaxValueBound() const = 0;

  virtual MDNode *toMetadata(LLVMContext &C) const = 0;

  virtual ~TType() = default;

  static std::unique_ptr<TType> createFromMetadata(MDNode *MDN);

  TTypeKind getKind() const { return Kind; }
private:
  const TTypeKind Kind;
};

/// A Fixed Point Type.
/// Contains bit width, number of fractional bits of the format
/// and whether it is signed or not.
class FPType : public TType {
public:
  FPType(unsigned Width, unsigned PointPos, bool Signed = true)
    : TType(K_FPType), Width((Signed) ? -Width : Width), PointPos(PointPos) {}

  FPType(int Width, unsigned PointPos)
    : TType(K_FPType), Width(Width), PointPos(PointPos) {}

  double getRoundingError() const override;
  double getMinValueBound() const override;
  double getMaxValueBound() const override;
  MDNode *toMetadata(LLVMContext &C) const override;
  unsigned getWidth() const { return std::abs(Width); }
  int getSWidth() const { return Width; }
  unsigned getPointPos() const { return PointPos; }
  bool isSigned() const { return Width < 0; }

  static bool isFPTypeMetadata(MDNode *MDN);
  static std::unique_ptr<FPType> createFromMetadata(MDNode *MDN);

  static bool classof(const TType *T) { return T->getKind() == K_FPType; }
protected:
  int Width; ///< Width of the format (in bits), negative if signed.
  unsigned PointPos; ///< Number of fractional bits.
};

struct Range {
public:
  double Min;
  double Max;

  Range() : Min(0.0), Max(0.0) {}
  Range(double Min, double Max) : Min(Min), Max(Max) {}

  MDNode *toMetadata(LLVMContext &C) const;
  static std::unique_ptr<Range> createFromMetadata(MDNode *MDN);
};

std::unique_ptr<double> CreateInitialErrorFromMetadata(MDNode *MDN);
MDNode *InitialErrorToMetadata(double Error);

/// Structure containing pointers to Type, Range, and initial Error
/// of an LLVM Value.
struct InputInfo {
  TType *IType;
  Range *IRange;
  double *IError;

  InputInfo()
    : IType(nullptr), IRange(nullptr), IError(nullptr) {}

  InputInfo(TType *T, Range *R, double *Error)
    : IType(T), IRange(R), IError(Error) {}

  MDNode *toMetadata(LLVMContext &C) const;
};


/// Struct containing info about a possible comparison error.
struct CmpErrorInfo {
public:
  double MaxTolerance; ///< Maximum error tolerance for this comparison.
  bool MayBeWrong; ///< True if this comparison may be wrong due to propagated errors.

  CmpErrorInfo(double MaxTolerance, bool MayBeWrong = true)
    : MaxTolerance(MaxTolerance), MayBeWrong(MayBeWrong) {}

  MDNode *toMetadata(LLVMContext &C) const;

  static std::unique_ptr<CmpErrorInfo> createFromMetadata(MDNode *MDN);
};

MDNode *createDoubleMDNode(LLVMContext &C, double Value);
double retrieveDoubleMDNode(MDNode *MDN);

} // end namespace mdutils

#endif
