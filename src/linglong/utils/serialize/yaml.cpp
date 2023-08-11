/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/utils/serialize/yaml.h"

#include <mutex>

namespace linglong::utils::serialize::yaml {
int init()
{
    static std::once_flag flag;
    std::call_once(flag, []() {
        QMetaType::registerConverter<QVariantMap, YAML::Node>(
                [](const QVariantMap &in) -> YAML::Node {
                    YAML::Node node;
                    node = in;
                    return node;
                });
        QMetaType::registerConverter<YAML::Node, QVariantMap>(
                [](const YAML::Node &in) -> QVariantMap {
                    return in.as<QVariantMap>();
                });
        QMetaType::registerConverter<QVariantList, YAML::Node>(
                [](const QVariantList &in) -> YAML::Node {
                    YAML::Node node;
                    node = in;
                    return node;
                });
        QMetaType::registerConverter<YAML::Node, QVariantList>(
                [](const YAML::Node &in) -> QVariantList {
                    return in.as<QVariantList>();
                });
    });
    return 0;
}
} // namespace linglong::utils::serialize::yaml
