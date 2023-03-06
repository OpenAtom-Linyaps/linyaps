/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_BUILDER_BUILDER_H_
#define LINGLONG_SRC_BUILDER_BUILDER_BUILDER_H_

#include "module/util/error.h"

#include <QObject>

namespace linglong {
namespace builder {

class Builder
{
public:
    virtual linglong::util::Error config(const QString &userName, const QString &password) = 0;

    virtual linglong::util::Error create(const QString &projectName) = 0;

    virtual linglong::util::Error build() = 0;

    virtual linglong::util::Error exportBundle(const QString &outputFilepath, bool useLocalDir) = 0;

    virtual util::Error push(const QString &repoUrl,
                             const QString &repoName,
                             const QString &channel,
                             bool pushWithDevel) = 0;

    virtual linglong::util::Error import() = 0;

    virtual linglong::util::Error track() = 0;

    virtual linglong::util::Error run() = 0;
};

void registerAllMetaType();

} // namespace builder
} // namespace linglong

#endif // LINGLONG_SRC_BUILDER_BUILDER_BUILDER_H_
