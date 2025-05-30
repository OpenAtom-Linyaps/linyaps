/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "source_fetcher.h"

#include "configure.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/global/initialize.h"

#include <QDir>
#include <QTemporaryDir>

namespace linglong::builder {

auto SourceFetcher::fetch(QDir destination) noexcept -> utils::error::Result<void>
{
    LINGLONG_TRACE("fetch source");

    if (!destination.mkpath(".")) {
        return LINGLONG_ERR(destination.absolutePath() + "source directory failed to create.");
    }

    if (this->source.kind != "git" && this->source.kind != "dsc" && this->source.kind != "file"
        && this->source.kind != "archive") {
        return LINGLONG_ERR("unknown source kind");
    }
    if (!source.url) {
        return LINGLONG_ERR("URL is missing");
    }
    if (this->source.kind == "git") {
        if (!source.commit) {
            return LINGLONG_ERR("digest missing");
        }
    } else {
        if (!source.digest) {
            return LINGLONG_ERR("digest missing");
        }
    }

    auto scriptName = QString("fetch-%1-source").arg(source.kind.c_str());
    // 如果二进制安装在系统目录中，优先使用系统中安装的脚本文件（便于用户更改），否则使用二进制内嵌的脚本（便于开发调试）
    auto scriptFile = QDir(LINGLONG_LIBEXEC_DIR).filePath(scriptName);
    auto useInstalledFile = utils::global::linglongInstalled() && QFile(scriptFile).exists();
    QScopedPointer<QTemporaryDir> dir;
    if (!useInstalledFile) {
        dir.reset(new QTemporaryDir);
        // 便于在执行失败时进行调试
        dir->setAutoRemove(false);
        scriptFile = dir->filePath(scriptName);
        qDebug() << "Dumping " << scriptName << "from qrc to" << scriptFile;
        QFile::copy(":/scripts/" + scriptName, scriptFile);
    }
    auto output = utils::command::Exec(
      "sh",
      {
        scriptFile,
        destination.absoluteFilePath(getSourceName()),
        QString::fromStdString(*source.url),
        QString::fromStdString(source.kind == "git" ? *source.commit : *source.digest),
        this->cacheDir.absolutePath(),
      });
    if (!output) {
        qDebug() << "output error:" << output.error();
        return LINGLONG_ERR("stderr:", output);
    }

    if (!dir.isNull()) {
        dir->remove();
    }
    return LINGLONG_OK;
}

// 如果source有name字段使用name字段，否则使用url的filename
QString SourceFetcher::getSourceName()
{
    if (source.name.has_value()) {
        return QString::fromStdString(*source.name);
    }
    if (source.url.has_value()) {
        QUrl url(source.url->c_str());
        return url.fileName();
    }
    qCritical() << "missing name and url field";
    Q_ASSERT(false);
    return "unknown";
}

SourceFetcher::SourceFetcher(api::types::v1::BuilderProjectSource source,
                             api::types::v1::BuilderConfig cfg,
                             const QDir &cacheDir)
    : cacheDir(cacheDir)
    , source(std::move(source))
    , cfg(std::move(cfg))
{
    if (this->cacheDir.mkpath(".")) {
        return;
    }

    qCritical() << "mkpath" << this->cacheDir << "failed";
    Q_ASSERT(false);
}

} // namespace linglong::builder
