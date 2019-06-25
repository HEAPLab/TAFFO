project(TAFFO-Tools)

find_package(LLVM REQUIRED CONFIG)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS}
    ../TaffoUtils)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs
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
  Passes)

add_executable(taffo-instmix
  taffo-instmix.cpp
  InstructionMix.h
  InstructionMix.cpp
)
target_link_libraries(taffo-instmix ${llvm_libs})
set_property(TARGET taffo-instmix PROPERTY CXX_STANDARD 11)
set_property(TARGET taffo-instmix PROPERTY CXX_STANDARD_REQUIRED ON)
if (NOT LLVM_ENABLE_RTTI)
  target_compile_options(taffo-instmix PRIVATE -fno-rtti)
endif()
install(TARGETS taffo-instmix DESTINATION bin)

add_executable(taffo-mlfeat
  taffo-mlfeat.cpp
  InstructionMix.h
  InstructionMix.cpp
  TaffoMLFeaturesAnalysis.cpp
  TaffoMLFeaturesAnalysis.h
)
target_link_libraries(taffo-mlfeat ${llvm_libs} TaffoUtils)
set_property(TARGET taffo-mlfeat PROPERTY CXX_STANDARD 11)
set_property(TARGET taffo-mlfeat PROPERTY CXX_STANDARD_REQUIRED ON)
if (NOT LLVM_ENABLE_RTTI)
  target_compile_options(taffo-mlfeat PRIVATE -fno-rtti)
endif()
install(TARGETS taffo-mlfeat DESTINATION bin)

add_subdirectory(taffo-j2a)