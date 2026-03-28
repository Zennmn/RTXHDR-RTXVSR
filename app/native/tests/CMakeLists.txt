add_executable(rtx_native_unit_tests
  unit/config_validation_tests.cpp)
target_include_directories(rtx_native_unit_tests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../core/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../video_io/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../enhance/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../pipeline/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../platform/include")
target_link_libraries(rtx_native_unit_tests PRIVATE rtx_native)

add_executable(rtx_native_integration_tests
  integration/probe_tests.cpp)
target_include_directories(rtx_native_integration_tests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../core/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../video_io/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../enhance/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../pipeline/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../platform/include")
target_link_libraries(rtx_native_integration_tests PRIVATE rtx_native)

set(_rtx_test_path
  "PATH=$<TARGET_FILE_DIR:rtx_native>;${FFMPEG_BIN_DIR};${NvidiaRtxVideoSdk_RUNTIME_DIR};$ENV{PATH}")

add_test(
  NAME rtx_native_unit_tests
  COMMAND "${CMAKE_COMMAND}" -E env "${_rtx_test_path}" $<TARGET_FILE:rtx_native_unit_tests>)
add_test(
  NAME rtx_native_integration_tests
  COMMAND "${CMAKE_COMMAND}" -E env "${_rtx_test_path}" $<TARGET_FILE:rtx_native_integration_tests>)
