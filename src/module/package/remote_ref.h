/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_PACKAGE_REMOTE_REF_H_
#define LINGLONG_SRC_MODULE_PACKAGE_REMOTE_REF_H_

#include "module/package/ref_new.h"
#include "ref_new.h"

namespace linglong {
namespace package {
namespace refact {

// ${repo}/${channel}:${packageID}/${version}[/${arch}[/${module}]]
class RemoteRef : public Ref
{
public:
    explicit RemoteRef(const QString &str);

    explicit RemoteRef(const QString &appId,
                       const QString &version,
                       const QString &arch = defaults::arch,
                       const QString &module = defaults::module,
                       const QString &repo = defaults::repo,
                       const QString &channel = defaults::channel);

    explicit RemoteRef(const Ref &ref,
                       const QString &repo = defaults::repo,
                       const QString &channel = defaults::channel);

    QString toOStreeString() const;
    QString toString() const;
    bool isVaild() const;

    QString repo;
    QString channel;
};
} // namespace refact
} // namespace package
} // namespace linglong
#endif
