/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/utils/serialize/json.h"

#include <qdbusargument.h>

namespace linglong::utils::serialize {
namespace {
QVariant decodeQDBusArgument(const QVariant &v)
{
    if (!v.canConvert<QDBusArgument>()) {
        return v;
    }

    const QDBusArgument &complexType = v.value<QDBusArgument>();
    switch (complexType.currentType()) {
    case QDBusArgument::MapType: {
        QVariantMap list;
        complexType >> list;

        for (auto iter = list.begin(); iter != list.end(); iter++) {
            iter.value() = decodeQDBusArgument(iter.value());
        }

        return list;
    }
    case QDBusArgument::ArrayType: {
        QVariantList list;
        complexType >> list;

        for (auto &item : list) {
            item = decodeQDBusArgument(item);
        }

        return list;
    }
    default:
        Q_ASSERT(false);
        return QVariant{};
    }
}

} // namespace

QJsonObject QJsonObjectfromVariantMap(const QVariantMap &map) noexcept
{
    QVariantMap newMap;
    for (auto it = map.constBegin(); it != map.constEnd(); it++) {
        newMap.insert(it.key(), decodeQDBusArgument(it.value()));
    }
    return QJsonObject::fromVariantMap(newMap);
}
} // namespace linglong::utils::serialize
