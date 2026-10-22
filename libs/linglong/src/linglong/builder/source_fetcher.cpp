/*
 * SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "source_fetcher.h"

#include "linglong/builder/builder_script.h"
#include "linglong/common/formatter.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/log/log.h"

#include <QDir>

namespace linglong::builder {

auto SourceFetcher::fetch(QDir destination) noexcept -> utils::error::Result<void>
{
    LINGLONG_TRACE("fetch source");

    if (!destination.mkpath(".")) {
        return LINGLONG_ERR(destination.absolutePath().toStdString()
                            + "source directory failed to create.");
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

    auto sourceName = getSourceName();
    if (sourceName.isEmpty() || sourceName == QLatin1String(".")
        || sourceName == QLatin1String("..") || sourceName.contains(QLatin1Char('/'))
        || sourceName.contains(QLatin1Char('\\'))) {
        return LINGLONG_ERR("invalid source name '" + sourceName.toStdString()
                            + "': expected a non-empty single filename");
    }

    auto scriptFile = findBuilderScript("fetch-" + source.kind + "-source");
    if (!scriptFile) {
        return LINGLONG_ERR(scriptFile);
    }
    if (source.kind == "git") {
        m_cmd->setEnv("GIT_SUBMODULES", source.submodules.value_or(true) ? "true" : "");
    }
    auto output = m_cmd->exec(
      std::vector<std::string>{ scriptFile->string(),
                                destination.absoluteFilePath(sourceName).toStdString(),
                                *source.url,
                                source.kind == "git" ? *source.commit : *source.digest,
                                this->cacheDir.absolutePath().toStdString() });
    if (!output.has_value()) {
        LogE("output error: {}", output.error());
        return LINGLONG_ERR("stderr:", output);
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
    LogE("missing name and url field");
    Q_ASSERT(false);
    return "unknown";
}

SourceFetcher::SourceFetcher(api::types::v1::BuilderProjectSource source, const QDir &cacheDir)
    : cacheDir(cacheDir)
    , source(std::move(source))
{
    if (this->cacheDir.mkpath(".")) {
        return;
    }

    LogE("mkpath {} failed", this->cacheDir.absolutePath().toStdString());
    Q_ASSERT(false);
}

} // namespace linglong::builder
