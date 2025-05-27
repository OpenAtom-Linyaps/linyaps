/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "extension.h"

#include <algorithm>
#include <fstream>

namespace linglong::extension {

ExtensionIf::~ExtensionIf() { }

std::unique_ptr<ExtensionIf> ExtensionFactory::makeExtension(const std::string &name)
{
    if (name == ExtensionImplNVIDIADisplayDriver::Identify) {
        return std::make_unique<ExtensionImplNVIDIADisplayDriver>();
    }

    return std::make_unique<ExtensionImplDummy>();
}

ExtensionImplNVIDIADisplayDriver::ExtensionImplNVIDIADisplayDriver()
{
    driverName = hostDriverEnable();
}

bool ExtensionImplNVIDIADisplayDriver::shouldEnable(std::string &extensionName)
{
    if (extensionName != Identify) {
        return false;
    }

    if (!driverName.empty()) {
        extensionName = std::string(Identify) + "." + driverName;
        return true;
    }

    return false;
}

std::string ExtensionImplNVIDIADisplayDriver::hostDriverEnable()
{
    std::string version;

    std::ifstream versionFile("/sys/module/nvidia/version");
    if (versionFile) {
        versionFile >> version;

        std::replace(version.begin(), version.end(), '.', '-');
    }

    return version;
}

} // namespace linglong::extension
