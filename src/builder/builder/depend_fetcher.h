/*
 * Copyright (c) 2022. ${ORGANIZATION_NAME}. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_BUILDER_DEPEND_FETCHER_H_
#define LINGLONG_SRC_BUILDER_BUILDER_DEPEND_FETCHER_H_

#include <QObject>

namespace linglong {
namespace builder {

class Project;
class BuildDepend;
class DependFetcherPrivate;
class DependFetcher : public QObject
{
    Q_OBJECT
public:
    explicit DependFetcher(const BuildDepend &bd, Project *parent);
    ~DependFetcher() override;

    linglong::util::Error fetch(const QString &subPath, const QString &targetPath);

private:
    QScopedPointer<DependFetcherPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), DependFetcher)
};

} // namespace builder
} // namespace linglong

#endif // LINGLONG_SRC_BUILDER_BUILDER_DEPEND_FETCHER_H_
