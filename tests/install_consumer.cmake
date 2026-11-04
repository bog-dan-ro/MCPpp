# Copyright (c) 2026 BogDan Vatra
# SPDX-License-Identifier: MIT

function(run_checked)
  execute_process(COMMAND ${ARGV} RESULT_VARIABLE result
    OUTPUT_VARIABLE output ERROR_VARIABLE errors)
  if(NOT result EQUAL 0)
    string(JOIN " " command ${ARGV})
    message(FATAL_ERROR "${command}\n${output}\n${errors}")
  endif()
endfunction()

set(test_root "${MCPP_PRODUCER_BINARY}/installed-consumer")
file(REMOVE_RECURSE "${test_root}")
set(original "${test_root}/original-prefix")
set(relocated "${test_root}/relocated-prefix")
run_checked("${CMAKE_COMMAND}" --install "${MCPP_PRODUCER_BINARY}"
  --prefix "${original}" --config "${MCPP_CONFIG}")
# Importing from a moved prefix must work without the original install location.
file(RENAME "${original}" "${relocated}")
if(NOT EXISTS "${relocated}/${MCPP_INSTALL_DATADIR}/licenses/MCPpp/LICENSE")
  message(FATAL_ERROR "The installed package is missing its license")
endif()
file(GLOB_RECURSE installed_files "${relocated}/*")
foreach(path IN LISTS installed_files)
  if(path MATCHES "\\.(a|so|lib|dll|dylib)$")
    message(FATAL_ERROR "MCPpp must install headers and CMake metadata only: ${path}")
  endif()
endforeach()
set(prefixes "${relocated};${MCPP_PREFIX_PATH}")
run_checked("${CMAKE_COMMAND}" -S "${MCPP_CONSUMER_SOURCE}" -B "${test_root}/core"
  -G "${MCPP_GENERATOR}" "-DCMAKE_CXX_COMPILER:FILEPATH=${MCPP_CXX_COMPILER}"
  "-DCMAKE_PREFIX_PATH:STRING=${prefixes}" "-DCMAKE_BUILD_TYPE:STRING=${MCPP_CONFIG}"
  -DCMAKE_DISABLE_FIND_PACKAGE_Boost=ON -DCMAKE_DISABLE_FIND_PACKAGE_OpenSSL=ON
  -DMCPP_TEST_BEAST=OFF)
run_checked("${CMAKE_COMMAND}" --build "${test_root}/core" --config "${MCPP_CONFIG}" --parallel 2)
run_checked("${CMAKE_CTEST_COMMAND}" --test-dir "${test_root}/core" -C "${MCPP_CONFIG}" --output-on-failure)
if(MCPP_BEAST)
  run_checked("${CMAKE_COMMAND}" -S "${MCPP_CONSUMER_SOURCE}" -B "${test_root}/beast"
    -G "${MCPP_GENERATOR}" "-DCMAKE_CXX_COMPILER:FILEPATH=${MCPP_CXX_COMPILER}"
    "-DCMAKE_PREFIX_PATH:STRING=${prefixes}" "-DCMAKE_BUILD_TYPE:STRING=${MCPP_CONFIG}"
    -DMCPP_TEST_BEAST=ON)
  run_checked("${CMAKE_COMMAND}" --build "${test_root}/beast" --config "${MCPP_CONFIG}" --parallel 2)
  run_checked("${CMAKE_CTEST_COMMAND}" --test-dir "${test_root}/beast" -C "${MCPP_CONFIG}" --output-on-failure)
endif()
message(STATUS "Installed core and optional Beast consumers passed from a relocated prefix")
