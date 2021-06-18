cmake_minimum_required(VERSION 3.12) # for FindPython

project(Taffo)

find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
cmake_policy(SET CMP0077 NEW)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include_directories(${LLVM_INCLUDE_DIRS})
include(AddLLVM)
include(HandleLLVMOptions)

find_package(ortools CONFIG REQUIRED)
# to be kept in sync with LLVM!
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_CLANG_TIDY "clang-tidy-10;")
#add_compile_options(-Wall -Wextra)

# build & install all tools by default
set(LLVM_BUILD_TOOLS ON)

# fix add_llvm_tool on macOS+Xcode
set(LLVM_MAIN_SRC_DIR ${CMAKE_SOURCE_DIR})

add_subdirectory(lib)
add_subdirectory(utils)
#add_subdirectory(unittests)
add_subdirectory(tool)

target_link_libraries(Taffo PRIVATE ortools::ortools)