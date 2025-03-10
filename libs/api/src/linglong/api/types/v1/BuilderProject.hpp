// This file is generated by tools/codegen.sh
// DO NOT EDIT IT.

// clang-format off

//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     BuilderProject.hpp data = nlohmann::json::parse(jsonString);

#pragma once

#include <optional>
#include <nlohmann/json.hpp>
#include "linglong/api/types/v1/helper.hpp"

#include "linglong/api/types/v1/BuilderProjectBuildEXT.hpp"
#include "linglong/api/types/v1/BuilderProjectModules.hpp"
#include "linglong/api/types/v1/BuilderProjectPackage.hpp"
#include "linglong/api/types/v1/ApplicationConfigurationPermissions.hpp"
#include "linglong/api/types/v1/BuilderProjectSource.hpp"

namespace linglong {
namespace api {
namespace types {
namespace v1 {
/**
* Linglong project build file.
*/

using nlohmann::json;

/**
* Linglong project build file.
*/
struct BuilderProject {
/**
* used base of package
*/
std::string base;
/**
* build script of builder project
*/
std::string build;
/**
* build extension for builder project
*/
std::optional<BuilderProjectBuildEXT> buildext;
/**
* command of builder project
*/
std::optional<std::vector<std::string>> command;
/**
* exclude files during exporting UAB. Every item should be absolute path in container. It
* could be a directory or a regular file. e.g. - /usr/share/locale (exclude all files which
* in this directory) - /usr/lib/libavfs.a (exclude one file)
*/
std::optional<std::vector<std::string>> exclude;
/**
* include files during exporting UAB. For example, the packer can declare a file/directory
* (e.g. /usr/share/locale/zh_CN) in the excluded directory (e.g. /usr/share/locale)  to
* exclude all files in /usr/share/locale except /usr/share/locale/zh_CN.
*/
std::optional<std::vector<std::string>> include;
/**
* Specify how to split application into modules.
*/
std::optional<std::vector<BuilderProjectModules>> modules;
/**
* package of build file
*/
BuilderProjectPackage package;
std::optional<ApplicationConfigurationPermissions> permissions;
/**
* used runtime of package
*/
std::optional<std::string> runtime;
/**
* sources of package
*/
std::optional<std::vector<BuilderProjectSource>> sources;
/**
* strip script of builder project
*/
std::optional<std::string> strip;
/**
* version of build file
*/
std::string version;
};
}
}
}
}

// clang-format on
