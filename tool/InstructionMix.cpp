#include <sstream>
#include "llvm/IR/Intrinsics.h"
#include "InstructionMix.h"

using namespace llvm;


void InstructionMix::updateWithInstruction(Instruction *inst)
{
  ninstr++;
  stat[inst->getOpcodeName()]++;

  if (isa<AllocaInst>(inst) || isa<LoadInst>(inst) || isa<StoreInst>(inst) || isa<GetElementPtrInst>(inst) ) {
    stat["MemOp"]++;
  } else if (isa<PHINode>(inst) || isa<SelectInst>(inst) || isa<FCmpInst>(inst) || isa<CmpInst>(inst) ) {
    stat["CmpOp"]++;
  } else if (isa<CastInst>(inst)) {
    stat["CastOp"]++;
  } else if (inst->isBinaryOp()) {
    stat["MathOp"]++;
    if (inst->getType()->isFloatingPointTy()) {
      stat["FloatingPointOp"]++;
      if (inst->getOpcode() == Instruction::FMul || inst->getOpcode() == Instruction::FDiv)
        stat["FloatMulDivOp"]++;
    } else
      stat["IntegerOp"]++;
  }
  if (inst->isShift()) {
    stat["Shift"]++;
  }

  if (CallBase *call = dyn_cast<CallBase>(inst)) {
    std::stringstream stm;
    
    if (isa<CallInst>(call)) {
      stm << "call(";
    } else {
      stm << "invoke(";
    }
    
    Function *opnd = call->getCalledFunction();
    if (opnd) {
      stm << opnd->getName().str();
    } else {
      stm << "%indirect";
    }
    
    stm << ")";
    stat[stm.str()]++;
  }
}


int isDelimiterInstruction(llvm::Instruction *instr)
{
  CallBase *call = dyn_cast<CallBase>(instr);
  if (!call)
    return 0;
  Function *opnd = call->getCalledFunction();
  StringRef fname;
  if (!opnd) {
    Value *v = call->getOperand(0);
    if (ConstantExpr *cexp = dyn_cast<ConstantExpr>(v)) {
      if (cexp->getOpcode() == Instruction::BitCast) {
        opnd = dyn_cast<Function>(cexp->getOperand(0));
      }
    }
  }
  
  if (!opnd)
    return 0;
  
  if (opnd->getName() == "polybench_timer_start" ||
      opnd->getName() == "timer_start") {
    return +1;
  } else if (opnd->getName() == "polybench_timer_stop" ||
             opnd->getName() == "timer_stop") {
    return -1;
  } else if (opnd->getName().contains("AxBenchTimer")) {
    if (opnd->getName().contains("nanosecondsSinceInit")) {
      return -1;
    } else {
      return +1;
    }
  }
  return 0;
}


bool isSkippableInstruction(llvm::Instruction *instr)
{
  CallBase *call = dyn_cast<CallBase>(instr);
  if (!call)
    return false;
  Function *opnd = call->getCalledFunction();
  if (!opnd)
    return false;
  
  if (opnd->getIntrinsicID() == Intrinsic::ID::annotation ||
      opnd->getIntrinsicID() == Intrinsic::ID::var_annotation ||
      opnd->getIntrinsicID() == Intrinsic::ID::ptr_annotation ||
      opnd->getIntrinsicID() == Intrinsic::ID::dbg_addr ||
      opnd->getIntrinsicID() == Intrinsic::ID::dbg_label ||
      opnd->getIntrinsicID() == Intrinsic::ID::dbg_value ||
      opnd->getIntrinsicID() == Intrinsic::ID::dbg_declare)
    return true;
  return false;
}

