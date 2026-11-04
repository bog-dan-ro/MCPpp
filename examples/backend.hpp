// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#pragma once
#if defined(MCPP_USE_BOOST)
#include <mcp++/json/boost.hpp>
using json_backend = mcp::boost_backend;
#elif defined(MCPP_USE_QT)
#include <mcp++/json/qt.hpp>
using json_backend = mcp::qt_backend;
#else
#include <mcp++/json/nlohmann.hpp>
using json_backend = mcp::nlohmann_backend;
#endif
