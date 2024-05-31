/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

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

    auto &mounts = content["mounts"];
    auto bindIfExist = [&mounts](std::string_view source, std::string_view destination) mutable {
        if (!std::filesystem::exists(source)) {
            return;
        }

        auto realDest = destination.empty() ? source : destination;
        mounts.push_back({ { "source", source },
                           { "type", "bind" },
                           { "destination", realDest },
                           { "options", nlohmann::json::array({ "rbind" }) } });
    };

    bindIfExist("/run/udev", "");
    bindIfExist("/dev/snd", "");
    bindIfExist("/dev/dri", "");

    nlohmann::json videoMounts = nlohmann::json::array();
    for (const auto &entry : std::filesystem::directory_iterator{ "/dev" }) {
        const auto &devPath = entry.path();
        auto devName = devPath.filename().string();
        if ((devName.rfind("video", 0) == 0) || (devName.rfind("nvidia", 0) == 0)) {
            auto dev = u8R"(
            {
                "type": "bind",
                "options": [ "rbind" ]
            })"_json;
            dev["destination"] = devPath.string();
            dev["source"] = devPath.string();

            videoMounts.emplace_back(std::move(dev));
        }
    }

    mounts.insert(mounts.end(), videoMounts.begin(), videoMounts.end());
    std::cout << content.dump() << std::endl;
    return 0;
}
