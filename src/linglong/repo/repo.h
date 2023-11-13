/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_REPO_REPO_H_
#define LINGLONG_SRC_MODULE_REPO_REPO_H_

#include "linglong/util/error.h"

#include <QString>

namespace linglong {

namespace package {
class Bundle;
class Ref;
} // namespace package

namespace repo {

class Repo
{
public:
    Repo() = default;
    Repo(const Repo &) = delete;
    Repo(Repo &&) = delete;
    Repo &operator=(const Repo &) = delete;
    Repo &operator=(Repo &&) = delete;
    virtual ~Repo() = default;

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

} // namespace repo
} // namespace linglong

#endif /* LINGLONG_SRC_MODULE_REPO_REPO_H_ */
