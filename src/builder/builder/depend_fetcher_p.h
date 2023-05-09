/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_BUILDER_DEPEND_FETCHER_P_H_
#define LINGLONG_SRC_BUILDER_BUILDER_DEPEND_FETCHER_P_H_

#include "module/util/result.h"
#include "project.h"

#include <QObject>

namespace linglong {
namespace builder {

class DependFetcherPrivate
{
public:
    explicit DependFetcherPrivate(const BuildDepend &bd, Project *parent);

    ~DependFetcherPrivate() = default;

    // TODO: dependType should be removed, buildDepend include it
    package::Ref ref;
    Project *project;
    const BuildDepend *buildDepend;
    QString dependType;
};

} // namespace builder
} // namespace linglong

#endif // LINGLONG_SRC_BUILDER_BUILDER_DEPEND_FETCHER_P_H_
