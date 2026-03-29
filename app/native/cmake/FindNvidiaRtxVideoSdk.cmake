set(_rtx_sdk_hints
  "${NV_RTX_VIDEO_SDK}"
  "$ENV{NV_RTX_VIDEO_SDK}"
  "${CMAKE_SOURCE_DIR}/../../RTX_Video_SDK_v1.1.0"
  "${CMAKE_SOURCE_DIR}/../../deps/RTX_Video_SDK_v1.1.0")

find_path(NvidiaRtxVideoSdk_INCLUDE_DIR
  NAMES nvsdk_ngx.h
  PATH_SUFFIXES include
  PATHS ${_rtx_sdk_hints})

find_library(NvidiaRtxVideoSdk_LIBRARY
  NAMES nvsdk_ngx_s nvsdk_ngx_s_dbg nvsdk_ngx_d nvsdk_ngx_d_dbg
  PATH_SUFFIXES lib/Windows/x64
  PATHS ${_rtx_sdk_hints})

find_path(NvidiaRtxVideoSdk_SAMPLE_API_DIR
  NAMES rtx_video_api.h
  PATH_SUFFIXES samples/RTX_Video_API
  PATHS ${_rtx_sdk_hints})

find_path(NvidiaRtxVideoSdk_RUNTIME_DIR
  NAMES nvngx_truehdr.dll
  PATH_SUFFIXES bin/Windows/x64/rel bin/Windows/x64/dev
  PATHS ${_rtx_sdk_hints})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NvidiaRtxVideoSdk
  REQUIRED_VARS
    NvidiaRtxVideoSdk_INCLUDE_DIR
    NvidiaRtxVideoSdk_LIBRARY
    NvidiaRtxVideoSdk_SAMPLE_API_DIR
    NvidiaRtxVideoSdk_RUNTIME_DIR)

mark_as_advanced(
  NvidiaRtxVideoSdk_INCLUDE_DIR
  NvidiaRtxVideoSdk_LIBRARY
  NvidiaRtxVideoSdk_SAMPLE_API_DIR
  NvidiaRtxVideoSdk_RUNTIME_DIR)

