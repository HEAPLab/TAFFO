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

set_property(TARGET TaffoUtils PROPERTY POSITION_INDEPENDENT_CODE ON)
