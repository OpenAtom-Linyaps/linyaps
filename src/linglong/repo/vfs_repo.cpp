/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "vfs_repo.h"

#include "linglong/util/erofs.h"
#include "linglong/util/file.h"
#include "linglong/util/runner.h"
#include "linglong/util/sysinfo.h"
#include "linglong/util/version/version.h"
#include "linglong/util/xdg.h"

#include <utility>

namespace linglong {
namespace repo {

/*!
 * ${LINGLONG_ROOT}/vfs/ is the root of vfs repo.
 * ${LINGLONG_ROOT}/vfs/layers for host mount point.
 * ${LINGLONG_ROOT}/vfs/blobs for erofs image.
 */

class VfsRepoPrivate
{
public:
    explicit VfsRepoPrivate(QString repoRootPath)
        : repoRootPath(std::move(repoRootPath))
    {
        this->repoRootPath += "/vfs";
    }

    // mount source.erofs {mount_point}
    static util::Error mount(const QString &src, const QString &mountPoint)
    {
        // mount.erofs-unsafe just a test suid for wrap mount
        return util::Exec("mount.erofs-unsafe", { src, mountPoint });
    }

    QString repoRootPath;
};

VfsRepo::VfsRepo(const QString &path)
    : dd_ptr(new VfsRepoPrivate(path))
{
}

VfsRepo::~VfsRepo() = default;

util::Error VfsRepo::importDirectory(const package::Ref &ref, const QString &path)
{
    return {};
}

util::Error VfsRepo::import(const package::Bundle &bundle)
{
    return {};
}

util::Error VfsRepo::exportBundle(package::Bundle &bundle)
{
    return {};
}

std::tuple<util::Error, QList<package::Ref>> VfsRepo::list(const QString &filter)
{
    return { NewError(-1, "Not Implemented"), {} };
}

std::tuple<util::Error, QList<package::Ref>> VfsRepo::query(const QString &filter)
{
    return {};
}

util::Error VfsRepo::push(const package::Ref &ref, bool force)
{
    return {};
}

util::Error VfsRepo::push(const package::Ref &ref)
{
    return {};
}

util::Error VfsRepo::push(const package::Bundle &bundle, bool force)
{
    return {};
}

util::Error VfsRepo::pull(const package::Ref &ref, bool force)
{
    return {};
}

/*!
 * prepare the path before request for layer
 * @param ref
 * @return
 */
QString VfsRepo::rootOfLayer(const package::Ref &ref)
{
    Q_D(VfsRepo);

    auto mountPoint = QStringList{ util::userRuntimeDir().canonicalPath(),
                                   "linglong",
                                   "vfs",
                                   "layers",
                                   ref.appId,
                                   ref.version,
                                   ref.arch }
                              .join(QDir::separator());
    util::ensureDir(mountPoint);

    // TODO: parse form meta data
    auto sourcePath =
            QStringList{ util::getLinglongRootPath(), "layers", ref.appId, ref.version, ref.arch }
                    .join(QDir::separator());
    QString hash(
            QCryptographicHash::hash(sourcePath.toLocal8Bit(), QCryptographicHash::Md5).toHex());
    auto source = QStringList{ d->repoRootPath, "blobs", hash }.join(QDir::separator());

    qDebug() << "erofs::mount" << source << mountPoint;
    // FIXME: must umount after app exit
    erofs::mount(source, mountPoint);

    return mountPoint;
}

package::Ref VfsRepo::latestOfRef(const QString &appId, const QString &appVersion)
{
    // TODO: parse form meta data
    auto latestVersionOf = [](const QString &appId) {
        auto localRepoRoot = util::getLinglongRootPath() + "/layers" + "/" + appId;

        QDir appRoot(localRepoRoot);

        // found latest
        if (appRoot.exists("latest")) {
            return appRoot.absoluteFilePath("latest");
        }

        appRoot.setSorting(QDir::Name | QDir::Reversed);
        auto verDirs = appRoot.entryList(QDir::NoDotAndDotDot | QDir::Dirs);
        auto available = verDirs.value(0);
        for (auto item : verDirs) {
            linglong::util::AppVersion versionIter(item);
            linglong::util::AppVersion dstVersion(available);
            if (versionIter.isValid() && versionIter.isBigThan(dstVersion)) {
                available = item;
            }
        }
        qDebug() << "available version" << available << appRoot << verDirs;
        return available;
    };

    // 未指定版本使用最新版本，指定版本下使用指定版本
    QString version;
    if (!appVersion.isEmpty()) {
        version = appVersion;
    } else {
        version = latestVersionOf(appId);
    }
    auto ref = appId + "/" + version + "/" + util::hostArch();
    return package::Ref(ref);
}

} // namespace repo
} // namespace linglong
