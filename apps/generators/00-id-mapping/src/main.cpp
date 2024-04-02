// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "nlohmann/json.hpp"

#include <iostream>

#include <unistd.h>

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
        std::cerr << "Unknown error occurred during parsing json." << std::endl;
        return -1;
    }

    if (ociVersion != "1.0.1") {
        std::cerr << "OCI version mismatched." << std::endl;
        return -1;
    }

    content["linux"]["uidMappings"] = nlohmann::json::array({ nlohmann::json::object({
      { "containerID", ::getuid() },
      { "hostID", ::getuid() },
      { "size", 1 },
    }) });

    content["linux"]["gidMappings"] = nlohmann::json::array({ nlohmann::json::object({
      { "containerID", ::getgid() },
      { "hostID", ::getgid() },
      { "size", 1 },
    }) });

    std::cout << content.dump() << std::endl;
    return 0;
}
