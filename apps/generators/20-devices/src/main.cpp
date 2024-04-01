/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "nlohmann/json.hpp"

#include <iostream>

int main()
{
    using namespace nlohmann::literals;

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

    auto commonMounts = u8R"( [
        {
            "destination": "/run/udev",
            "type": "bind",
            "source": "/run/udev",
            "options": [
                    "rbind"
            ]
        },{
            "destination": "/dev/dri",
            "type": "bind",
            "source": "/dev/dri",
            "options": [
                    "rbind"
            ]
        },{
            "destination": "/dev/snd",
            "type": "bind",
            "source": "/dev/snd",
            "options": [
                    "rbind"
            ]
        }
    ])"_json;

    auto& mounts = content["mounts"];
    mounts.insert(mounts.end(), commonMounts.begin(), commonMounts.end());

    nlohmann::json videoMounts = nlohmann::json::array();

    // for Video Capture Interface:
    // https://www.kernel.org/doc/html/v4.19/media/uapi/v4l/dev-capture.html
    // assume using udev
    for (std::size_t index = 0; index < 64; ++index) {
        auto devPath = "/dev/video" + std::to_string(index);
        if (!std::filesystem::exists(devPath)) {
            break;
        }

        auto dev = u8R"(
            {
                "type": "bind",
                "options": [ "rbind" ]
            }
        )"_json;
        dev["destination"] = devPath;
        dev["source"] = devPath;

        videoMounts.emplace_back(std::move(dev));
    }

    mounts.insert(mounts.end(), videoMounts.begin(), videoMounts.end());
    std::cout << content.dump() << std::endl;

    return 0;
}
