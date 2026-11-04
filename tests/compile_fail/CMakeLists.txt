# Copyright (c) 2026 BogDan Vatra
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.30)
project(mcpp_compile_fail LANGUAGES CXX)

get_filename_component(MCPP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
# These tests check compilation, without requiring an entry point or linking.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

function(compile_source source)
  try_compile(compiled
    SOURCES "${source}"
    CMAKE_FLAGS "-DINCLUDE_DIRECTORIES:STRING=${MCPP_SOURCE_DIR}/include"
    COMPILE_DEFINITIONS -freflection
    CXX_STANDARD 26
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    NO_CACHE
    OUTPUT_VARIABLE compiler_output)
  set(compiled "${compiled}" PARENT_SCOPE)
  set(compiler_output "${compiler_output}" PARENT_SCOPE)
endfunction()

# Reject an unusable toolchain/include setup before interpreting any failures.
compile_source("${MCPP_SOURCE_DIR}/tests/reflection_smoke.cpp")
if(NOT compiled)
  message(FATAL_ERROR "Reflection baseline failed to compile:\n${compiler_output}")
endif()

function(expect_compile_failure case diagnostic)
  compile_source("${CMAKE_CURRENT_SOURCE_DIR}/${case}.cpp")
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${case}.log" "${compiler_output}")
  if(compiled)
    message(FATAL_ERROR "${case}: expected compilation to fail")
  endif()
  string(FIND "${compiler_output}" "${diagnostic}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "${case}: expected diagnostic '${diagnostic}':\n${compiler_output}")
  endif()
  message(STATUS "${case}: rejected with expected diagnostic")
endfunction()

expect_compile_failure(unsupported "unsupported parameter type")
expect_compile_failure(unnamed "every tool parameter must have a name")
expect_compile_failure(overload "duplicate tool name")
expect_compile_failure(static "static tools are unsupported")
expect_compile_failure(qualified "ref-qualified and volatile tools are unsupported")
expect_compile_failure(template "tool must be an ordinary member function")
expect_compile_failure(return_view "unsupported return type")
expect_compile_failure(reference "unsupported parameter type")
expect_compile_failure(variadic "variadic tools are unsupported")
expect_compile_failure(explicit_object "explicit object parameters are unsupported")
