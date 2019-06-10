project(TAFFO-Tools)

find_package(LLVM REQUIRED CONFIG)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS}
    ../TaffoUtils)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs support core irreader)

add_executable(istr_type
  istr_type.cpp
)
target_link_libraries(istr_type ${llvm_libs})
set_property(TARGET istr_type PROPERTY CXX_STANDARD 11)
set_property(TARGET istr_type PROPERTY CXX_STANDARD_REQUIRED ON)
if (NOT LLVM_ENABLE_RTTI)
  target_compile_options(istr_type PRIVATE -fno-rtti)
endif()
install(TARGETS istr_type DESTINATION bin)

