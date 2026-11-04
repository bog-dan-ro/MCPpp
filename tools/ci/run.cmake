# Copyright (c) 2026 BogDan Vatra
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.30)
get_filename_component(source "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
find_program(python NAMES python3 python REQUIRED)
find_program(inspector NAMES mcp-inspector REQUIRED)
find_program(conformance NAMES conformance REQUIRED)
if(NOT DEFINED MCPP_CI_PROFILES)
  set(MCPP_CI_PROFILES verified sanitizers http-only core-only)
endif()
function(run_checked)
  execute_process(COMMAND ${ARGV} RESULT_VARIABLE result COMMAND_ECHO STDOUT)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "CI command failed with exit code ${result}")
  endif()
endfunction()
foreach(profile IN LISTS MCPP_CI_PROFILES)
  set(arguments)
  if(profile STREQUAL "verified")
    list(APPEND arguments -DMCPP_WITH_BEAST=ON -DMCPP_WITH_NLOHMANN=ON
      -DMCPP_VALIDATE_PROTOCOL=ON -DMCPP_EXTERNAL_CHECKS=ON)
  elseif(profile STREQUAL "sanitizers")
    list(APPEND arguments -DMCPP_WITH_BEAST=ON -DMCPP_SANITIZERS=ON -DMCPP_VALIDATE_PROTOCOL=ON)
  elseif(profile STREQUAL "http-only")
    list(APPEND arguments -DMCPP_WITH_BEAST=ON -DMCPP_WITH_BEAST_TLS=OFF)
  elseif(profile STREQUAL "core-only")
    # Keep the native package and embedding tests, but require no JSON backend
    # or transport libraries when configuring the producer.
    list(APPEND arguments -DMCPP_WITH_BOOST=OFF -DMCPP_WITH_NLOHMANN=OFF -DMCPP_WITH_BEAST=OFF
      -DCMAKE_DISABLE_FIND_PACKAGE_Boost=ON -DCMAKE_DISABLE_FIND_PACKAGE_OpenSSL=ON)
  else()
    message(FATAL_ERROR "Unknown CI profile '${profile}'")
  endif()
  set(binary "${source}/build-ci-${profile}")
  run_checked("${CMAKE_COMMAND}" -S "${source}" -B "${binary}" -G Ninja
    -DCMAKE_CXX_COMPILER=g++-16 -DCMAKE_BUILD_TYPE=Debug
    "-DPython3_EXECUTABLE:FILEPATH=${python}"
    "-DMCPP_INSPECTOR_EXECUTABLE:FILEPATH=${inspector}"
    "-DMCPP_CONFORMANCE_EXECUTABLE:FILEPATH=${conformance}" ${arguments})
  run_checked("${CMAKE_COMMAND}" --build "${binary}" --parallel 2)
  if(profile STREQUAL "sanitizers")
    run_checked("${CMAKE_COMMAND}" -E env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
      "${CMAKE_CTEST_COMMAND}" --test-dir "${binary}" --output-on-failure --parallel 2)
  else()
    run_checked("${CMAKE_CTEST_COMMAND}" --test-dir "${binary}" --output-on-failure --parallel 2)
  endif()
endforeach()
