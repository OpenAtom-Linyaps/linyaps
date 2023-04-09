/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "vfs_repo.h"

#include <utility>

#include "module/util/runner.h"

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
    auto mountPoint =
            QStringList{ d->repoRootPath, "layers", ref.appId, ref.version, ref.arch }.join(
                    QDir::separator());
    auto source =
            QStringList{ d->repoRootPath, "blobs", ref.toSpecString() }.join(QDir::separator());

    VfsRepoPrivate::mount(source, mountPoint);

    return mountPoint;
}

package::Ref VfsRepo::latestOfRef(const QString &appId, const QString &appVersion)
{
    return package::Ref(QString());
}

} // namespace repo
} // namespace linglong