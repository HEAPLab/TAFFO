
#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <memory>
#include <string>
#include "llvm/Demangle/Demangle.h"




#ifndef TAFFO_HANDLED_FUNCTION
#define TAFFO_HANDLED_FUNCTION

using namespace llvm;

extern bool EnableMathFunctionsConversionsFlag;
extern bool EnableMathFunctionSinFlag;
extern bool EnableMathFunctionCosFlag; 

namespace taffo {

template <typename T>
inline bool start_with(const std::string &name, T &&prefix) noexcept {
  return name.find(std::forward<T>(prefix)) == 0;
}

class HandledFunction {

public:
  /**Check if a function is handled
   * @param f the function to check*/
  static bool isHandled(const llvm::Function *f) {
    llvm::StringRef old_fName = f->getName();
    std::string fName(demangle(old_fName.str()));  
    llvm::dbgs() << "Demangle " << fName << "\n";
    
    auto handled = getHandledFunction();
    auto &list_of_handled = handled->handledFunction;
    // check if function name is present
    auto founded =
        std::find_if(std::begin(list_of_handled), std::end(list_of_handled),
                     [&fName](const std::string &toComp) {
                       return fName.find(toComp) == 0;
                     }) != std::end(list_of_handled);
    llvm::dbgs() << "Founded " << founded << "\n";
    return founded;
  }

  static const llvm::SmallVector<std::string, 3U> &handledFunctions() {
    auto handled = getHandledFunction();
    return handled->handledFunction;
  }


   static std::string demangle(const std::string &MangledName) {
   char *Demangled;
   if (isItaniumEncoding(MangledName))
     Demangled = llvm::itaniumDemangle(MangledName.c_str(), nullptr, nullptr, nullptr);
   else
    return MangledName;
 
   if (!Demangled)
     return MangledName;
 
   std::string Ret = Demangled;
   std::free(Demangled);
   return Ret;
 }

private:
  /**Constructor the list of handled function
   */
  const bool enabled(bool b){ return b || EnableMathFunctionsConversionsFlag;
  }
   static bool isItaniumEncoding(const std::string &MangledName) {
   size_t Pos = MangledName.find_first_not_of('_');
   // A valid Itanium encoding requires 1-4 leading underscores, followed by 'Z'.
   return Pos > 0 && Pos <= 4 && MangledName[Pos] == 'Z';
 }


  HandledFunction() {
    if(enabled(EnableMathFunctionSinFlag)) handledFunction.emplace_back("sin");
    if(enabled(EnableMathFunctionCosFlag)) handledFunction.emplace_back("cos");
    if(enabled(EnableMathFunctionSinFlag)) handledFunction.emplace_back("_ZSt3sin");
    if(enabled(EnableMathFunctionCosFlag)) handledFunction.emplace_back("_ZSt3cos");
    if(enabled(EnableMathFunctionCosFlag)) handledFunction.emplace_back("abs");
  }

  /*get an instance of HandledFunction
   * singleton implementation
   * used only to check what special function are supported*/
  static HandledFunction *getHandledFunction() {
    static HandledFunction *instance;
    if (instance != nullptr) {
      return instance;
    }

    instance = new HandledFunction();
    return instance;
  }

  llvm::SmallVector<std::string, 3> handledFunction;
};

} // namespace taffo

#endif
