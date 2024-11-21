// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "nlohmann/json.hpp"

#include <filesystem>
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

    if (annotations.find("org.deepin.linglong.runtimeDir") != annotations.end()) {
        mounts.push_back({ { "destination", "/runtime" },
                           { "options", nlohmann::json::array({ "rbind", "ro" }) },
                           { "source",
                             std::filesystem::path(
                               annotations["org.deepin.linglong.runtimeDir"].get<std::string>())
                               / "files" },
                           { "type", "bind" } });
    }

    if (annotations.find("org.deepin.linglong.appDir") != annotations.end()) {
        mounts.push_back({
          { "destination", "/opt" },
          { "options", nlohmann::json::array({ "nodev", "nosuid", "mode=700" }) },
          { "source", "tmpfs" },
          { "type", "tmpfs" },
        });

        mounts.push_back(
          { { "destination", annotations["org.deepin.linglong.appPrefix"] },
            { "options", nlohmann::json::array({ "rbind", "ro" }) },
            { "source",
              std::filesystem::path(annotations["org.deepin.linglong.appDir"].get<std::string>())
                / "files" },
            { "type", "bind" } });
    }

    std::cout << content.dump() << std::endl;
    return 0;
}
