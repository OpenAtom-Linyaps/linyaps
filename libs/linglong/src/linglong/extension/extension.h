/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <string>

namespace linglong::extension {

class ExtensionIf
{
public:
    ExtensionIf() { };
    virtual ~ExtensionIf() = 0;

    virtual bool shouldDownload(std::string &) { return false; }

    virtual bool shouldEnable(std::string &) { return false; }

    virtual bool shouldPrune(std::string &) { return false; }
};

class ExtensionFactory
{
public:
    static ExtensionIf *makeExtension(const std::string &name);
};

class ExtensionImplNVIDIADisplayDriver : public ExtensionIf
{
public:
    ExtensionImplNVIDIADisplayDriver();

    ~ExtensionImplNVIDIADisplayDriver() override { }

    bool shouldEnable(std::string &extensionName) override;

    static std::string name;

private:
    std::string hostDriverEnable();

    std::string driverName;
};

} // namespace linglong::extension
