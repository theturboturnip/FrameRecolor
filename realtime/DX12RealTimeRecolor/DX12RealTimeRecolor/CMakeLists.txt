# CMakeList.txt : CMake project for DX12RealTimeRecolor, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
add_executable (DX12RealTimeRecolor "DX12RealTimeRecolor.cpp" "DX12RealTimeRecolor.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET DX12RealTimeRecolor PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
