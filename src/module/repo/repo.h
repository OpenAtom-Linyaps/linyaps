/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_REPO_REPO_H_
#define LINGLONG_SRC_MODULE_REPO_REPO_H_

#include <QString>

#include "module/util/result.h"

namespace linglong {

namespace package {
class Bundle;
class Ref;
} // namespace package

namespace repo {

class Repo
{
public:
    virtual linglong::util::Error importDirectory(const package::Ref &ref, const QString &path) = 0;

    virtual linglong::util::Error import(const package::Bundle &bundle) = 0;

    virtual linglong::util::Error exportBundle(package::Bundle &bundle) = 0;

    virtual std::tuple<linglong::util::Error, QList<package::Ref>> list(const QString &filter) = 0;

    virtual std::tuple<linglong::util::Error, QList<package::Ref>> query(const QString &filter) = 0;

    virtual linglong::util::Error push(const package::Ref &ref, bool force) = 0;

    virtual linglong::util::Error push(const package::Ref &ref) = 0;

    virtual linglong::util::Error push(const package::Bundle &bundle, bool force) = 0;

    virtual linglong::util::Error pull(const package::Ref &ref, bool force) = 0;

    virtual QString rootOfLayer(const package::Ref &ref) = 0;

    virtual package::Ref latestOfRef(const QString &appId, const QString &appVersion) = 0;
};

void registerAllMetaType();

} // namespace repo
} // namespace linglong

#endif /* LINGLONG_SRC_MODULE_REPO_REPO_H_ */
