/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/CliContainer.hpp"
#include "linglong/api/types/v1/CommonResult.hpp"
#include "linglong/api/types/v1/LayerInfo.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/RepoConfig.hpp"
#include "linglong/api/types/v1/State.hpp"
#include "linglong/api/types/v1/SubState.hpp"
#include "linglong/utils/error/error.h"

#include <QJsonObject>
#include <QObject>
#include <QString>

namespace linglong::cli {

class Printer
{
public:
    Printer() = default;
    Printer(const Printer &) = delete;
    Printer(Printer &&) = delete;
    Printer &operator=(const Printer &) = delete;
    Printer &operator=(Printer &&) = delete;
    virtual ~Printer() = default;

    virtual void printErr(const utils::error::Error &);
    virtual void printPackage(const api::types::v1::PackageInfoV2 &);
    virtual void printPackages(const std::vector<api::types::v1::PackageInfoV2> &);
    virtual void printContainers(const std::vector<api::types::v1::CliContainer> &);
    virtual void printReply(const api::types::v1::CommonResult &);
    virtual void printRepoConfig(const api::types::v1::RepoConfig &);
    virtual void printLayerInfo(const api::types::v1::LayerInfo &);
    virtual void printTaskState(const double percentage,
                                 const QString &message,
                                 const api::types::v1::State state,
                                 const api::types::v1::SubState subState);
    virtual void printContent(const QStringList &filePaths);

private:
    void printPackageInfo(const api::types::v1::PackageInfoV2 &);
};

} // namespace linglong::cli
