// Copyright (c) 2026 BogDan Vatra
// SPDX-License-Identifier: MIT

#include <mcp++/annotations.hpp>
#include <meta>
#include <string_view>
#include <vector>

struct Sample {
    [[=mcp::tool]] void printMessage(std::string_view message) {}
    [[=mcp::tool]] int add(int a, int b) { return a + b; }
    void privateMemberFunction() {}
private:
    [[=mcp::tool]] int secret(int password) { return password; }
};

consteval auto tools() {
    std::vector<std::meta::info> result;
    for (auto member : std::meta::members_of(^^Sample,
                          std::meta::access_context::unprivileged())) {
        if (std::meta::is_function(member) &&
            !std::meta::annotations_of_with_type(member, ^^mcp::tool_t).empty())
            result.push_back(member);
    }
    return result;
}

constexpr auto exposed = std::define_static_array(tools());
static_assert(exposed.size() == 2);
static_assert(std::meta::identifier_of(exposed[0]) == "printMessage");
static_assert(std::meta::identifier_of(exposed[1]) == "add");
static_assert(std::meta::return_type_of(exposed[0]) == ^^void);
static_assert(std::meta::return_type_of(exposed[1]) == ^^int);
constexpr auto add_params = std::define_static_array(std::meta::parameters_of(exposed[1]));
static_assert(add_params.size() == 2);
static_assert(std::meta::identifier_of(add_params[0]) == "a");
static_assert(std::meta::identifier_of(add_params[1]) == "b");
static_assert(std::meta::type_of(add_params[0]) == ^^int);
static_assert(std::meta::type_of(add_params[1]) == ^^int);
constexpr auto print_params = std::define_static_array(std::meta::parameters_of(exposed[0]));
static_assert(print_params.size() == 1);
static_assert(std::meta::identifier_of(print_params[0]) == "message");
static_assert(std::meta::type_of(print_params[0]) == std::meta::dealias(^^std::string_view));

int main() {
    Sample sample;
    template for (constexpr auto member : exposed) {
        if constexpr (std::meta::identifier_of(member) == "add") {
            if (sample.[:member:](2, 3) != 5) return 1;
        }
    }
}
