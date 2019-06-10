#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <deque>
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_os_ostream.h"

using namespace llvm;


cl::OptionCategory IstrFreqOptions("istr_type options");
cl::opt<bool> Verbose("verbose", cl::value_desc("verbose"),
  cl::desc("Enable Verbose Output"),
  cl::init(false));
cl::opt<bool> CountCallSite("callsites", cl::value_desc("callsites"),
  cl::desc("Count call instructions to profiled functions"),
  cl::init(false));
cl::opt<std::string> InputFilename(cl::Positional, cl::Required,
  cl::desc("<input file>"));


struct block_eval_status {
  BasicBlock *block;
  int eval;
  
  block_eval_status(BasicBlock *bb, int e): block(bb), eval(e) {};
};


bool analyze_function(Function *, std::unordered_set<BasicBlock *>&, std::map<std::string, int>&, int &, int &);
void analyze_basic_block(BasicBlock *, std::unordered_set<BasicBlock *>&, std::map<std::string, int>&, int &, int &);


void analyze_basic_block(BasicBlock *bb, std::unordered_set<BasicBlock *>& countedbbs, std::map<std::string, int>& stat, int &eval, int &ninstr)
{
  if (Verbose) {
    raw_os_ostream stm(std::cerr);
    stm << "  BasicBlock: ";
    bb->printAsOperand(stm, true);
    stm << '\n';
  }
  countedbbs.insert(bb);
  
  for (auto iter3 = bb->begin(); iter3 != bb->end(); iter3++) {
    Instruction &inst = *iter3;

    Function *opnd = nullptr;
    CallInst *call = dyn_cast<CallInst>(&inst);
    if (call)
      opnd = call->getCalledFunction();
    InvokeInst *invoke = dyn_cast<InvokeInst>(&inst);
    if (invoke)
      opnd = invoke->getCalledFunction();
    
    if (opnd) {
      if (opnd->getName() == "polybench_timer_start" ||
          opnd->getName() == "timer_start") {
        eval++;
        continue;
      } else if (opnd->getName() == "polybench_timer_stop" ||
                 opnd->getName() == "timer_stop") {
        eval--;
      } else if (opnd->getName().contains("AxBenchTimer")) {
        if (opnd->getName().contains("nanosecondsSinceInit")) {
          eval--;
        } else {
          eval++;
          continue;
        }
      } else if (opnd->getIntrinsicID() == Intrinsic::ID::annotation ||
                 opnd->getIntrinsicID() == Intrinsic::ID::var_annotation ||
                 opnd->getIntrinsicID() == Intrinsic::ID::ptr_annotation) {
        continue;
      } else {
        bool success = analyze_function(opnd, countedbbs, stat, eval, ninstr);
        if (success && !CountCallSite)
          continue;
      }
    }

    if (!eval)
      continue;
    
    ninstr++;
    stat[inst.getOpcodeName()]++;

    if (isa<AllocaInst>(inst) || isa<LoadInst>(inst) || isa<StoreInst>(inst) || isa<GetElementPtrInst>(inst) ) {
      stat["MemOp"]++;
    } else if (isa<PHINode>(inst) || isa<SelectInst>(inst) || isa<FCmpInst>(inst) || isa<CmpInst>(inst) ) {
      stat["CmpOp"]++;
    } else if (isa<CastInst>(inst)) {
      stat["CastOp"]++;
    } else if (inst.isBinaryOp()) {
      stat["MathOp"]++;
      if (inst.getType()->isFloatingPointTy()) {
        stat["FloatingPointOp"]++;
        if (inst.getOpcode() == Instruction::FMul || inst.getOpcode() == Instruction::FDiv)
          stat["FloatMulDivOp"]++;
      } else
        stat["IntegerOp"]++;
    }
    if (inst.isShift()) {
      stat["Shift"]++;
    }
    
    if (call || invoke) {
      std::stringstream stm;
      
      if (call) {
        stm << "call(";
      } else {
        stm << "invoke(";
      }
      
      if (opnd) {
        stm << opnd->getName().str();
      } else {
        stm << "%indirect";
      }
      
      stm << ")";
      stat[stm.str()]++;
    }
  }
  
  return;
}


bool analyze_function(Function *f, std::unordered_set<BasicBlock *>& countedbbs, std::map<std::string, int>& stat, int &eval, int &ninstr)
{
  if (Verbose)
    std::cerr << " Function: " << f->getName().str() << std::endl;
  
  auto &bblist = f->getBasicBlockList();
  if (bblist.empty()) {
    return false;
  }
  
  if (countedbbs.find(&(f->getEntryBlock())) != countedbbs.end()) {
    if (Verbose)
      std::cerr << "Recursion!" << std::endl;
    return true;
  }
  
  std::deque<block_eval_status> queue;
  queue.push_back(block_eval_status(&f->getEntryBlock(), eval));
  while (queue.size() > 0) {
    block_eval_status top = queue.front();
    queue.pop_front();
    
    analyze_basic_block(top.block, countedbbs, stat, top.eval, ninstr);
    eval = top.eval;
    
    Instruction *term = top.block->getTerminator();
    assert(term && "denormal bb found; abort");
    int numbb = term->getNumSuccessors();
    for (int bbi = 0; bbi < numbb; bbi++) {
      BasicBlock *nextbb = term->getSuccessor(bbi);
      if (countedbbs.find(nextbb) != countedbbs.end()) {
        if (Verbose)
          std::cerr << "Loop!" << std::endl;
        continue;
      }
      
      queue.push_back(block_eval_status(nextbb, top.eval));
    }
  }
  
  return true;
}


int main(int argc, char *argv[])
{
  cl::ParseCommandLineOptions(argc, argv);

  LLVMContext c;
  SMDiagnostic Err;
  std::unique_ptr<Module> m = parseIRFile(InputFilename, Err, c);

  if (Verbose) {
    std::cerr << "Successfully read Module:" << std::endl;
    std::cerr << " Name: " << m.get()->getName().str() << std::endl;
    std::cerr << " Target triple: " << m->getTargetTriple() << std::endl;
  }

  int eval = 0;
  int ninstr = 0;
  std::map<std::string, int> stat;
  std::unordered_set<BasicBlock *> bbs;
  
  Function *mainfunc = m->getFunction("main");
  if (!mainfunc) {
    std::cout << "No main function found!\n";
  } else {
    analyze_function(mainfunc, bbs, stat, eval, ninstr);
  }

  for (auto iter1 = m->getFunctionList().begin();
       iter1 != m->getFunctionList().end(); iter1++) {
    Function &f = *iter1;
    
    if (f.getName() != "main")
      continue;
  }

  std::cout << "* " << ninstr << std::endl;
  for (auto it = stat.begin(); it != stat.end(); it++) {
    std::cout << it->first << " " << it->second;
    std::cout << std::endl;
  }

  return 0;
}
