# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("DX12RealTimeRecolor")

# Include sub-projects.
add_subdirectory ("DirectX-Headers")
add_subdirectory ("DX12RealTimeRecolor")
