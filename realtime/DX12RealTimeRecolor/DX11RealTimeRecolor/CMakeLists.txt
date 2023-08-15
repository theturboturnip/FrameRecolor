# CMakeList.txt : CMake project for DX11RealTimeRecolor, include source and define
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
set(HLSL_SHADER_FILES "posuv_vert.hlsl" "yuv2RGB_frag.hlsl" "posuv_vert.hlsl" "yuv2RGB_frag.hlsl" "yuv2RGB_comp.hlsl" "rgb_frag.hlsl")
add_executable (DX11RealTimeRecolor WIN32 "DX11RealTimeRecolor.cpp" "DX11RealTimeRecolor.h" "Utils/windxheaders.h" ${HLSL_SHADER_FILES})

# Build HLSL shaders
add_custom_target(shaders)

set_source_files_properties("posuv_vert.hlsl" PROPERTIES ShaderType "vs")
set_source_files_properties("rgb_frag.hlsl" PROPERTIES ShaderType "ps")
set_source_files_properties("yuv2RGB_frag.hlsl" PROPERTIES ShaderType "ps")
set_source_files_properties("yuv2RGB_comp.hlsl" PROPERTIES ShaderType "cs")
set_source_files_properties(${HLSL_SHADER_FILES} PROPERTIES ShaderModel "5_0")

foreach(FILE ${HLSL_SHADER_FILES})
  get_filename_component(FILE_WE ${FILE} NAME_WE)
  get_source_file_property(shadertype ${FILE} ShaderType)
  get_source_file_property(shadermodel ${FILE} ShaderModel)
  add_custom_command(TARGET shaders
                     COMMAND fxc.exe /nologo /Emain /T${shadertype}_${shadermodel} $<IF:$<CONFIG:DEBUG>,/Od,/O1> /Zi /Fo ${CMAKE_BINARY_DIR}/DX11RealTimeRecolor/${FILE_WE}.cso ${FILE}
                     MAIN_DEPENDENCY ${FILE}
                     COMMENT "HLSL ${FILE}"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                     DEPENDS ${FILE}
                     VERBATIM)
endforeach(FILE)

add_dependencies(DX11RealTimeRecolor shaders)

# From https://stackoverflow.com/a/48975072/4248422
target_compile_definitions(DX11RealTimeRecolor PRIVATE "UNICODE" "_UNICODE")
target_link_libraries(DX11RealTimeRecolor PRIVATE "d3d11.lib" "dxgi.lib" "d3dcompiler.lib")
target_include_directories(DX11RealTimeRecolor PRIVATE ${AVCODEC_INCLUDE_DIR} ${AVFORMAT_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR} ${AVDEVICE_INCLUDE_DIR})
target_link_libraries(DX11RealTimeRecolor PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${AVDEVICE_LIBRARY})

file(GLOB _ffmpeg_dlls "${CMAKE_SOURCE_DIR}/ffmpeg-master-latest-win64-gpl-shared/bin/*.dll")
add_custom_command(
    TARGET DX11RealTimeRecolor
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy ${_ffmpeg_dlls} $<TARGET_FILE_DIR:DX11RealTimeRecolor>
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET DX11RealTimeRecolor PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
