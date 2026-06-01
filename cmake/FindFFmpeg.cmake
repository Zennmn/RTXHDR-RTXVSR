set(_ffmpeg_roots "")
if(DEFINED FFMPEG_ROOT)
  list(APPEND _ffmpeg_roots "${FFMPEG_ROOT}")
endif()
if(DEFINED ENV{FFMPEG_ROOT})
  list(APPEND _ffmpeg_roots "$ENV{FFMPEG_ROOT}")
endif()

find_path(FFMPEG_INCLUDE_DIR
  NAMES libavformat/avformat.h
  PATHS ${_ffmpeg_roots}
  PATH_SUFFIXES include
)

foreach(_component avformat avcodec avutil swscale swresample)
  string(TOUPPER "${_component}" _upper)
  find_library(FFMPEG_${_upper}_LIBRARY
    NAMES ${_component}
    PATHS ${_ffmpeg_roots}
    PATH_SUFFIXES lib lib/x64 bin
  )
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFmpeg
  REQUIRED_VARS
    FFMPEG_INCLUDE_DIR
    FFMPEG_AVFORMAT_LIBRARY
    FFMPEG_AVCODEC_LIBRARY
    FFMPEG_AVUTIL_LIBRARY
    FFMPEG_SWSCALE_LIBRARY
    FFMPEG_SWRESAMPLE_LIBRARY
)

if(FFmpeg_FOUND AND NOT TARGET FFmpeg::FFmpeg)
  add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
  target_include_directories(FFmpeg::FFmpeg INTERFACE "${FFMPEG_INCLUDE_DIR}")
  target_link_libraries(FFmpeg::FFmpeg INTERFACE
    "${FFMPEG_AVFORMAT_LIBRARY}"
    "${FFMPEG_AVCODEC_LIBRARY}"
    "${FFMPEG_AVUTIL_LIBRARY}"
    "${FFMPEG_SWSCALE_LIBRARY}"
    "${FFMPEG_SWRESAMPLE_LIBRARY}"
  )
endif()
