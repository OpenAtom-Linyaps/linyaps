/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong <huqinghong@uniontech.com>
 *
 * Maintainer: huqinghong <huqinghong@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QString>

namespace linglong {
namespace util {

// 包管理命令可选参数
const QString KEY_VERSION = "version";
const QString KEY_REPO_POINT = "repo-point";
const QString KEY_ARCH = "arch";
const QString KEY_DELDATA = "delete-data";
const QString KEY_NO_CACHE = "force";
const QString KEY_EXEC = "exec";

const QString KEY_BUS_TYPE = "bus-type";
const QString KEY_NO_PROXY = "no-proxy";
const QString KEY_FILTER_NAME = "filter-name";
const QString KEY_FILTER_PATH = "filter-path";
const QString KEY_FILTER_IFACE = "filter-interface";
} // namespace util
} // namespace linglong
