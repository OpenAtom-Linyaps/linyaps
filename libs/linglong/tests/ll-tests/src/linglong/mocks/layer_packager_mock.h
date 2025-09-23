// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/package/layer_packager.h"
#include "linglong/utils/error/error.h"

#include <filesystem>
#include <functional>
#include <string>

using namespace linglong;

namespace linglong::package {

class MockLayerPackager : public package::LayerPackager
{
public:
    using package::LayerPackager::initWorkDir;

    // Mock virtual methods that need to be overridden for testing
    std::function<utils::error::Result<bool>()> wrapCheckErofsFuseExistsFunc;
    std::function<utils::error::Result<void>(const std::string &)> wrapMkdirDirFunc;
    std::function<bool(const std::string &)> wrapIsFileReadableFunc;

protected:
    utils::error::Result<bool> checkErofsFuseExists() const override
    {
        return wrapCheckErofsFuseExistsFunc ? wrapCheckErofsFuseExistsFunc()
                                            : LayerPackager::checkErofsFuseExists();
    }

    utils::error::Result<void> mkdirDir(const std::string &path) noexcept override
    {
        return wrapMkdirDirFunc ? wrapMkdirDirFunc(path) : LayerPackager::mkdirDir(path);
    }

    bool isFileReadable(const std::string &path) const override
    {
        return wrapIsFileReadableFunc ? wrapIsFileReadableFunc(path)
                                      : LayerPackager::isFileReadable(path);
    }
};

} // namespace linglong::package