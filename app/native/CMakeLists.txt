cmake_minimum_required(VERSION 3.24)

project(rtx_native
  VERSION 0.1.0
  DESCRIPTION "Native offline video enhancement core for RTXHRD+RTXVSR"
  LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
enable_testing()

include(cmake/ProjectOptions.cmake)
include(cmake/Dependencies.cmake)

set(RTX_NATIVE_PUBLIC_INCLUDE_DIRS
  "${CMAKE_CURRENT_SOURCE_DIR}/core/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/video_io/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/enhance/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/pipeline/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/platform/include"
  "${FFMPEG_INCLUDE_DIR}"
  "${NvidiaRtxVideoSdk_INCLUDE_DIR}"
  "${NvidiaRtxVideoSdk_SAMPLE_API_DIR}"
  "${VideoCodecInterface_INCLUDE_DIR}"
  "${CUDAToolkit_INCLUDE_DIRS}")

set(RTX_NATIVE_SOURCES
  core/src/logger.cpp
  core/src/validation.cpp
  video_io/src/ffmpeg_probe.cpp
  video_io/src/ffmpeg_runtime.cpp
  enhance/src/null_backend.cpp
  enhance/src/nvidia_ngx_backend.cpp
  enhance/src/sdk_cuda_sample_bridge.cpp
  pipeline/src/job_manager.cpp
  pipeline/src/processing_pipeline.cpp
  platform/src/system_probe.cpp
  platform/src/native_api.cpp)

add_library(rtx_native SHARED ${RTX_NATIVE_SOURCES})
target_include_directories(rtx_native PRIVATE ${RTX_NATIVE_PUBLIC_INCLUDE_DIRS})
target_compile_definitions(rtx_native PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_link_libraries(rtx_native
  PRIVATE
    ffmpeg_avcodec
    ffmpeg_avformat
    ffmpeg_avutil
    ffmpeg_avfilter
    ffmpeg_swscale
    ffmpeg_swresample
    "${NvidiaRtxVideoSdk_LIBRARY}"
    CUDA::cuda_driver
    CUDA::cudart)

target_compile_definitions(rtx_native PRIVATE RTX_NATIVE_VERSION="${PROJECT_VERSION}")

set_target_properties(rtx_native PROPERTIES
  OUTPUT_NAME "rtx_native"
  WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_subdirectory(tests)

install(TARGETS rtx_native
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION bin
  ARCHIVE DESTINATION lib)

install(FILES
  "${NvidiaRtxVideoSdk_RUNTIME_DIR}/nvngx_truehdr.dll"
  "${NvidiaRtxVideoSdk_RUNTIME_DIR}/nvngx_vsr.dll"
  DESTINATION bin)
