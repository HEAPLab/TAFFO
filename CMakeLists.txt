cmake_minimum_required(VERSION 3.12) # for FindPython

project(Taffo)

find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include_directories(${LLVM_INCLUDE_DIRS})
include(AddLLVM)
include(HandleLLVMOptions)

# to be kept in sync with LLVM!
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# build & install all tools by default
set(LLVM_BUILD_TOOLS ON)

add_subdirectory(lib)
add_subdirectory(utils)
add_subdirectory(unittests)
add_subdirectory(tool)
