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
  )

add_llvm_tool(taffo-mlfeat
  taffo-mlfeat.cpp
  TaffoMLFeaturesAnalysis.cpp
  TaffoMLFeaturesAnalysis.h
  )
target_link_libraries(taffo-mlfeat PRIVATE
  InstructionMix
  TaffoUtils
  )

