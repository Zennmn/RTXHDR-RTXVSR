set(_ffmpeg_roots "")
if(DEFINED FFMPEG_ROOT)
  list(APPEND _ffmpeg_roots "${FFMPEG_ROOT}")
endif()
if(DEFINED ENV{FFMPEG_ROOT})
  list(APPEND _ffmpeg_roots "$ENV{FFMPEG_ROOT}")
endif()

set(_ffmpeg_components avformat avcodec avutil swscale swresample)
set(_ffmpeg_runtime_required_vars "")
set(FFMPEG_RUNTIME_DLLS "")

find_path(FFMPEG_INCLUDE_DIR
  NAMES libavformat/avformat.h
  PATHS ${_ffmpeg_roots}
  PATH_SUFFIXES include
)

foreach(_component IN LISTS _ffmpeg_components)
  string(TOUPPER "${_component}" _upper)
  find_library(FFMPEG_${_upper}_LIBRARY
    NAMES ${_component}
    PATHS ${_ffmpeg_roots}
    PATH_SUFFIXES lib lib/x64 bin
  )

  if(WIN32)
    set(FFMPEG_${_upper}_DLL "")
    foreach(_root IN LISTS _ffmpeg_roots)
      file(GLOB _ffmpeg_dll_candidates
        "${_root}/bin/${_component}.dll"
        "${_root}/bin/${_component}-*.dll"
      )
      if(_ffmpeg_dll_candidates)
        list(SORT _ffmpeg_dll_candidates)
        list(GET _ffmpeg_dll_candidates 0 _ffmpeg_dll)
        set(FFMPEG_${_upper}_DLL "${_ffmpeg_dll}")
        list(APPEND FFMPEG_RUNTIME_DLLS "${_ffmpeg_dll}")
        break()
      endif()
    endforeach()
    list(APPEND _ffmpeg_runtime_required_vars FFMPEG_${_upper}_DLL)
  endif()
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
    ${_ffmpeg_runtime_required_vars}
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
