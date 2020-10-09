cmake_minimum_required(VERSION 3.4.3) # like llvm

project(Taffo)

find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include_directories(${LLVM_INCLUDE_DIRS}
  TaffoUtils)
include(AddLLVM)
include(HandleLLVMOptions)

# to be kept in sync with LLVM!
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(Initializer)
add_subdirectory(RangeAnalysis)
add_subdirectory(DataTypeAlloc)
add_subdirectory(Conversion)
add_subdirectory(ErrorAnalysis)

add_subdirectory(TaffoUtils)

add_llvm_library(Taffo MODULE
  $<TARGET_OBJECTS:obj.TaffoInitializer>
  $<TARGET_OBJECTS:obj.TaffoVRA>
  $<TARGET_OBJECTS:obj.TaffoDTA>
  $<TARGET_OBJECTS:obj.LLVMFloatToFixed>
  $<TARGET_OBJECTS:obj.LLVMErrorPropagator>

  LINK_LIBS
  TaffoUtils
  m
  stdc++

  PLUGIN_TOOL
  opt
)

add_subdirectory(utils)
add_subdirectory(unittests)
add_subdirectory(tool)
