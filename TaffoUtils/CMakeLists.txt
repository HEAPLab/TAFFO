find_package(LLVM REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})

add_library(TaffoUtils
  Metadata.h
  Metadata.cpp
  InputInfo.h
  InputInfo.cpp
  TypeUtils.h
  TypeUtils.cpp
  MultiValueMap.h
  handle.h
)

set_property(TARGET TaffoUtils PROPERTY CXX_STANDARD 11)
set_property(TARGET TaffoUtils PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET TaffoUtils PROPERTY POSITION_INDEPENDENT_CODE ON)
