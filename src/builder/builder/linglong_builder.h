/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_BUILDER_LINGLONG_BUILDER_H_
#define LINGLONG_SRC_BUILDER_BUILDER_LINGLONG_BUILDER_H_

#include "builder.h"
#include "module/package/bundle.h"
#include "module/util/status_code.h"
#include "project.h"

namespace linglong {
namespace builder {

class LinglongBuilder : public QObject, public Builder
{
    Q_OBJECT
public:
    linglong::util::Error config(const QString &userName, const QString &password) override;

    linglong::util::Error create(const QString &projectName) override;

    linglong::util::Error build() override;

    linglong::util::Error exportBundle(const QString &outputFilepath, bool useLocalDir) override;

    linglong::util::Error push(const QString &repoUrl,
                               const QString &repoName,
                               const QString &channel,
                               bool pushWithDevel) override;

    linglong::util::Error import() override;

    linglong::util::Error run() override;

    linglong::util::Error track() override;

    linglong::util::Error initRepo();

    linglong::util::Error buildFlow(Project *project);
};

// TODO: remove later
class message : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(message)
public:
    Q_JSON_PROPERTY(QString, type);
    Q_JSON_PROPERTY(int, pid);
    Q_JSON_PROPERTY(QString, arg0);
    Q_JSON_PROPERTY(int, wstatus);
    Q_JSON_PROPERTY(QString, information);
};
} // namespace builder
} // namespace linglong

#endif // LINGLONG_SRC_BUILDER_BUILDER_LINGLONG_BUILDER_H_
