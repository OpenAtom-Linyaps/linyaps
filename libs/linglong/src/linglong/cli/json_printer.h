/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_CLI_JSON_PRINTER_H_
#define LINGLONG_CLI_JSON_PRINTER_H_

#include "linglong/cli/printer.h"

namespace linglong::cli {

class JSONPrinter : public Printer
{
public:
    void printErr(const utils::error::Error &) override;
    void printPackage(const api::types::v1::PackageInfo &) override;
    void printPackages(const std::vector<api::types::v1::PackageInfo> &) override;
    void printContainers(const std::vector<api::types::v1::CliContainer> &) override;
    void printReply(const api::types::v1::CommonResult &) override;
    void printRepoConfig(const api::types::v1::RepoConfig &) override;
    void printLayerInfo(const api::types::v1::LayerInfo &) override;
    void printTaskStatus(const QString &percentage, const QString &message, int status) override;
    void printContent(const QStringList &desktopPaths) override;
};

} // namespace linglong::cli

#endif
