# Copyright (c) 2026 BogDan Vatra
# SPDX-License-Identifier: MIT

function(run_checked)
  execute_process(COMMAND ${ARGV} RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "FetchContent consumer failed:\n${output}\n${errors}")
  endif()
endfunction()
set(test_root "${MCPP_PRODUCER_BINARY}/fetch-content-consumer")
file(REMOVE_RECURSE "${test_root}")
foreach(flavor core beast)
  if(flavor STREQUAL "beast" AND NOT MCPP_BEAST)
    continue()
  endif()
  set(arguments)
  if(flavor STREQUAL "core")
    list(APPEND arguments -DCMAKE_DISABLE_FIND_PACKAGE_Boost=ON
      -DCMAKE_DISABLE_FIND_PACKAGE_OpenSSL=ON -DCMAKE_DISABLE_FIND_PACKAGE_Python3=ON)
  else()
    list(APPEND arguments -DMCPP_TEST_BEAST=ON "-DMCPP_TEST_TLS:BOOL=${MCPP_TLS}")
  endif()
  run_checked("${CMAKE_COMMAND}" -S "${MCPP_SOURCE_DIR}/tests/fetch_content"
    -B "${test_root}/${flavor}" -G "${MCPP_GENERATOR}"
    "-DCMAKE_CXX_COMPILER:FILEPATH=${MCPP_CXX_COMPILER}"
    "-DCMAKE_BUILD_TYPE:STRING=${MCPP_CONFIG}"
    "-DMCPP_SOURCE_DIR:PATH=${MCPP_SOURCE_DIR}"
    "-DCMAKE_PREFIX_PATH:STRING=${MCPP_PREFIX_PATH}" ${arguments})
  run_checked("${CMAKE_COMMAND}" --build "${test_root}/${flavor}" --config "${MCPP_CONFIG}" --parallel 2)
  run_checked("${CMAKE_CTEST_COMMAND}" --test-dir "${test_root}/${flavor}"
    -C "${MCPP_CONFIG}" --output-on-failure)
endforeach()
message(STATUS "FetchContent core and optional Beast consumers passed")
