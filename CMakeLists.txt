# Copyright (c) 2026 BogDan Vatra
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.30)
project(mcp_plus_plus VERSION 0.1.0 LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
  include(CTest)
  set(mcpp_default_tests "${BUILD_TESTING}")
else()
  set(mcpp_default_tests OFF)
endif()
option(MCPP_BUILD_TESTS "Build MCP++ tests" "${mcpp_default_tests}")
option(MCPP_BUILD_EXAMPLES "Build MCP++ examples" "${PROJECT_IS_TOP_LEVEL}")
option(MCPP_INSTALL "Enable MCP++ install/export rules" "${PROJECT_IS_TOP_LEVEL}")
if(MCPP_BUILD_TESTS)
  enable_testing()
endif()
include(CheckCXXSourceCompiles)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_REQUIRED_FLAGS "-std=c++26 -freflection")
check_cxx_source_compiles("
  #include <meta>
  struct tag {}; constexpr tag marker{};
  struct S { [[=marker]] int f(int a) { return a; } };
  static_assert(!std::meta::annotations_of_with_type(^^S::f, ^^tag).empty());
  int main() {}" MCPP_HAS_REFLECTION)
if(NOT MCPP_HAS_REFLECTION)
  message(FATAL_ERROR "MCP++ needs C++26 reflection and annotations. Tested: GCC 16.2 with -std=c++26 -freflection. See CMakeFiles/CMakeConfigureLog.yaml.")
endif()

add_library(mcpp INTERFACE)
add_library(mcp::mcpp ALIAS mcpp)
target_include_directories(mcpp INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_compile_features(mcpp INTERFACE cxx_std_26)
target_compile_options(mcpp INTERFACE -freflection)

option(MCPP_WITH_NLOHMANN "Build the optional nlohmann/json adapter example and tests" OFF)
option(MCPP_WITH_QT "Build the optional Qt JSON adapter example and tests" OFF)
option(MCPP_WITH_BOOST "Build the Boost.JSON adapter example and tests" "${PROJECT_IS_TOP_LEVEL}")
option(MCPP_WITH_BEAST "Build the optional Beast HTTP transport and examples" OFF)
option(MCPP_WITH_BEAST_TLS "Enable OpenSSL TLS in the Beast transport" ON)
option(MCPP_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(MCPP_VALIDATE_PROTOCOL "Validate process-test responses against the pinned official schema" OFF)
option(MCPP_EXTERNAL_CHECKS "Run separately installed, pinned Inspector and conformance tools" OFF)
if(MCPP_SANITIZERS)
  target_compile_options(mcpp INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(mcpp INTERFACE -fsanitize=address,undefined)
endif()

if(MCPP_BUILD_TESTS)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  if(MCPP_VALIDATE_PROTOCOL)
    execute_process(COMMAND ${Python3_EXECUTABLE} -c "import jsonschema"
      RESULT_VARIABLE schema_dependency OUTPUT_QUIET ERROR_QUIET)
    if(NOT schema_dependency EQUAL 0)
      message(FATAL_ERROR "MCPP_VALIDATE_PROTOCOL requires Python jsonschema; select its environment with Python3_EXECUTABLE")
    endif()
    # Tests remain offline; the schema and its source hash are stored in-tree.
    set(MCPP_SCHEMA_ENV "MCPP_PROTOCOL_SCHEMA=${CMAKE_CURRENT_SOURCE_DIR}/tests/protocol/mcp-2026-07-28.schema.json")
  endif()
  if(MCPP_WITH_BOOST OR MCPP_WITH_NLOHMANN OR MCPP_WITH_QT)
    find_package(Boost 1.92.0 EXACT REQUIRED COMPONENTS unit_test_framework)
  endif()
  add_executable(reflection_smoke tests/reflection_smoke.cpp)
  target_link_libraries(reflection_smoke PRIVATE mcpp)
  add_test(NAME reflection_smoke COMMAND reflection_smoke)
endif()

if(MCPP_WITH_BEAST)
  find_package(Boost 1.92.0 EXACT REQUIRED COMPONENTS beast asio)
  find_package(Threads REQUIRED)
  add_library(mcpp_beast INTERFACE)
  add_library(mcp::beast ALIAS mcpp_beast)
  set_target_properties(mcpp_beast PROPERTIES EXPORT_NAME beast)
  target_link_libraries(mcpp_beast INTERFACE mcpp Boost::beast Boost::asio Threads::Threads)
  if(MCPP_WITH_BEAST_TLS)
    find_package(OpenSSL 3.0 REQUIRED)
    target_link_libraries(mcpp_beast INTERFACE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(mcpp_beast INTERFACE MCPP_BEAST_TLS=1)
    if(MCPP_BUILD_TESTS)
      find_program(MCPP_OPENSSL_EXECUTABLE openssl REQUIRED)
      add_test(NAME beast_test_certificate COMMAND ${CMAKE_COMMAND}
        "-DOPENSSL_EXECUTABLE:FILEPATH=${MCPP_OPENSSL_EXECUTABLE}"
        "-DCERTIFICATE_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/test-tls"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_certificate.cmake")
      set_tests_properties(beast_test_certificate PROPERTIES FIXTURES_SETUP beast_tls)
    endif()
  endif()
endif()

function(mcpp_select_backend target dependency)
  target_link_libraries(${target} PRIVATE mcpp ${dependency})
  if(dependency STREQUAL "Boost::json")
    target_compile_definitions(${target} PRIVATE MCPP_USE_BOOST)
  elseif(dependency STREQUAL "Qt6::Core")
    target_compile_definitions(${target} PRIVATE MCPP_USE_QT)
  endif()
endfunction()

function(mcpp_backend suffix dependency)
  add_executable(calculator${suffix} examples/calculator.cpp)
  mcpp_select_backend(calculator${suffix} ${dependency})
  add_executable(fulfillment${suffix} examples/fulfillment.cpp)
  mcpp_select_backend(fulfillment${suffix} ${dependency})
  if(MCPP_BUILD_TESTS)
    foreach(test schema_tests dispatch_tests)
      add_executable(${test}${suffix} tests/${test}.cpp)
      target_link_libraries(${test}${suffix} PRIVATE mcpp ${dependency} Boost::unit_test_framework)
      target_compile_definitions(${test}${suffix} PRIVATE BOOST_TEST_DYN_LINK)
      if(dependency STREQUAL "Boost::json")
        target_compile_definitions(${test}${suffix} PRIVATE MCPP_USE_BOOST)
      elseif(dependency STREQUAL "Qt6::Core")
        target_compile_definitions(${test}${suffix} PRIVATE MCPP_USE_QT)
      endif()
      add_test(NAME ${test}${suffix} COMMAND ${test}${suffix})
    endforeach()
    add_test(NAME stdio_smoke${suffix} COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/stdio_smoke.py $<TARGET_FILE:calculator${suffix}>)
    add_test(NAME fulfillment_smoke${suffix} COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/fulfillment_smoke.py $<TARGET_FILE:fulfillment${suffix}>)
    if(MCPP_VALIDATE_PROTOCOL)
      set_tests_properties(stdio_smoke${suffix} fulfillment_smoke${suffix} PROPERTIES ENVIRONMENT "${MCPP_SCHEMA_ENV}")
    endif()
  endif()
  if(MCPP_WITH_BEAST)
    foreach(example calculator fulfillment)
      if(example STREQUAL "calculator")
        set(source examples/calculator_http.cpp)
      else()
        set(source examples/fulfillment.cpp)
      endif()
      add_executable(${example}_http${suffix} ${source})
      mcpp_select_backend(${example}_http${suffix} ${dependency})
      target_link_libraries(${example}_http${suffix} PRIVATE mcp::beast)
      target_compile_definitions(${example}_http${suffix} PRIVATE MCPP_EXAMPLE_HTTP)
    endforeach()
    if(MCPP_BUILD_TESTS)
      add_executable(beast_tests${suffix} tests/beast_tests.cpp)
      mcpp_select_backend(beast_tests${suffix} ${dependency})
      target_link_libraries(beast_tests${suffix} PRIVATE mcp::beast Boost::unit_test_framework)
      target_compile_definitions(beast_tests${suffix} PRIVATE BOOST_TEST_DYN_LINK)
      add_test(NAME beast_tests${suffix} COMMAND beast_tests${suffix})
      add_test(NAME http_smoke${suffix} COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/http_smoke.py $<TARGET_FILE:calculator_http${suffix}>)
      add_test(NAME fulfillment_http_smoke${suffix} COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/fulfillment_smoke.py $<TARGET_FILE:fulfillment_http${suffix}> --http)
      add_test(NAME auth_smoke${suffix} COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/auth_smoke.py $<TARGET_FILE:calculator_http${suffix}>)
      if(MCPP_VALIDATE_PROTOCOL)
        set_tests_properties(http_smoke${suffix} fulfillment_http_smoke${suffix} auth_smoke${suffix} PROPERTIES ENVIRONMENT "${MCPP_SCHEMA_ENV}")
      endif()
      if(MCPP_WITH_BEAST_TLS)
        add_test(NAME https_smoke${suffix} COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_CURRENT_SOURCE_DIR}/tests/http_smoke.py $<TARGET_FILE:calculator_http${suffix}>
          --tls "${CMAKE_CURRENT_BINARY_DIR}/test-tls/cert.pem" "${CMAKE_CURRENT_BINARY_DIR}/test-tls/key.pem")
        add_test(NAME fulfillment_https_smoke${suffix} COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_CURRENT_SOURCE_DIR}/tests/fulfillment_smoke.py $<TARGET_FILE:fulfillment_http${suffix}> --http
          --tls "${CMAKE_CURRENT_BINARY_DIR}/test-tls/cert.pem" "${CMAKE_CURRENT_BINARY_DIR}/test-tls/key.pem")
        add_test(NAME auth_https_smoke${suffix} COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_CURRENT_SOURCE_DIR}/tests/auth_smoke.py $<TARGET_FILE:calculator_http${suffix}>
          --tls "${CMAKE_CURRENT_BINARY_DIR}/test-tls/cert.pem" "${CMAKE_CURRENT_BINARY_DIR}/test-tls/key.pem")
        set_tests_properties(https_smoke${suffix} fulfillment_https_smoke${suffix} auth_https_smoke${suffix} PROPERTIES FIXTURES_REQUIRED beast_tls)
        if(MCPP_VALIDATE_PROTOCOL)
          set_tests_properties(https_smoke${suffix} fulfillment_https_smoke${suffix} auth_https_smoke${suffix} PROPERTIES ENVIRONMENT "${MCPP_SCHEMA_ENV}")
        endif()
      endif()
    endif()
  endif()
endfunction()

# Dependencies belong to adapter consumers only; mcpp itself has none.
if(MCPP_WITH_NLOHMANN AND (MCPP_BUILD_EXAMPLES OR MCPP_BUILD_TESTS))
  find_package(nlohmann_json 3.11.3 EXACT REQUIRED CONFIG)
  mcpp_backend("_nlohmann" nlohmann_json::nlohmann_json)
endif()

if(MCPP_WITH_QT AND (MCPP_BUILD_EXAMPLES OR MCPP_BUILD_TESTS))
  find_package(Qt6 REQUIRED COMPONENTS Core)
  mcpp_backend("_qt" Qt6::Core)
endif()

if(MCPP_BUILD_TESTS AND MCPP_WITH_BEAST AND MCPP_WITH_BOOST)
  add_executable(slow_http_server tests/slow_http_server.cpp)
  target_link_libraries(slow_http_server PRIVATE mcp::beast Boost::json)
  add_test(NAME shutdown_smoke COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/shutdown_smoke.py $<TARGET_FILE:slow_http_server>)
  if(MCPP_VALIDATE_PROTOCOL)
    set_tests_properties(shutdown_smoke PROPERTIES ENVIRONMENT "${MCPP_SCHEMA_ENV}")
  endif()
  if(MCPP_WITH_BEAST_TLS)
    add_test(NAME shutdown_https_smoke COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/shutdown_smoke.py $<TARGET_FILE:slow_http_server>
      --tls "${CMAKE_CURRENT_BINARY_DIR}/test-tls/cert.pem" "${CMAKE_CURRENT_BINARY_DIR}/test-tls/key.pem")
    set_tests_properties(shutdown_https_smoke PROPERTIES FIXTURES_REQUIRED beast_tls)
    if(MCPP_VALIDATE_PROTOCOL)
      set_tests_properties(shutdown_https_smoke PROPERTIES ENVIRONMENT "${MCPP_SCHEMA_ENV}")
    endif()
  endif()
endif()
if(MCPP_WITH_BOOST AND (MCPP_BUILD_EXAMPLES OR MCPP_BUILD_TESTS))
  find_package(Boost 1.92.0 EXACT REQUIRED COMPONENTS json)
  mcpp_backend("" Boost::json)
endif()

if(MCPP_BUILD_TESTS AND MCPP_EXTERNAL_CHECKS)
  if(NOT MCPP_WITH_BEAST OR NOT MCPP_WITH_BOOST)
    message(FATAL_ERROR "MCPP_EXTERNAL_CHECKS requires MCPP_WITH_BEAST and MCPP_WITH_BOOST")
  endif()
  find_program(MCPP_INSPECTOR_EXECUTABLE mcp-inspector HINTS
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verification/node_modules/.bin" REQUIRED)
  find_program(MCPP_CONFORMANCE_EXECUTABLE conformance HINTS
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verification/node_modules/.bin" REQUIRED)
  add_test(NAME official_conformance COMMAND ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance_smoke.py" $<TARGET_FILE:calculator_http>
    "${MCPP_CONFORMANCE_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/conformance-results")
  add_test(NAME inspector_smoke COMMAND ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/inspector_smoke.py" $<TARGET_FILE:calculator>
    $<TARGET_FILE:calculator_http> "${MCPP_INSPECTOR_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/inspector-http")
  if(MCPP_WITH_BEAST_TLS)
    add_test(NAME inspector_https_smoke COMMAND ${Python3_EXECUTABLE}
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/inspector_smoke.py" $<TARGET_FILE:calculator>
      $<TARGET_FILE:calculator_http> "${MCPP_INSPECTOR_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/inspector-https"
      --tls "${CMAKE_CURRENT_BINARY_DIR}/test-tls/cert.pem" "${CMAKE_CURRENT_BINARY_DIR}/test-tls/key.pem")
    set_tests_properties(inspector_https_smoke PROPERTIES FIXTURES_REQUIRED beast_tls)
  endif()
endif()

if(MCPP_BUILD_TESTS)
  add_test(NAME compile_fail COMMAND ${CMAKE_COMMAND}
    -S "${CMAKE_CURRENT_SOURCE_DIR}/tests/compile_fail"
    -B "${CMAKE_CURRENT_BINARY_DIR}/compile_fail"
    -G "${CMAKE_GENERATOR}"
    "-DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
    "-DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}"
    "-DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}"
    "-DCMAKE_TRY_COMPILE_CONFIGURATION:STRING=$<CONFIG>")
endif()

if(MCPP_INSTALL)
# Both public targets contain only usage requirements; consumers compile the
# complete implementation from the installed headers.
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  FILES_MATCHING PATTERN "*.hpp")
install(TARGETS mcpp EXPORT MCPppCoreTargets)
install(EXPORT MCPppCoreTargets FILE MCPppCoreTargets.cmake NAMESPACE mcp::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MCPpp")
set(MCPP_PACKAGE_BEAST_TLS OFF)
if(MCPP_WITH_BEAST)
  install(TARGETS mcpp_beast EXPORT MCPppBeastTargets)
  install(EXPORT MCPppBeastTargets FILE MCPppBeastTargets.cmake NAMESPACE mcp::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MCPpp")
  if(MCPP_WITH_BEAST_TLS)
    set(MCPP_PACKAGE_BEAST_TLS ON)
  endif()
endif()
configure_package_config_file(cmake/MCPppConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/MCPppConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MCPpp")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/MCPppConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMinorVersion ARCH_INDEPENDENT)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/MCPppConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/MCPppConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MCPpp")
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/MCPpp")
endif()

if(MCPP_BUILD_TESTS AND MCPP_INSTALL)
  add_test(NAME installed_package COMMAND ${CMAKE_COMMAND}
    "-DMCPP_PRODUCER_BINARY:PATH=${CMAKE_CURRENT_BINARY_DIR}"
    "-DMCPP_CONSUMER_SOURCE:PATH=${CMAKE_CURRENT_SOURCE_DIR}/tests/consumer"
    "-DMCPP_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
    "-DMCPP_GENERATOR:STRING=${CMAKE_GENERATOR}"
    "-DMCPP_CONFIG:STRING=$<CONFIG>"
    "-DMCPP_BEAST:BOOL=${MCPP_WITH_BEAST}"
    "-DMCPP_PREFIX_PATH:STRING=${CMAKE_PREFIX_PATH}"
    "-DMCPP_INSTALL_DATADIR:STRING=${CMAKE_INSTALL_DATADIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/install_consumer.cmake")
  set_tests_properties(installed_package PROPERTIES RESOURCE_LOCK mcpp_consumer_build)
endif()

if(MCPP_BUILD_TESTS)
  add_test(NAME fetch_content COMMAND ${CMAKE_COMMAND}
    "-DMCPP_PRODUCER_BINARY:PATH=${CMAKE_CURRENT_BINARY_DIR}"
    "-DMCPP_SOURCE_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}"
    "-DMCPP_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
    "-DMCPP_GENERATOR:STRING=${CMAKE_GENERATOR}"
    "-DMCPP_CONFIG:STRING=$<CONFIG>"
    "-DMCPP_BEAST:BOOL=${MCPP_WITH_BEAST}"
    "-DMCPP_TLS:BOOL=${MCPP_WITH_BEAST_TLS}"
    "-DMCPP_PREFIX_PATH:STRING=${CMAKE_PREFIX_PATH}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/fetch_content.cmake")
  set_tests_properties(fetch_content PROPERTIES RESOURCE_LOCK mcpp_consumer_build)
endif()
