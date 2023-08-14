# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("DX12RealTimeRecolor")

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/ffmpeg-master-latest-win64-gpl-shared/include" "${CMAKE_SOURCE_DIR}/ffmpeg-master-latest-win64-gpl-shared/lib")

# Include sub-projects.
add_subdirectory ("DirectX-Headers")
add_subdirectory ("DX11RealTimeRecolor")
add_subdirectory ("DX12RealTimeRecolor")