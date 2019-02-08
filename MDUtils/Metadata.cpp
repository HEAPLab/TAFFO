//===-- Metadata.cpp - Metadata Utils for ErrorPropagator -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definitions of utility functions that handle metadata in Error Propagator.
///
//===----------------------------------------------------------------------===//

#include "Metadata.h"

#include <sstream>

namespace mdutils {

using namespace llvm;

MetadataManager& MetadataManager::getMetadataManager() {
  static MetadataManager Instance;
  return Instance;
}

InputInfo* MetadataManager::retrieveInputInfo(const Instruction &I) {
  return retrieveInputInfo(I.getMetadata(INPUT_INFO_METADATA));
}

InputInfo* MetadataManager::retrieveInputInfo(const GlobalObject &V) {
  return retrieveInputInfo(V.getMetadata(INPUT_INFO_METADATA));
}

void MetadataManager::
retrieveArgumentInputInfo(const Function &F, SmallVectorImpl<InputInfo *> &ResII) {
  MDNode *ArgsMD = F.getMetadata(FUNCTION_ARGS_METADATA);
  if (ArgsMD == nullptr)
    return;

  ResII.reserve(ArgsMD->getNumOperands());
  for (auto ArgMDOp = ArgsMD->op_begin(), ArgMDOpEnd = ArgsMD->op_end();
       ArgMDOp != ArgMDOpEnd; ++ArgMDOp) {
    MDNode *ArgMDNode = cast<MDNode>(ArgMDOp->get());
    ResII.push_back(retrieveInputInfo(ArgMDNode));
  }
}

void MetadataManager::
setInputInfoMetadata(Instruction &I, const InputInfo &IInfo) {
  I.setMetadata(INPUT_INFO_METADATA, IInfo.toMetadata(I.getContext()));
}

void MetadataManager::
setInputInfoMetadata(GlobalObject &V, const InputInfo &IInfo) {
  V.setMetadata(INPUT_INFO_METADATA, IInfo.toMetadata(V.getContext()));
}

void MetadataManager::
setArgumentInputInfoMetadata(Function &F, const ArrayRef<InputInfo *> AInfo) {
  LLVMContext &Context = F.getContext();
  SmallVector<Metadata *, 2U> AllArgsMD;
  AllArgsMD.reserve(AInfo.size());

  for (InputInfo *IInfo : AInfo) {
    assert(IInfo != nullptr);
    AllArgsMD.push_back(IInfo->toMetadata(Context));
  }

  F.setMetadata(FUNCTION_ARGS_METADATA, MDNode::get(Context, AllArgsMD));
}

StructInfo* MetadataManager::retrieveStructInfo(const Instruction &I) {
  return retrieveStructInfo(I.getMetadata(STRUCT_INFO_METADATA));
}

StructInfo* MetadataManager::retrieveStructInfo(const GlobalObject &V) {
  return retrieveStructInfo(V.getMetadata(STRUCT_INFO_METADATA));
}

void MetadataManager::setStructInfoMetadata(Instruction &I, const StructInfo &SInfo) {
  I.setMetadata(STRUCT_INFO_METADATA, SInfo.toMetadata(I.getContext()));
}

void MetadataManager::setStructInfoMetadata(GlobalObject &V, const StructInfo &SInfo) {
  V.setMetadata(STRUCT_INFO_METADATA, SInfo.toMetadata(V.getContext()));
}

void MetadataManager::
setMaxRecursionCountMetadata(Function &F, unsigned MaxRecursionCount) {
  ConstantInt *CIRC = ConstantInt::get(Type::getInt32Ty(F.getContext()),
				       MaxRecursionCount,
				       false);
  ConstantAsMetadata *CMRC = ConstantAsMetadata::get(CIRC);
  MDNode *RCNode = MDNode::get(F.getContext(), CMRC);
  F.setMetadata(MAX_REC_METADATA, RCNode);
}

unsigned MetadataManager::
retrieveMaxRecursionCount(const Function &F) {
  MDNode *RecC = F.getMetadata(MAX_REC_METADATA);
  if (RecC == nullptr)
    return 0U;

  assert(RecC->getNumOperands() > 0 && "Must contain the recursion count.");
  ConstantAsMetadata *CMRC = cast<ConstantAsMetadata>(RecC->getOperand(0U));
  ConstantInt *CIRC = cast<ConstantInt>(CMRC->getValue());
  return CIRC->getZExtValue();
}

void MetadataManager::
setLoopUnrollCountMetadata(Loop &L, unsigned UnrollCount) {
  // Get Loop header terminating instruction
  BasicBlock *Header = L.getHeader();
  assert(Header && "Loop with no header.");

  TerminatorInst *HTI = Header->getTerminator();
  assert(HTI && "Block with no terminator.");

  // Prepare MD Node
  ConstantInt *CIUC = ConstantInt::get(Type::getInt32Ty(HTI->getContext()),
				       UnrollCount,
				       false);
  ConstantAsMetadata *CMUC = ConstantAsMetadata::get(CIUC);
  MDNode *UCNode = MDNode::get(HTI->getContext(), CMUC);

  HTI->setMetadata(UNROLL_COUNT_METADATA, UCNode);
}

void MetadataManager::
setLoopUnrollCountMetadata(Function &F,
			   const SmallVectorImpl<Optional<unsigned> > &LUCs) {
  std::ostringstream EncLUCs;
  for (const Optional<unsigned> &LUC : LUCs) {
    if (LUC.hasValue())
      EncLUCs << LUC.getValue() << " ";
    else
      EncLUCs << "U ";
  }

  F.setMetadata(UNROLL_COUNT_METADATA,
		MDNode::get(F.getContext(),
			    MDString::get(F.getContext(), EncLUCs.str())));
}

Optional<unsigned>
MetadataManager::retrieveLoopUnrollCount(const Loop &L, LoopInfo *LI) {
  Optional<unsigned> MDLUC = retrieveLUCFromHeaderMD(L);

  if (!MDLUC.hasValue() && LI)
    return retrieveLUCFromFunctionMD(L, *LI);

  return MDLUC;
}

Optional<unsigned>
MetadataManager::retrieveLUCFromHeaderMD(const Loop &L) {
  // Get Loop header terminating instruction
  BasicBlock *Header = L.getHeader();
  assert(Header && "Loop with no header.");

  TerminatorInst *HTI = Header->getTerminator();
  assert(HTI && "Block with no terminator.");

  MDNode *UCNode = HTI->getMetadata(UNROLL_COUNT_METADATA);
  if (UCNode == nullptr)
    return NoneType();

  assert(UCNode->getNumOperands() > 0 && "Must contain the unroll count.");
  ConstantAsMetadata *CMUC = cast<ConstantAsMetadata>(UCNode->getOperand(0U));
  ConstantInt *CIUC = cast<ConstantInt>(CMUC->getValue());
  return CIUC->getZExtValue();
}

Optional<unsigned>
MetadataManager::retrieveLUCFromFunctionMD(const Loop &L, LoopInfo &LI) {
  unsigned LIdx = getLoopIndex(L, LI);

  Function *F = L.getHeader()->getParent();
  assert(F);
  SmallVector<Optional<unsigned>, 4U> LUCs = retrieveLUCListFromFunctionMD(*F);

  if (LIdx >= LUCs.size())
    return NoneType();

  return LUCs[LIdx];
}

unsigned
MetadataManager::getLoopIndex(const Loop &L, LoopInfo &LI) {
  unsigned LIdx = 0;
  for (const Loop *CLoop : LI.getLoopsInPreorder()) {
    if (&L == CLoop)
      return LIdx;
    else
      ++LIdx;
  }
  llvm_unreachable("User-provided loop not found in LoopInfo.");
}

SmallVector<Optional<unsigned>, 4U>
MetadataManager::retrieveLUCListFromFunctionMD(Function &F) {
  SmallVector<Optional<unsigned>, 4U> LUCList;

  MDNode *LUCListMDN = F.getMetadata(UNROLL_COUNT_METADATA);
  if (!LUCListMDN)
    return LUCList;

  MDString *LUCListMDS = dyn_cast<MDString>(LUCListMDN->getOperand(0U).get());
  if (!LUCListMDS)
    return LUCList;

  SmallVector<StringRef, 4U> LUCSRefs;
  LUCListMDS->getString().split(LUCSRefs, ' ', -1, false);
  for (const StringRef &LUCSR : LUCSRefs) {
    errs() << LUCSR;
    unsigned LUC;
    if (!LUCSR.getAsInteger(10U, LUC)) {
      LUCList.push_back(LUC);
      errs() << " done\n";
    }
    else {
      LUCList.push_back(NoneType());
      errs() << " nope\n";
    }
  }
  return LUCList;
}

void MetadataManager::setErrorMetadata(Instruction &I, double Error) {
  I.setMetadata(COMP_ERROR_METADATA, createDoubleMDNode(I.getContext(), Error));
}

double MetadataManager::retrieveErrorMetadata(const Instruction &I) {
  return retrieveDoubleMDNode(I.getMetadata(COMP_ERROR_METADATA));
}

void MetadataManager::
setCmpErrorMetadata(Instruction &I, const CmpErrorInfo &CEI) {
  if (!CEI.MayBeWrong)
    return;

  I.setMetadata(WRONG_CMP_METADATA, CEI.toMetadata(I.getContext()));
}

std::unique_ptr<CmpErrorInfo> MetadataManager::
retrieveCmpError(const Instruction &I) {
  return CmpErrorInfo::createFromMetadata(I.getMetadata(WRONG_CMP_METADATA));
}

void MetadataManager::setStartingPoint(Function &F) {
  Metadata *MD[] = {ConstantAsMetadata::get(ConstantInt::getTrue(F.getContext()))};
  F.setMetadata(START_FUN_METADATA, MDNode::get(F.getContext(), MD));
}

bool MetadataManager::isStartingPoint(const Function &F) {
  return F.getMetadata(START_FUN_METADATA) != nullptr;
}

void MetadataManager::setTargetMetadata(Instruction &I, StringRef Name) {
  MDNode *TMD = MDNode::get(I.getContext(), MDString::get(I.getContext(), Name));
  I.setMetadata(TARGET_METADATA, TMD);
}

Optional<StringRef> MetadataManager::retrieveTargetMetadata(const Instruction &I) {
  MDNode *MD = I.getMetadata(TARGET_METADATA);
  if (MD == nullptr)
    return NoneType();

  MDString *MDName = cast<MDString>(MD->getOperand(0U).get());
  return MDName->getString();
}

void MetadataManager::setTargetMetadata(GlobalObject &I, StringRef Name) {
  MDNode *TMD = MDNode::get(I.getContext(), MDString::get(I.getContext(), Name));
  I.setMetadata(TARGET_METADATA, TMD);
}

Optional<StringRef> MetadataManager::retrieveTargetMetadata(const GlobalObject &I) {
  MDNode *MD = I.getMetadata(TARGET_METADATA);
  if (MD == nullptr)
    return NoneType();

  MDString *MDName = cast<MDString>(MD->getOperand(0U).get());
  return MDName->getString();
}


TType *MetadataManager::retrieveTType(MDNode *MDN) {
  if (MDN == nullptr)
    return nullptr;

  auto CachedTT = TTypes.find(MDN);
  if (CachedTT != TTypes.end())
    return CachedTT->second.get();

  std::unique_ptr<TType> TT = TType::createFromMetadata(MDN);
  TType *RetTT = TT.get();

  TTypes.insert(std::make_pair(MDN, std::move(TT)));
  return RetTT;
}

Range *MetadataManager::retrieveRange(MDNode *MDN) {
  if (MDN == nullptr)
    return nullptr;

  auto CachedRange = Ranges.find(MDN);
  if (CachedRange != Ranges.end())
    return CachedRange->second.get();

  std::unique_ptr<Range> NRange = Range::createFromMetadata(MDN);
  Range *RetRange = NRange.get();

  Ranges.insert(std::make_pair(MDN, std::move(NRange)));
  return RetRange;
}

double *MetadataManager::retrieveError(MDNode *MDN) {
  if (MDN == nullptr)
    return nullptr;

  auto CachedError = IErrors.find(MDN);
  if (CachedError != IErrors.end())
    return CachedError->second.get();

  std::unique_ptr<double> NError = CreateInitialErrorFromMetadata(MDN);
  double *RetError = NError.get();

  IErrors.insert(std::make_pair(MDN, std::move(NError)));
  return RetError;
}

InputInfo *MetadataManager::retrieveInputInfo(MDNode *MDN) {
  if (MDN == nullptr)
    return nullptr;

  auto CachedIInfo = IInfos.find(MDN);
  if (CachedIInfo != IInfos.end())
    return CachedIInfo->second.get();

  std::unique_ptr<InputInfo> NIInfo = createInputInfoFromMetadata(MDN);
  InputInfo *RetIInfo = NIInfo.get();

  IInfos.insert(std::make_pair(MDN, std::move(NIInfo)));
  return RetIInfo;
}

StructInfo *MetadataManager::retrieveStructInfo(MDNode *MDN) {
  if (MDN == nullptr)
    return nullptr;

  auto CachedStructInfo = StructInfos.find(MDN);
  if (CachedStructInfo != StructInfos.end())
    return cast<StructInfo>(CachedStructInfo->second.get());

  std::unique_ptr<StructInfo> NSInfo = createStructInfoFromMetadata(MDN);
  StructInfo *RetSInfo = NSInfo.get();

  StructInfos.insert(std::make_pair(MDN, std::move(NSInfo)));
  return RetSInfo;
}

std::unique_ptr<InputInfo> MetadataManager::
createInputInfoFromMetadata(MDNode *MDN) {
  assert(MDN != nullptr);
  assert(MDN->getNumOperands() == 3U && "Must have Type, Range, Initial Error.");

  Metadata *ITypeMDN = MDN->getOperand(0U).get();
  TType *IType = (IsNullInputInfoField(ITypeMDN))
    ? nullptr : retrieveTType(cast<MDNode>(ITypeMDN));

  Metadata *IRangeMDN = MDN->getOperand(1U).get();
  Range *IRange = (IsNullInputInfoField(IRangeMDN))
    ? nullptr : retrieveRange(cast<MDNode>(IRangeMDN));

  Metadata *IErrorMDN = MDN->getOperand(2U).get();
  double *IError = (IsNullInputInfoField(IErrorMDN))
    ? nullptr : retrieveError(cast<MDNode>(IErrorMDN));

  return std::unique_ptr<InputInfo>(new InputInfo(IType, IRange, IError));
}

std::unique_ptr<StructInfo> MetadataManager::
createStructInfoFromMetadata(MDNode *MDN) {
  assert(MDN != nullptr);

  SmallVector<MDInfo *, 4U> Fields;
  Fields.reserve(MDN->getNumOperands());
  for (const MDOperand &MDO : MDN->operands()) {
    Metadata *MDField = MDO.get();
    assert(MDField != nullptr);
    if (IsNullInputInfoField(MDField)) {
      Fields.push_back(nullptr);
    }
    else if (InputInfo::isInputInfoMetadata(MDField)) {
      Fields.push_back(retrieveInputInfo(cast<MDNode>(MDField)));
    }
    else if (MDNode *MDNField = dyn_cast<MDNode>(MDField)) {
      Fields.push_back(retrieveStructInfo(MDNField));
    }
    else {
      llvm_unreachable("Malformed structinfo Metadata.");
    }
  }

  return std::unique_ptr<StructInfo>(new StructInfo(Fields));
}

}
