/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/utils/serialize/json.h"

#include <qdbusargument.h>

namespace linglong::utils::serialize {
namespace {
static QVariant decodeQDBusArgument(const QVariant &v)
{
    if (!v.canConvert<QDBusArgument>()) {
        return v;
    }

    const QDBusArgument &complexType = v.value<QDBusArgument>();
    switch (complexType.currentType()) {
    case QDBusArgument::MapType: {
        QVariantMap list;
        complexType >> list;
        QVariantMap res;
        for (auto iter = list.begin(); iter != list.end(); iter++) {
            res[iter.key()] = decodeQDBusArgument(iter.value());
        }
        return res;
    }
    case QDBusArgument::ArrayType: {
        QVariantList list;
        complexType >> list;
        QVariantList res;
        res.reserve(list.size());
        for (const auto &item : std::as_const(list)) {
            res << decodeQDBusArgument(item);
        }
        return res;
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
    for (auto it = map.constBegin(); it != map.end(); it++) {
        newMap.insert(it.key(), decodeQDBusArgument(it.value()));
    }
    return QJsonObject::fromVariantMap(newMap);
}
} // namespace linglong::utils::serialize
