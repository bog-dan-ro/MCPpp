# Copyright (c) 2026 BogDan Vatra
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.30)
project(mcpp_installed_consumer LANGUAGES CXX)
enable_testing()
option(MCPP_TEST_BEAST "Exercise the installed Beast component" OFF)

find_package(MCPpp 0.1 CONFIG REQUIRED COMPONENTS core)
get_target_property(core_type mcp::mcpp TYPE)
if(NOT core_type STREQUAL "INTERFACE_LIBRARY")
  message(FATAL_ERROR "The core must be header-only")
endif()
add_executable(installed_core core.cpp)
target_link_libraries(installed_core PRIVATE mcp::mcpp)
add_test(NAME core COMMAND installed_core)

if(MCPP_TEST_BEAST)
  # Discovering the optional component in a later find_package call must work.
  find_package(MCPpp 0.1 CONFIG REQUIRED COMPONENTS beast)
  find_package(Boost 1.92.0 EXACT REQUIRED COMPONENTS json)
  get_target_property(beast_type mcp::beast TYPE)
  if(NOT beast_type STREQUAL "INTERFACE_LIBRARY")
    message(FATAL_ERROR "The Beast transport must be header-only")
  endif()
  add_executable(installed_beast beast.cpp second.cpp)
  target_link_libraries(installed_beast PRIVATE mcp::beast Boost::json)
  target_compile_definitions(installed_beast PRIVATE MCPP_EXPECT_TLS=$<BOOL:${MCPpp_BEAST_TLS}>)
  add_test(NAME beast_multiple_translation_units COMMAND installed_beast)
endif()
