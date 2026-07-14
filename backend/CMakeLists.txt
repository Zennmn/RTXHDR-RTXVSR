cmake_minimum_required(VERSION 3.25)
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()
if(POLICY CMP0144)
  cmake_policy(SET CMP0144 NEW)
endif()

project(rtx_video_backend VERSION 1.0.1 LANGUAGES CXX)

option(VSR_ENABLE_TESTS "Build unit tests" ON)
option(VSR_ENABLE_FFMPEG "Build FFmpeg transcode pipeline" OFF)
option(VSR_ENABLE_RTX_SDK "Build NVIDIA RTX Video SDK integration" OFF)

if(MSVC AND VSR_ENABLE_RTX_SDK)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FetchContent)

function(vsr_require_nlohmann_json)
  find_package(nlohmann_json CONFIG QUIET)
  if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
      nlohmann_json
      URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(nlohmann_json)
  endif()
endfunction()

function(vsr_require_httplib)
  find_package(httplib CONFIG QUIET)
  if(NOT httplib_FOUND)
    FetchContent_Declare(
      httplib
      GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
      GIT_TAG v0.15.3
    )
    FetchContent_MakeAvailable(httplib)
  endif()
endfunction()

function(vsr_require_gtest)
  find_package(GTest CONFIG QUIET)
  if(NOT GTest_FOUND)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()
endfunction()

add_library(vsr_backend_core STATIC src/core/build_anchor.cpp)
target_include_directories(vsr_backend_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(vsr_backend_core PUBLIC
  NOMINMAX
  WIN32_LEAN_AND_MEAN
  VSR_APP_VERSION="${PROJECT_VERSION}"
)
target_compile_features(vsr_backend_core PUBLIC cxx_std_20)

set(_vsr_core_sources
  src/core/time.cpp
  src/jobs/job_store.cpp
  src/jobs/job_runner.cpp
  src/video/fake_pipeline.cpp
  src/video/media_probe_service.cpp
  src/api/json_dto.cpp
  src/api/http_server.cpp
  src/platform/capabilities.cpp
  src/platform/logging.cpp
)

foreach(_source IN LISTS _vsr_core_sources)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_source}")
    target_sources(vsr_backend_core PRIVATE "${_source}")
  endif()
endforeach()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/api/json_dto.cpp")
  vsr_require_nlohmann_json()
  target_link_libraries(vsr_backend_core PUBLIC nlohmann_json::nlohmann_json)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/api/http_server.cpp")
  vsr_require_httplib()
  target_link_libraries(vsr_backend_core PUBLIC httplib::httplib)
endif()

if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/capabilities.cpp")
  target_link_libraries(vsr_backend_core PRIVATE d3d11)
endif()

if(VSR_ENABLE_FFMPEG)
  find_package(FFmpeg REQUIRED)
  foreach(_source IN ITEMS
      src/video/ffmpeg/ffmpeg_probe.cpp
      src/video/ffmpeg/ffmpeg_transcode_pipeline.cpp)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_source}")
      target_sources(vsr_backend_core PRIVATE "${_source}")
    endif()
  endforeach()
  target_link_libraries(vsr_backend_core PRIVATE FFmpeg::FFmpeg)
  target_compile_definitions(vsr_backend_core PUBLIC VSR_ENABLE_FFMPEG=1)
endif()

set(VSR_RTX_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../RTX_Video_SDK_v1.1.0" CACHE PATH "NVIDIA RTX Video SDK root")
if(VSR_ENABLE_RTX_SDK AND NOT WIN32)
  message(FATAL_ERROR "VSR_ENABLE_RTX_SDK requires Windows")
endif()

if(VSR_ENABLE_RTX_SDK)
  set(_vsr_rtx_dx11_source "${VSR_RTX_SDK_ROOT}/samples/RTX_Video_API/rtx_video_api_dx11_impl.cpp")
  set(_vsr_rtx_wrapper_source "${CMAKE_CURRENT_SOURCE_DIR}/src/video/rtx/rtx_dx11_processor.cpp")
  set(_vsr_rtx_ngx_release_lib "${VSR_RTX_SDK_ROOT}/lib/Windows/x64/nvsdk_ngx_s.lib")
  set(_vsr_rtx_ngx_debug_lib "${VSR_RTX_SDK_ROOT}/lib/Windows/x64/nvsdk_ngx_s_dbg.lib")
  set(_vsr_rtx_vsr_dll "${VSR_RTX_SDK_ROOT}/bin/Windows/x64/rel/nvngx_vsr.dll")
  set(_vsr_rtx_truehdr_dll "${VSR_RTX_SDK_ROOT}/bin/Windows/x64/rel/nvngx_truehdr.dll")
  if(NOT EXISTS "${_vsr_rtx_dx11_source}")
    message(FATAL_ERROR "RTX Video SDK DX11 source not found: ${_vsr_rtx_dx11_source}")
  endif()
  if(NOT EXISTS "${_vsr_rtx_wrapper_source}")
    message(FATAL_ERROR "RTX DX11 processor source not found: ${_vsr_rtx_wrapper_source}")
  endif()
  if(NOT EXISTS "${_vsr_rtx_ngx_release_lib}")
    message(FATAL_ERROR "RTX Video SDK NGX release library not found: ${_vsr_rtx_ngx_release_lib}")
  endif()
  if(NOT EXISTS "${_vsr_rtx_ngx_debug_lib}")
    message(FATAL_ERROR "RTX Video SDK NGX debug library not found: ${_vsr_rtx_ngx_debug_lib}")
  endif()
  if(NOT EXISTS "${_vsr_rtx_vsr_dll}")
    message(FATAL_ERROR "RTX Video VSR runtime DLL not found: ${_vsr_rtx_vsr_dll}")
  endif()
  if(NOT EXISTS "${_vsr_rtx_truehdr_dll}")
    message(FATAL_ERROR "RTX Video TrueHDR runtime DLL not found: ${_vsr_rtx_truehdr_dll}")
  endif()

  add_library(rtx_video_api_dx11 STATIC
    "${_vsr_rtx_dx11_source}"
  )
  target_include_directories(rtx_video_api_dx11 PUBLIC
    "${VSR_RTX_SDK_ROOT}/include"
    "${VSR_RTX_SDK_ROOT}/samples/RTX_Video_API"
  )
  target_link_libraries(rtx_video_api_dx11 PUBLIC
    d3d11
    dxgi
    "$<$<CONFIG:Debug>:${_vsr_rtx_ngx_debug_lib}>"
    "$<$<NOT:$<CONFIG:Debug>>:${_vsr_rtx_ngx_release_lib}>"
  )
  target_compile_definitions(rtx_video_api_dx11 PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)

  target_sources(vsr_backend_core PRIVATE "${_vsr_rtx_wrapper_source}")
  target_include_directories(vsr_backend_core PRIVATE "${VSR_RTX_SDK_ROOT}/samples/RTX_Video_API")
  target_link_libraries(vsr_backend_core PRIVATE rtx_video_api_dx11 d3d11 dxgi)
  target_compile_definitions(vsr_backend_core PUBLIC VSR_ENABLE_RTX_SDK=1)
endif()

add_executable(vsr_backend src/app/main.cpp)
target_link_libraries(vsr_backend PRIVATE vsr_backend_core)

if(VSR_ENABLE_FFMPEG AND WIN32 AND FFMPEG_RUNTIME_DLLS)
  add_custom_command(TARGET vsr_backend POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${FFMPEG_RUNTIME_DLLS}
      "$<TARGET_FILE_DIR:vsr_backend>"
    VERBATIM
  )
endif()

if(VSR_ENABLE_RTX_SDK)
  add_custom_command(TARGET vsr_backend POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${_vsr_rtx_vsr_dll}"
      "$<TARGET_FILE_DIR:vsr_backend>/nvngx_vsr.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${_vsr_rtx_truehdr_dll}"
      "$<TARGET_FILE_DIR:vsr_backend>/nvngx_truehdr.dll"
  )
endif()

if(VSR_ENABLE_TESTS)
  enable_testing()

  set(_vsr_test_sources
    tests/unit/bootstrap_tests.cpp
    tests/unit/job_request_tests.cpp
    tests/unit/job_store_tests.cpp
    tests/unit/job_runner_tests.cpp
    tests/unit/json_dto_tests.cpp
    tests/unit/capabilities_tests.cpp
    tests/unit/logging_tests.cpp
    tests/unit/ffmpeg_transcode_pipeline_tests.cpp
    tests/unit/media_probe_tests.cpp
    tests/unit/http_server_tests.cpp
  )
  set(_vsr_existing_test_sources "")
  foreach(_source IN LISTS _vsr_test_sources)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_source}")
      list(APPEND _vsr_existing_test_sources "${_source}")
    endif()
  endforeach()

  if(_vsr_existing_test_sources)
    vsr_require_gtest()
    add_executable(vsr_backend_tests ${_vsr_existing_test_sources})
    target_link_libraries(vsr_backend_tests PRIVATE vsr_backend_core GTest::gtest_main)
    if(VSR_ENABLE_FFMPEG)
      target_link_libraries(vsr_backend_tests PRIVATE FFmpeg::FFmpeg)
    endif()
    include(GoogleTest)
    gtest_discover_tests(vsr_backend_tests)
  endif()
endif()
