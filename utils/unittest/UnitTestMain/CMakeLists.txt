add_llvm_library(gtest_main
  TestMain.cpp

  LINK_LIBS
  gtest

  LINK_COMPONENTS
  Support # Depends on llvm::cl

  # This library is not meant to be in the install tree, only the build tree.
  BUILDTREE_ONLY
  )
set_property(TARGET gtest_main PROPERTY CXX_STANDARD 11)
set_property(TARGET gtest_main PROPERTY CXX_STANDARD_REQUIRED ON)
if (NOT LLVM_ENABLE_RTTI)
  target_compile_options(gtest_main PRIVATE -fno-rtti)
endif()
