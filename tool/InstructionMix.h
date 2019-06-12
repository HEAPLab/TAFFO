#include <string>
#include <unordered_map>
#include "llvm/IR/Instructions.h"


#ifndef INSTRUCTION_MIX_H
#define INSTRUCTION_MIX_H


class InstructionMix
{
public:
  std::map<std::string, int> stat;
  int ninstr;
  
  void updateWithInstruction(llvm::Instruction *instr);
};


#endif


