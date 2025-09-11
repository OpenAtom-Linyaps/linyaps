// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// refer: https://zh.wikipedia.org/wiki/SHA-2

#pragma once

#include "linglong/builder/linglong_builder.h"
#include "linglong/cli/cli.h"

#include <string>
#include <vector>

struct CreateCommandOptions
{
    std::string projectName;
};

struct BuildCommandOptions
{
    std::vector<std::string> commands;
    bool buildOffline = false;
    linglong::builder::BuilderBuildOptions builderSpecificOptions;
};

struct RunCommandOptions
{
    std::vector<std::string> execModules;
    std::vector<std::string> commands;
    bool debugMode = false;
};

struct ListCommandOptions
{
    // No members needed yet
};

struct RemoveCommandOptions
{
    bool noCleanObjects = false;
    std::vector<std::string> removeList; // List of apps to remove
};

struct ExportCommandOptions
{
    linglong::builder::ExportOption exportSpecificOptions;
    bool layerMode = false;
    std::string outputFile;
};

struct PushCommandOptions
{
    std::vector<std::string> pushModules;
    linglong::cli::RepoOptions repoOptions;
};

struct ImportCommandOptions
{
    std::string layerFile; // Path to the layer file
};

struct ImportDirCommandOptions
{
    std::string layerDir; // Path to the layer directory
};

struct ExtractCommandOptions
{
    std::string layerFile;
    std::string dir;
};

struct RepoSubcommandOptions
{
    linglong::cli::RepoOptions repoOptions;
};
