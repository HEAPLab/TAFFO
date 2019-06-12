#include <sstream>
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

