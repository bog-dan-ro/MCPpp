# Use MCP++ from another CMake project

MCP++ and its optional Beast transport are header-only. The CMake targets
carry include paths, C++26 reflection flags and dependency links. Both
installation and FetchContent are tested with multi-file consumers.

## FetchContent

```cmake
cmake_minimum_required(VERSION 3.30)
project(my_tools LANGUAGES CXX)
include(FetchContent)

FetchContent_Declare(MCPpp
  GIT_REPOSITORY https://github.com/bog-dan-ro/MCPpp.git
  GIT_TAG master) # Pin a reviewed commit for reproducible builds.
FetchContent_MakeAvailable(MCPpp)

find_package(Boost 1.92.0 EXACT REQUIRED COMPONENTS json)
add_executable(my_tools main.cpp)
target_link_libraries(my_tools PRIVATE mcp::mcpp Boost::json)
```

The parent must select a reflection-capable compiler, for example with
`-DCMAKE_CXX_COMPILER=g++-16` at initial configuration. MCP++ supplies
`cxx_std_26` and `-freflection` through its interface target.

When embedded, `MCPP_BUILD_TESTS`, `MCPP_BUILD_EXAMPLES`, and `MCPP_INSTALL`
default to OFF, even when the parent's `BUILD_TESTING` is ON. Core-only
FetchContent does not discover Boost, OpenSSL, Python or Node. The application's
JSON backend remains its own dependency; the Boost.JSON link above is just one
choice.

For HTTP(S), configure the transport before making the dependency available:

```cmake
set(MCPP_WITH_BEAST ON CACHE BOOL "" FORCE)
set(MCPP_WITH_BEAST_TLS ON CACHE BOOL "" FORCE) # OFF for HTTP without OpenSSL.
FetchContent_Declare(MCPpp
  GIT_REPOSITORY https://github.com/bog-dan-ro/MCPpp.git
  GIT_TAG master)
FetchContent_MakeAvailable(MCPpp)

find_package(Boost 1.92.0 EXACT REQUIRED COMPONENTS json)
add_executable(my_tools main.cpp)
target_link_libraries(my_tools PRIVATE mcp::beast Boost::json)
```

The Beast target finds Boost.Beast/Asio and Threads. With TLS enabled, it also
finds OpenSSL 3.0 or newer and supplies `MCPP_BEAST_TLS=1` and the required
links. Include `<mcp++/beast.hpp>` in HTTP consumers. All translation units
must use the same TLS configuration; linking the target ensures this.

For an already checked-out source tree, replace the Git fields with
`SOURCE_DIR /path/to/MCPpp`, or use CMake's
`FETCHCONTENT_SOURCE_DIR_MCPPP` override.

## Install and find_package

```sh
cmake -S . -B build-install -G Ninja -DCMAKE_CXX_COMPILER=g++-16 \
  -DBUILD_TESTING=OFF -DMCPP_BUILD_EXAMPLES=OFF -DMCPP_WITH_BEAST=ON
cmake --install build-install --prefix /path/to/mcpp-prefix
```

The prefix contains headers, relocatable CMake package files and the MIT
license. External dependencies are found on the consuming machine.

For a core consumer:

```cmake
find_package(MCPpp 0.1 CONFIG REQUIRED COMPONENTS core)
target_link_libraries(my_tools PRIVATE mcp::mcpp)
```

For HTTP(S):

```cmake
find_package(MCPpp 0.1 CONFIG REQUIRED COMPONENTS beast)
target_link_libraries(my_tools PRIVATE mcp::beast)
```

Set `CMAKE_PREFIX_PATH=/path/to/mcpp-prefix` when configuring the parent. A
plain `find_package(MCPpp CONFIG REQUIRED)` loads only the core. It does not
discover transport or JSON dependencies, even if the installed package also
contains the Beast component. Requesting an unavailable component fails
clearly. `MCPpp_BEAST_TLS` reports the installed transport's TLS configuration.

The version file accepts patch updates within the requested major/minor
version. Experimental compiler reflection APIs may change independently;
GCC 16.2.0 is the tested compiler.

## Consumer checks

CTest's `installed_package` entry installs into a temporary prefix, moves that
prefix, and configures an independent project with `find_package`. It first
compiles a reflection consumer with Boost/OpenSSL discovery disabled, then
links and runs a Beast executable built from two translation units. It checks
the exported target types, TLS definition, absence of MCP++ library binaries,
and installed license.

The `fetch_content` entry configures a parent with `BUILD_TESTING=ON` and
checks that MCP++ tests, examples and install rules stay off. It builds and
runs core and optional multi-file Beast consumers, with Boost, OpenSSL and
Python discovery disabled for the core check.
