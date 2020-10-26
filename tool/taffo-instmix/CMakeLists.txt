set(LLVM_LINK_COMPONENTS
  AggressiveInstCombine
  Analysis
  BitWriter
  CodeGen
  Core
  Coroutines
  IPO
  IRReader
  InstCombine
  Instrumentation
  MC
  ObjCARCOpts
  ScalarOpts
  Support
  Target
  TransformUtils
  Vectorize
  Passes
  InstructionMix
  )

add_llvm_tool(taffo-instmix
  taffo-instmix.cpp
  )
target_link_libraries(taffo-instmix PRIVATE
  InstructionMix
  )
