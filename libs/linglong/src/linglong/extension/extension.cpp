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

ExtensionIf *ExtensionFactory::makeExtension(const std::string &name)
{
    if (name == ExtensionImplNVIDIADisplayDriver::name) {
        return new ExtensionImplNVIDIADisplayDriver();
    }

    return new ExtensionImplDummy();
}

std::string ExtensionImplNVIDIADisplayDriver::name = "org.deepin.driver.display.nvidia";

ExtensionImplNVIDIADisplayDriver::ExtensionImplNVIDIADisplayDriver()
{
    driverName = hostDriverEnable();
}

bool ExtensionImplNVIDIADisplayDriver::shouldEnable(std::string &extensionName)
{
    if (extensionName != name) {
        return false;
    }

    if (!driverName.empty()) {
        extensionName = name + "." + driverName;
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
