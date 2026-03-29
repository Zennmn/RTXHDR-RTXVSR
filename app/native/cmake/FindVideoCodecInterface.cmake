set(_vci_hints
  "${NV_VIDEO_CODEC_INTERFACE}"
  "$ENV{NV_VIDEO_CODEC_INTERFACE}"
  "${CMAKE_SOURCE_DIR}/../../Video_Codec_Interface_13.0.37"
  "${CMAKE_SOURCE_DIR}/../../deps/Video_Codec_Interface_13.0.37")

find_path(VideoCodecInterface_INCLUDE_DIR
  NAMES nvEncodeAPI.h
  PATH_SUFFIXES Interface
  PATHS ${_vci_hints})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VideoCodecInterface
  REQUIRED_VARS VideoCodecInterface_INCLUDE_DIR)

mark_as_advanced(VideoCodecInterface_INCLUDE_DIR)

