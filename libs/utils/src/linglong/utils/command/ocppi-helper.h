/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "ocppi/runtime/config/types/Config.hpp"

#include <QStringList>

namespace linglong::utils::command {

enum class AnnotationKey { MountRootfsComments, MountRuntimeComments };

void AddMount(ocppi::runtime::config::types::Config &config,
              const QString &src,
              const QString &dist,
              const QStringList &options,
              const QString &bind = "bind");
void AddAnnotation(ocppi::runtime::config::types::Config &config,
                   const AnnotationKey &key,
                   const QString &val);

} // namespace linglong::utils::command
