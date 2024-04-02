// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "nlohmann/json.hpp"

#include <iostream>

int main()
{
    nlohmann::json content;
    std::string ociVersion;
    try {
        content = nlohmann::json::parse(std::cin);
        ociVersion = content.at("ociVersion");
    } catch (std::exception &exp) {
        std::cerr << exp.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "unknown error occurred during parsing json." << std::endl;
        return -1;
    }

    if (ociVersion != "1.0.1") {
        std::cerr << "OCI version mismatched." << std::endl;
        return -1;
    }

    nlohmann::json annotations;
    std::string appID;
    try {
        annotations = content.at("annotations");
        appID = annotations.at("org.deepin.linglong.appID");
    } catch (std::exception &exp) {
        std::cerr << exp.what() << std::endl;
        return -1;
    }

    auto &mounts = content["mounts"];

    if (annotations.contains("org.deepin.linglong.runtimeDir")) {
        mounts.push_back(
          { { "destination", "/runtime" },
            { "options", nlohmann::json::array({ "rbind", "ro" }) },
            { "source",
                std::filesystem::path("") / annotations["org.deepin.linglong.runtimeDir"] / "files" },
            { "type", "bind" } });
    }

    if (annotations.contains("org.deepin.linglong.appDir")) {
        mounts.push_back({
          { "destination", "/opt/apps/" },
          { "options", nlohmann::json::array({ "nodev", "nosuid", "mode=700" }) },
          { "source", "tmpfs" },
          { "type", "tmpfs" },
        });

        mounts.push_back(
          { { "destination",
              std::filesystem::path("/opt/apps") / annotations["org.deepin.linglong.appID"]
                / "files" },
            { "options", nlohmann::json::array({ "rbind", "rw" }) },
            { "source",
              std::filesystem::path("") / annotations["org.deepin.linglong.appDir"] / "files" },
            { "type", "bind" } });
    }

    std::cout << content.dump() << std::endl;
    return 0;
}
