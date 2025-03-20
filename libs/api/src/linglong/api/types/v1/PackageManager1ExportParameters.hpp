// This file is generated by tools/codegen.sh
// DO NOT EDIT IT.

// clang-format off

//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     PackageManager1ExportParameters.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <optional>
#include <nlohmann/json.hpp>
#include "linglong/api/types/v1/helper.hpp"

namespace linglong {
namespace api {
namespace types {
namespace v1 {
/**
* Export path
*/

using nlohmann::json;

/**
* Export path
*/
struct PackageManager1ExportParameters {
/**
* export's app id
*/
std::string appID;
/**
* Whether to export full package
*/
std::optional<bool> full;
/**
* Path to icon
*/
std::optional<std::string> iconPath;
/**
* Path to loader
*/
std::optional<std::string> loader;
};
}
}
}
}

// clang-format on
