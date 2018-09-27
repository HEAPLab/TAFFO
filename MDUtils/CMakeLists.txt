add_library(MDUtils
  Metadata.cpp
  InputInfo.cpp
  Metadata.h
  InputInfo.h
)

set_property(TARGET MDUtils PROPERTY CXX_STANDARD 11)
set_property(TARGET MDUtils PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET MDUtils PROPERTY POSITION_INDEPENDENT_CODE ON)
