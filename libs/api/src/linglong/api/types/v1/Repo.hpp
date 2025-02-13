// This file is generated by tools/codegen.sh
// DO NOT EDIT IT.

// clang-format off

//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     Repo.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <optional>
#include <nlohmann/json.hpp>
#include "linglong/api/types/v1/helper.hpp"

namespace linglong {
namespace api {
namespace types {
namespace v1 {
/**
* Configuration for a single repository.
*/

using nlohmann::json;

/**
* Configuration for a single repository.
*/
struct Repo {
/**
* alias of repo name
*/
std::optional<std::string> alias;
/**
* repo name
*/
std::string name;
/**
* repo url
*/
std::string url;
};
}
}
}
}

// clang-format on
