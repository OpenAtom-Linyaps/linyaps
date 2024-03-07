/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/utils/command/ocppi-helper.h"

namespace linglong::utils::command {

// 添加一个mount到config
void AddMount(ocppi::runtime::config::types::Config &config,
              const QString &source,
              const QString &destination,
              const QStringList &options,
              const QString &bind)
{
    ocppi::runtime::config::types::Mount m;
    m.type = bind.toStdString();
    m.source = source.toStdString();
    m.destination = destination.toStdString();

    std::vector<std::string> opts;
    for (auto opt : options) {
        opts.push_back(opt.toStdString());
    }
    m.options = opts;

    if (config.mounts.has_value()) {
        config.mounts->push_back(m);
    } else {
        config.mounts = { m };
    }
}

std::map<AnnotationKey, std::string> AnnotationMap{
    { AnnotationKey::MountRootfsComments, "dev.linglong.mount.rootfs.comments" },
};

// 添加一个annotation到config
void AddAnnotation(ocppi::runtime::config::types::Config &config,
                   const AnnotationKey &key,
                   const QString &val)
{

    std::pair a = { AnnotationMap[key], val.toStdString() };
    if (config.annotations.has_value()) {
        config.annotations->insert(a);
    } else {
        config.annotations = { a };
    }
}

} // namespace linglong::utils::command
