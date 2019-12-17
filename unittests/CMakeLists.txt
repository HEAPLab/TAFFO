find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(
    ${LLVM_INCLUDE_DIRS}
    ../TaffoUtils)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

llvm_map_components_to_libnames(llvm_libs
  #
  )

# this function is mostly copied from AddLLVM.cmake because it assumes that the
# entire LLVM source is available, even though in our case it might not 
function(taffo_add_unittest test_suite test_name)
  if( NOT TAFFO_BUILD_TESTS )
    set(EXCLUDE_FROM_ALL ON)
  endif()

  # Our current version of gtest does not properly recognize C++11 support
  # with MSVC, so it falls back to tr1 / experimental classes.  Since LLVM
  # itself requires C++11, we can safely force it on unconditionally so that
  # we don't have to fight with the buggy gtest check.
  add_definitions(-DGTEST_LANG_CXX11=1)
  add_definitions(-DGTEST_HAS_TR1_TUPLE=0)

  include_directories(../utils/unittest/googletest/include)
  include_directories(../utils/unittest/googlemock/include)
  if (NOT LLVM_ENABLE_THREADS)
    list(APPEND LLVM_COMPILE_DEFINITIONS GTEST_HAS_PTHREAD=0)
  endif ()

  if (SUPPORTS_VARIADIC_MACROS_FLAG)
    list(APPEND LLVM_COMPILE_FLAGS "-Wno-variadic-macros")
  endif ()
  # Some parts of gtest rely on this GNU extension, don't warn on it.
  if(SUPPORTS_GNU_ZERO_VARIADIC_MACRO_ARGUMENTS_FLAG)
    list(APPEND LLVM_COMPILE_FLAGS "-Wno-gnu-zero-variadic-macro-arguments")
  endif()

  set(LLVM_REQUIRES_RTTI OFF)

  list(APPEND LLVM_LINK_COMPONENTS Support) # gtest needs it for raw_ostream
  add_executable(${test_name} ${ARGN})
  set_property(TARGET ${test_name} PROPERTY CXX_STANDARD 11)
  set_property(TARGET ${test_name} PROPERTY CXX_STANDARD_REQUIRED ON)
  set(outdir ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR})
  set_output_directory(${test_name} BINARY_DIR ${outdir} LIBRARY_DIR ${outdir})
  # libpthreads overrides some standard library symbols, so main
  # executable must be linked with it in order to provide consistent
  # API for all shared libaries loaded by this executable.
  target_link_libraries(${test_name} PRIVATE ${llvm_libs} gtest_main gtest ${LLVM_PTHREAD_LIB})

  add_dependencies(${test_suite} ${test_name})
  get_target_property(test_suite_folder ${test_suite} FOLDER)
  if (NOT ${test_suite_folder} STREQUAL "NOTFOUND")
    set_property(TARGET ${test_name} PROPERTY FOLDER "${test_suite_folder}")
  endif ()
endfunction()

function(add_taffo_unittest test_dirname)
  taffo_add_unittest(TAFFOUnitTests ${test_dirname} ${ARGN})
endfunction()

add_taffo_unittest(TAFFOUnitTests MultiValueMapTest.cpp)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set_property(TARGET TAFFOUnitTests APPEND_STRING PROPERTY
    LINK_FLAGS " -undefined dynamic_lookup")
endif()

set_property(TARGET TAFFOUnitTests PROPERTY CXX_STANDARD 11)
set_property(TARGET TAFFOUnitTests PROPERTY CXX_STANDARD_REQUIRED ON)

