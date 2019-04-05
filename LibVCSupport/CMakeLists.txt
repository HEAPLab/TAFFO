add_subdirectory(LibVC)

add_library(TAFFOCompiler STATIC
  TAFFOCompiler/TAFFOCompiler.cpp
  TAFFOCompiler/TAFFOCompiler.hpp)
set_property(TARGET TAFFOCompiler PROPERTY CXX_STANDARD 11)
set_property(TARGET TAFFOCompiler PROPERTY CXX_STANDARD_REQUIRED ON)
target_include_directories(TAFFOCompiler PRIVATE LibVC/include)
target_include_directories(TAFFOCompiler PUBLIC TAFFOCompiler/)

