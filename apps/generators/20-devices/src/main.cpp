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
            auto dev = R"(
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

    // using FHS media directory and ignore '/run/media' for now
    // FIXME: the mount base location of udisks will be affected by the flag '--enable-fhs-media',
    // if not set this option, udisks will choose `/run/media` as the mount location. some linux
    // distros (e.g. ArchLinux) don't have this flag enabled, perhaps we could find a better way to
    // compatible with those distros.
    // https://github.com/storaged-project/udisks/commit/ae2a5ff1e49ae924605502ace170eb831e9c38e4
    if (std::filesystem::exists("/media")) {
        mounts.push_back({ { "source", "/media" },
                           { "type", "bind" },
                           { "destination", "/media" },
                           { "options", { "rbind", "rshared", "copy-symlink" } } });
    }

    std::cout << content.dump() << std::endl;
    return 0;
}
