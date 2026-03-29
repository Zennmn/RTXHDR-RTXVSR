include(cmake/FindNvidiaRtxVideoSdk.cmake)
include(cmake/FindVideoCodecInterface.cmake)

set(_ffmpeg_hints
  "${FFMPEG_ROOT}"
  "$ENV{FFMPEG_ROOT}"
  "${CMAKE_SOURCE_DIR}/../../deps/ffmpeg"
  "C:/Program Files (x86)/ffmpeg-N-122395-g48c9c38684-win64-gpl-shared")

find_path(FFMPEG_INCLUDE_DIR
  NAMES libavformat/avformat.h
  PATH_SUFFIXES include
  PATHS ${_ffmpeg_hints})

find_library(FFMPEG_AVCODEC_LIBRARY NAMES avcodec PATH_SUFFIXES lib PATHS ${_ffmpeg_hints})
find_library(FFMPEG_AVFORMAT_LIBRARY NAMES avformat PATH_SUFFIXES lib PATHS ${_ffmpeg_hints})
find_library(FFMPEG_AVUTIL_LIBRARY NAMES avutil PATH_SUFFIXES lib PATHS ${_ffmpeg_hints})
find_library(FFMPEG_AVFILTER_LIBRARY NAMES avfilter PATH_SUFFIXES lib PATHS ${_ffmpeg_hints})
find_library(FFMPEG_SWSCALE_LIBRARY NAMES swscale PATH_SUFFIXES lib PATHS ${_ffmpeg_hints})
find_library(FFMPEG_SWRESAMPLE_LIBRARY NAMES swresample PATH_SUFFIXES lib PATHS ${_ffmpeg_hints})
find_path(FFMPEG_BIN_DIR NAMES ffmpeg.exe ffprobe.exe PATH_SUFFIXES bin PATHS ${_ffmpeg_hints})
find_file(FFMPEG_AVCODEC_DLL NAMES avcodec-62.dll PATH_SUFFIXES bin PATHS ${_ffmpeg_hints})
find_file(FFMPEG_AVFORMAT_DLL NAMES avformat-62.dll PATH_SUFFIXES bin PATHS ${_ffmpeg_hints})
find_file(FFMPEG_AVUTIL_DLL NAMES avutil-60.dll PATH_SUFFIXES bin PATHS ${_ffmpeg_hints})
find_file(FFMPEG_AVFILTER_DLL NAMES avfilter-11.dll PATH_SUFFIXES bin PATHS ${_ffmpeg_hints})
find_file(FFMPEG_SWSCALE_DLL NAMES swscale-9.dll PATH_SUFFIXES bin PATHS ${_ffmpeg_hints})
find_file(FFMPEG_SWRESAMPLE_DLL NAMES swresample-6.dll PATH_SUFFIXES bin PATHS ${_ffmpeg_hints})

find_package(CUDAToolkit REQUIRED)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFMPEG
  REQUIRED_VARS
    FFMPEG_INCLUDE_DIR
    FFMPEG_AVCODEC_LIBRARY
    FFMPEG_AVFORMAT_LIBRARY
    FFMPEG_AVUTIL_LIBRARY
    FFMPEG_AVFILTER_LIBRARY
    FFMPEG_SWSCALE_LIBRARY
    FFMPEG_SWRESAMPLE_LIBRARY
    FFMPEG_BIN_DIR)

if(FFMPEG_FOUND)
  add_library(ffmpeg_avcodec SHARED IMPORTED GLOBAL)
  set_target_properties(ffmpeg_avcodec PROPERTIES
    IMPORTED_IMPLIB "${FFMPEG_AVCODEC_LIBRARY}"
    IMPORTED_LOCATION "${FFMPEG_AVCODEC_DLL}")
  add_library(ffmpeg_avformat SHARED IMPORTED GLOBAL)
  set_target_properties(ffmpeg_avformat PROPERTIES
    IMPORTED_IMPLIB "${FFMPEG_AVFORMAT_LIBRARY}"
    IMPORTED_LOCATION "${FFMPEG_AVFORMAT_DLL}")
  add_library(ffmpeg_avutil SHARED IMPORTED GLOBAL)
  set_target_properties(ffmpeg_avutil PROPERTIES
    IMPORTED_IMPLIB "${FFMPEG_AVUTIL_LIBRARY}"
    IMPORTED_LOCATION "${FFMPEG_AVUTIL_DLL}")
  add_library(ffmpeg_avfilter SHARED IMPORTED GLOBAL)
  set_target_properties(ffmpeg_avfilter PROPERTIES
    IMPORTED_IMPLIB "${FFMPEG_AVFILTER_LIBRARY}"
    IMPORTED_LOCATION "${FFMPEG_AVFILTER_DLL}")
  add_library(ffmpeg_swscale SHARED IMPORTED GLOBAL)
  set_target_properties(ffmpeg_swscale PROPERTIES
    IMPORTED_IMPLIB "${FFMPEG_SWSCALE_LIBRARY}"
    IMPORTED_LOCATION "${FFMPEG_SWSCALE_DLL}")
  add_library(ffmpeg_swresample SHARED IMPORTED GLOBAL)
  set_target_properties(ffmpeg_swresample PROPERTIES
    IMPORTED_IMPLIB "${FFMPEG_SWRESAMPLE_LIBRARY}"
    IMPORTED_LOCATION "${FFMPEG_SWRESAMPLE_DLL}")
endif()
