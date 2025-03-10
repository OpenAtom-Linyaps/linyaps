// This file is generated by tools/codegen.sh
// DO NOT EDIT IT.

// clang-format off

//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     Apt.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <optional>
#include <nlohmann/json.hpp>
#include "linglong/api/types/v1/helper.hpp"

namespace linglong {
namespace api {
namespace types {
namespace v1 {
/**
* build extension for apt
*/

using nlohmann::json;

/**
* build extension for apt
*/
struct Apt {
/**
* packages to be installed in build environment before build starting
*/
std::optional<std::vector<std::string>> buildDepends;
/**
* packages to be installed in runtime environment
*/
std::optional<std::vector<std::string>> depends;
};
}
}
}
}

// clang-format on
