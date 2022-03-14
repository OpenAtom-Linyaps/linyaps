/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "xdg.h"

#include <QDir>
#include <QMap>
#include <QStandardPaths>

namespace linglong {
namespace util {

QDir userRuntimeDir()
{
    return QDir(QStandardPaths::standardLocations(QStandardPaths::RuntimeLocation).value(0));
}

QDir userCacheDir()
{
    return QDir(QStandardPaths::standardLocations(QStandardPaths::GenericCacheLocation).value(0));
}

//! https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#exec-variables
//! \param exec
//! \return
//! TODO: should compatibility with：
//!     env a=b c
QStringList parseExec(const QString &exec)
{
    // const auto Backtick = '`';
    const auto Quote = '"';
    const auto Space = ' ';
    const auto BackSlash = '\\';

    QStringList args;
    bool argBegin = false;
    bool inQuote = false;
    QString arg;
    for (int i = 0; i < exec.length(); ++i) {
        switch (exec.at(i).toLatin1()) {
        case BackSlash:
            if (inQuote) {
                inQuote = !inQuote;
                arg.push_back(exec.at(i));
                break;
            }

            argBegin = true;
            inQuote = true;
            break;
        case Quote:
            if (inQuote) {
                inQuote = !inQuote;
                arg.push_back(exec.at(i));
                break;
            }
            break;
        case Space:
            if (inQuote) {
                inQuote = !inQuote;
                arg.push_back(exec.at(i));
                break;
            }

            // terminal arg with space
            if (!argBegin) {
            } else {
                args.push_back(arg);
                arg = "";
            }
            argBegin = false;
            break;
        default:
            argBegin = true;
            arg.push_back(exec.at(i));
            break;
        }
    }
    if (!arg.isEmpty()) {
        args.push_back(arg);
    }
    return args;
}

QPair<QString, QString> parseEnvKeyValue(QString env, const QString &sep)
{
    QRegExp exp("(\\$\\{.*\\})");
    exp.setMinimal(true);
    exp.indexIn(env);

    for (auto const &envReplace : exp.capturedTexts()) {
        auto envKey = QString(envReplace).replace("$", "").replace("{", "").replace("}", "");
        auto envValue = qEnvironmentVariable(envKey.toStdString().c_str());
        env.replace(envReplace, envValue);
    }

    auto sepPos = env.indexOf(sep);

    if (sepPos < 0) {
        return {env, ""};
    }

    return {env.left(sepPos), env.right(env.length() - sepPos - 1)};
}

QStringList convertSpecialCharacters(const QStringList &args)
{
    QStringList retArgs;
    retArgs.clear();
    for (auto arg : args) {
        arg.replace(' ', QString("\\%1").arg(' '));
        arg.replace('"', QString("\\%1").arg('"'));
        retArgs.push_back(arg);
    }
    return retArgs;
}

static QMap<QString, QString> const USER_DIR_MAP = {
    {"desktop", QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)},
    {"documents", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)},
    {"downloads", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)},
    {"music", QStandardPaths::writableLocation(QStandardPaths::MusicLocation)},
    {"pictures", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)},
    {"picture", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)},
    {"videos", QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)},
    {"templates", ""},
    {"home", QStandardPaths::writableLocation(QStandardPaths::HomeLocation)},
    {"cache", QStandardPaths::writableLocation(QStandardPaths::CacheLocation)},
    {"config", QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)},
    {"data", QStandardPaths::writableLocation(QStandardPaths::DataLocation)},
    {"runtime", QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)},
    {"temp", QStandardPaths::writableLocation(QStandardPaths::TempLocation)},
    {"public_share", ""}};

QPair<bool, QString> getXdgDir(QString name)
{
    auto foundResult = USER_DIR_MAP.value(name.toLower(), "");
    if (!foundResult.isEmpty()) {
        QFileInfo fileInfo(foundResult);
        if (fileInfo.exists()) {
            return {true, foundResult};
        }
        return {false, foundResult};
    } else if (name.toLower() == "public_share") {
        // FIXME: need read user-dirs.dirs XDG_PUBLICSHARE_DIR
        return {true, QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.Public"};
    } else if (name.toLower() == "templates") {
        // FIXME: need read user-dirs.dirs XDG_TEMPLATES_DIR
        return {true, QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.Templates"};
    }
    return {false, ""};
}

QList<QString> getXdgUserDir()
{
    if (USER_DIR_MAP.isEmpty()) {
        return QList<QString>();
    }
    return USER_DIR_MAP.keys();
}

} // namespace util
} // namespace linglong
