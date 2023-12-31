﻿# CMakeList.txt : CMake project for DX12RealTimeRecolor, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
find_library(AVCODEC_LIBRARY avcodec)

find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
find_library(AVFORMAT_LIBRARY avformat)

find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h)
find_library(AVUTIL_LIBRARY avutil)

find_path(AVDEVICE_INCLUDE_DIR libavdevice/avdevice.h)
find_library(AVDEVICE_LIBRARY avdevice)

# Add source to this project's executable.
add_executable (DX12RealTimeRecolor WIN32 "DX12RealTimeRecolor.cpp" "DX12RealTimeRecolor.h" "Utils/windxheaders.h")

# From https://stackoverflow.com/a/48975072/4248422
target_compile_definitions(DX12RealTimeRecolor PRIVATE "UNICODE" "_UNICODE")
target_link_libraries(DX12RealTimeRecolor PRIVATE "d3d12.lib" "dxgi.lib" "d3dcompiler.lib")
target_include_directories(DX12RealTimeRecolor PRIVATE "${DirectX-Headers_SOURCE_DIR}/include/directx")
target_include_directories(DX12RealTimeRecolor PRIVATE ${AVCODEC_INCLUDE_DIR} ${AVFORMAT_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR} ${AVDEVICE_INCLUDE_DIR})
target_link_libraries(DX12RealTimeRecolor PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${AVDEVICE_LIBRARY})

file(GLOB _ffmpeg_dlls "${CMAKE_SOURCE_DIR}/ffmpeg-master-latest-win64-gpl-shared/bin/*.dll")
add_custom_command(
    TARGET DX12RealTimeRecolor
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy ${_ffmpeg_dlls} $<TARGET_FILE_DIR:DX12RealTimeRecolor>
)

file(GLOB _shader "${CMAKE_CURRENT_SOURCE_DIR}/*.hlsl")
add_custom_command(
    TARGET DX12RealTimeRecolor
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy ${_shader} $<TARGET_FILE_DIR:DX12RealTimeRecolor>
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET DX12RealTimeRecolor PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
