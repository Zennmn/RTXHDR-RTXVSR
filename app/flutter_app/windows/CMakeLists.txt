# Project-level configuration.
cmake_minimum_required(VERSION 3.14)
project(rtxhdr_rtxvsr LANGUAGES CXX)

# The name of the executable created for the application. Change this to change
# the on-disk name of your application.
set(BINARY_NAME "rtxhdr_rtxvsr")

# Explicitly opt in to modern CMake behaviors to avoid warnings with recent
# versions of CMake.
cmake_policy(VERSION 3.14...3.25)

# Define build configuration option.
get_property(IS_MULTICONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(IS_MULTICONFIG)
  set(CMAKE_CONFIGURATION_TYPES "Debug;Profile;Release"
    CACHE STRING "" FORCE)
else()
  if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE
      STRING "Flutter build mode" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
      "Debug" "Profile" "Release")
  endif()
endif()
# Define settings for the Profile build mode.
set(CMAKE_EXE_LINKER_FLAGS_PROFILE "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
set(CMAKE_SHARED_LINKER_FLAGS_PROFILE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")
set(CMAKE_C_FLAGS_PROFILE "${CMAKE_C_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_PROFILE "${CMAKE_CXX_FLAGS_RELEASE}")

# Use Unicode for all projects.
add_definitions(-DUNICODE -D_UNICODE)

# Compilation settings that should be applied to most targets.
#
# Be cautious about adding new options here, as plugins use this function by
# default. In most cases, you should add new options to specific targets instead
# of modifying this function.
function(APPLY_STANDARD_SETTINGS TARGET)
  target_compile_features(${TARGET} PUBLIC cxx_std_17)
  target_compile_options(${TARGET} PRIVATE /W4 /WX /wd"4100")
  target_compile_options(${TARGET} PRIVATE /EHsc)
  target_compile_definitions(${TARGET} PRIVATE "_HAS_EXCEPTIONS=0")
  target_compile_definitions(${TARGET} PRIVATE "$<$<CONFIG:Debug>:_DEBUG>")
endfunction()

# Flutter library and tool build rules.
set(FLUTTER_MANAGED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/flutter")
add_subdirectory(${FLUTTER_MANAGED_DIR})

# Application build; see runner/CMakeLists.txt.
add_subdirectory("runner")


# Generated plugin build rules, which manage building the plugins and adding
# them to the application.
include(flutter/generated_plugins.cmake)


# === Installation ===
# Support files are copied into place next to the executable, so that it can
# run in place. This is done instead of making a separate bundle (as on Linux)
# so that building and running from within Visual Studio will work.
set(BUILD_BUNDLE_DIR "$<TARGET_FILE_DIR:${BINARY_NAME}>")
# Make the "install" step default, as it's required to run.
set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD 1)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${BUILD_BUNDLE_DIR}" CACHE PATH "..." FORCE)
endif()

set(INSTALL_BUNDLE_DATA_DIR "${CMAKE_INSTALL_PREFIX}/data")
set(INSTALL_BUNDLE_LIB_DIR "${CMAKE_INSTALL_PREFIX}")
set(RTX_NATIVE_RUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../native/out/install/bin")
set(FFMPEG_BIN_DIR "$ENV{FFMPEG_ROOT}/bin")
if(NOT EXISTS "${FFMPEG_BIN_DIR}")
  set(FFMPEG_BIN_DIR "C:/Program Files (x86)/ffmpeg-N-122395-g48c9c38684-win64-gpl-shared/bin")
endif()

install(TARGETS ${BINARY_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}"
  COMPONENT Runtime)

install(FILES "${FLUTTER_ICU_DATA_FILE}" DESTINATION "${INSTALL_BUNDLE_DATA_DIR}"
  COMPONENT Runtime)

install(FILES "${FLUTTER_LIBRARY}" DESTINATION "${INSTALL_BUNDLE_LIB_DIR}"
  COMPONENT Runtime)

if(EXISTS "${RTX_NATIVE_RUNTIME_DIR}/rtx_native.dll")
  install(FILES
    "${RTX_NATIVE_RUNTIME_DIR}/rtx_native.dll"
    "${RTX_NATIVE_RUNTIME_DIR}/nvngx_truehdr.dll"
    "${RTX_NATIVE_RUNTIME_DIR}/nvngx_vsr.dll"
    DESTINATION "${INSTALL_BUNDLE_LIB_DIR}"
    COMPONENT Runtime)
endif()

foreach(FFMPEG_RUNTIME
  ffmpeg.exe
  ffprobe.exe
  avcodec-62.dll
  avformat-62.dll
  avutil-60.dll
  swresample-6.dll
  swscale-9.dll)
  if(EXISTS "${FFMPEG_BIN_DIR}/${FFMPEG_RUNTIME}")
    install(FILES "${FFMPEG_BIN_DIR}/${FFMPEG_RUNTIME}"
      DESTINATION "${INSTALL_BUNDLE_LIB_DIR}"
      COMPONENT Runtime)
  endif()
endforeach()

install(CODE "
  foreach(_runtime
    rtx_native.dll
    nvngx_truehdr.dll
    nvngx_vsr.dll)
    if(EXISTS \"${RTX_NATIVE_RUNTIME_DIR}/\${_runtime}\")
      execute_process(
        COMMAND \"${CMAKE_COMMAND}\" -E copy_if_different
          \"${RTX_NATIVE_RUNTIME_DIR}/\${_runtime}\"
          \"${INSTALL_BUNDLE_LIB_DIR}/\${_runtime}\")
    endif()
  endforeach()

  foreach(_ffmpeg_runtime
    ffmpeg.exe
    ffprobe.exe
    avcodec-62.dll
    avformat-62.dll
    avutil-60.dll
    swresample-6.dll
    swscale-9.dll)
    if(EXISTS \"${FFMPEG_BIN_DIR}/\${_ffmpeg_runtime}\")
      execute_process(
        COMMAND \"${CMAKE_COMMAND}\" -E copy_if_different
          \"${FFMPEG_BIN_DIR}/\${_ffmpeg_runtime}\"
          \"${INSTALL_BUNDLE_LIB_DIR}/\${_ffmpeg_runtime}\")
    endif()
  endforeach()
" COMPONENT Runtime)

if(PLUGIN_BUNDLED_LIBRARIES)
  install(FILES "${PLUGIN_BUNDLED_LIBRARIES}"
    DESTINATION "${INSTALL_BUNDLE_LIB_DIR}"
    COMPONENT Runtime)
endif()

# Copy the native assets provided by the build.dart from all packages.
set(NATIVE_ASSETS_DIR "${PROJECT_BUILD_DIR}native_assets/windows/")
install(DIRECTORY "${NATIVE_ASSETS_DIR}"
   DESTINATION "${INSTALL_BUNDLE_LIB_DIR}"
   COMPONENT Runtime)

# Fully re-copy the assets directory on each build to avoid having stale files
# from a previous install.
set(FLUTTER_ASSET_DIR_NAME "flutter_assets")
install(CODE "
  file(REMOVE_RECURSE \"${INSTALL_BUNDLE_DATA_DIR}/${FLUTTER_ASSET_DIR_NAME}\")
  " COMPONENT Runtime)
install(DIRECTORY "${PROJECT_BUILD_DIR}/${FLUTTER_ASSET_DIR_NAME}"
  DESTINATION "${INSTALL_BUNDLE_DATA_DIR}" COMPONENT Runtime)

# Install the AOT library on non-Debug builds only.
install(FILES "${AOT_LIBRARY}" DESTINATION "${INSTALL_BUNDLE_DATA_DIR}"
  CONFIGURATIONS Profile;Release
  COMPONENT Runtime)
