cmake_minimum_required(VERSION 3.4.3) # like llvm

project(Taffo)

add_subdirectory(Autotuner)
add_subdirectory(Conversion)
add_subdirectory(ErrorAnalysis)
add_subdirectory(Initializer)
#add_subdirectory(RangeAnalysis)

add_subdirectory(MDUtils)
