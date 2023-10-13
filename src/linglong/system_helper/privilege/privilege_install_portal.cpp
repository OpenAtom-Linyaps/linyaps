/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "privilege_install_portal.h"

#include "linglong/dbus_ipc/package_manager_param.h"
#include "linglong/package/ref.h"
#include "linglong/util/file.h"
#include "linglong/util/qserializer/yaml.h"

#include <QDBusError>
#include <QDir>
#include <QDirIterator>

namespace linglong {
namespace system {
namespace helper {

QSERIALIZER_IMPL(FilePortalRule);
QSERIALIZER_IMPL(PortalRule);

const char *PrivilegePortalRule = R"MLS00(
whiteList:
  - org.deepin.screen-recorder
  - org.deepin.calendar
  - org.deepin.compressor
# TODO: use org.deepin.calendar instead
  - org.dde.calendar
  - org.deepin.downloader
# the target must be absolute path
fileRuleList:
  - source: files/lib/dde-dock/plugins/*.so
    target: /usr/lib/dde-dock/plugins
  - source: files/share/applications/context-menus/*.conf
    target: /usr/share/applications/context-menus
  - source: entries/systemd/user/*.service
    target: /etc/systemd/user
  - source: entries/systemd/user/*.timer
    target: /etc/systemd/user
  - source: files/etc/browser/native-messaging-hosts/*
    target: /etc/browser/native-messaging-hosts
  - source: files/libexec/openconnect/*
    target: /usr/libexec/openconnect
#  - source: files/share/glib-2.0/schemas/*.xml
#    target: /usr/share/glib-2.0/schemas
)MLS00";

static bool hasPrivilege(const QString &ref, const QStringList &whiteList)
{
    for (const auto &packageId : whiteList) {
        if (package::Ref(ref).appId == packageId) {
            return true;
        }
    }
    return false;
}

static void rebuildFileRule(const QString &installPath,
                            const QString &ref,
                            QSharedPointer<const FilePortalRule> rule)
{
    QDir sourceDir(installPath);

    qDebug() << "rebuild file rule" << installPath << ref << rule;
    QDirIterator iter(installPath,
                      QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot,
                      QDirIterator::Subdirectories);
    while (iter.hasNext()) {
        iter.next();
        const auto relativeFilePath = sourceDir.relativeFilePath(iter.filePath());
        if (QDir::match(rule->source, relativeFilePath)) {
            const auto targetFilePath = rule->target + QDir::separator() + iter.fileName();
            // TODO: save file to db and remove when uninstall
            QFileInfo targetFileInfo(targetFilePath);
            // FIXME: if another version is install, need an conflict or force, just remove is not
            // so safe
            if (targetFileInfo.isSymLink()) {
                qWarning() << "remove" << targetFilePath << "-->"
                           << QFile(targetFilePath).symLinkTarget();
                QFile::remove(targetFilePath);
            }
            qDebug() << "link file" << iter.filePath() << targetFilePath;
            if (!QFile::link(iter.filePath(), targetFilePath)) {
                qWarning() << "link failed";
            }
        }
    }
}

static void ruinFileRule(const QString &installPath,
                         const QString &ref,
                         QSharedPointer<const FilePortalRule> rule)
{
    QDir sourceDir(installPath);

    qDebug() << "ruin file rule" << installPath << ref << rule;
    QDirIterator iter(installPath,
                      QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot,
                      QDirIterator::Subdirectories);
    while (iter.hasNext()) {
        iter.next();
        const auto relativeFilePath = sourceDir.relativeFilePath(iter.filePath());
        if (QDir::match(rule->source, relativeFilePath)) {
            const auto targetFilePath = rule->target + QDir::separator() + iter.fileName();
            qDebug() << "remove link file" << iter.filePath() << targetFilePath;
            const auto checkSource = QFile::symLinkTarget(targetFilePath);
            if (checkSource != iter.filePath()) {
                qWarning() << "ignore file not link to source version";
            } else {
                QFile::remove(targetFilePath);
            }
        }
    }
}

util::Error rebuildPrivilegeInstallPortal(const QString &installPath,
                                          const QString &ref,
                                          const QVariantMap & /*options*/)
{
    YAML::Node doc = YAML::Load(PrivilegePortalRule);
    auto privilegePortalRule =
      QVariant::fromValue<YAML::Node>(doc).value<QSharedPointer<PortalRule>>();

    if (!hasPrivilege(ref, privilegePortalRule->whiteList)) {
        return NewError(QDBusError::AccessDenied, "No Privilege Package");
    }

    for (const auto &rule : privilegePortalRule->fileRuleList) {
        if (!QDir::isAbsolutePath(rule->target)) {
            qWarning() << "target must be absolute path" << rule->target;
            continue;
        }
        rebuildFileRule(installPath, ref, rule);
    }

    return Success();
}

util::Error ruinPrivilegeInstallPortal(const QString &installPath,
                                       const QString &ref,
                                       const QVariantMap &options)
{
    YAML::Node doc = YAML::Load(PrivilegePortalRule);
    auto privilegePortalRule =
      QVariant::fromValue<YAML::Node>(doc).value<QSharedPointer<PortalRule>>();

    if (options.contains(linglong::util::kKeyDelData)) {
        const auto appLinglongPath = options[linglong::util::kKeyDelData].toString();
        linglong::util::removeDir(appLinglongPath);
    }
    if (!hasPrivilege(ref, privilegePortalRule->whiteList)) {
        return NewError(QDBusError::AccessDenied, "No Privilege Package");
    }

    for (const auto &rule : privilegePortalRule->fileRuleList) {
        if (!QDir::isAbsolutePath(rule->target)) {
            qWarning() << "target must be absolute path" << rule->target;
            continue;
        }
        ruinFileRule(installPath, ref, rule);
    }

    return Success();
}

} // namespace helper
} // namespace system
} // namespace linglong
