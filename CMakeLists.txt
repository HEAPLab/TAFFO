cmake_minimum_required(VERSION 3.4.3) # like llvm

project(Taffo)

add_subdirectory(DataTypeAlloc)
add_subdirectory(Conversion)
add_subdirectory(ErrorAnalysis)
add_subdirectory(Initializer)
add_subdirectory(RangeAnalysis)
add_custom_target(taffo)
add_dependencies(taffo LLVMFloatToFixed TaffoDTA LLVMErrorPropagator TaffoInitializer TaffoVRA)

add_subdirectory(TaffoUtils)
add_subdirectory(utils)
add_subdirectory(unittests)
add_subdirectory(tool)
