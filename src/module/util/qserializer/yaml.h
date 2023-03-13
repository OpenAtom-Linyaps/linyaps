/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_QSERIALIZER_YAML_H
#define LINGLONG_SRC_MODULE_UTIL_QSERIALIZER_YAML_H

#include "module/util/error.h"

#include <yaml-cpp/yaml.h>

#include <QFile>
#include <QVariantList>
#include <QVariantMap>

namespace linglong {
namespace util {

std::tuple<QVariantMap, Error> mapFromYAML(const QByteArray &bytes);
std::tuple<QVariantList, Error> listFromYAML(const QByteArray &bytes);

template<typename T>
std::tuple<T, Error> fromYAML(const QString &filePath)
{
    QFile file(filePath);
    file.open(QIODevice::ReadOnly);
    auto bytes = file.readAll();
    return fromYAML<T>(bytes);
}

template<typename T>
std::tuple<T, Error> fromYAML(const QByteArray &bytes)
{
    auto result = mapFromYAML(bytes);
    auto err = std::get<1>(result);
    if (!err.success()) {
        return { {}, WrapError(err, "") };
    }
    QVariant v = std::get<0>(result);
    if (!v.canConvert<T>()) {
        return { {},
                 NewError(-1,
                          QString("Failed to convert QVariantMap to %1.")
                                  .arg(QMetaType::fromType<T>().name())) };
    }
    return { v.value<T>(), {} };
}

template<typename T>
std::tuple<QList<T>, Error> fromYAML(const QByteArray &bytes)
{
    auto result = listFromYAML(bytes);
    auto err = std::get<1>(result);
    if (!err.success()) {
        return { {}, WrapError(err, "") };
    }
    QVariant v = std::get<0>(result);
    if (!v.canConvert<T>()) {
        return { {},
                 NewError(-1,
                          QString("Failed to convert QVariantMap to %1.")
                                  .arg(QMetaType::fromType<T>().name())) };
    }
    return { v.value<T>(), {} };
}

template<typename T>
std::tuple<QByteArray, Error> toYAML(const T &x)
{
    QVariant v = x;
    if (!v.canConvert<QVariantMap>()) {
        return { {},
                 NewError(-1,
                          QString("Failed to convert %1 to QVariantMap.")
                                  .arg(QMetaType::fromType<T>().name())) };
    }

    v = v.toMap();
    if (!v.canConvert<YAML::Node>()) {
        return { {}, NewError(-1, "Failed to convert QVariantMap to YAML::Node.") };
    }

    auto node = v.value<YAML::Node>();
    if (!node.IsMap()) {
        return { {}, NewError(-1, "Failed to convert QVariantList to YAML::Node as a map.") };
    }

    YAML::Emitter emitter;
    emitter << node;
    return { QByteArray(emitter.c_str()), {} };
}

template<typename T>
std::tuple<QByteArray, Error> toYAML(const QList<T> &x)
{
    QVariant v = x;
    if (!v.canConvert<QVariantList>()) {
        return { {},
                 NewError(-1,
                          QString("Failed to convert %1 to QVariantList.")
                                  .arg(QMetaType::fromType<T>().name())) };
    }

    v = v.toList();
    if (!v.canConvert<YAML::Node>()) {
        return { {}, NewError(-1, "Failed to convert QVariantMap to YAML::Node.") };
    }

    auto node = v.value<YAML::Node>();
    if (!node.IsSequence()) {
        return { {}, NewError(-1, "Failed to convert QVariantList to YAML::Node as a sequence.") };
    }

    YAML::Emitter emitter;
    emitter << node;
    return { QByteArray(emitter.c_str()), {} };
}

namespace QSerializerPrivateNamespaceYAMLNode {
int init();
static int _ = init();
} // namespace QSerializerPrivateNamespaceYAMLNode

} // namespace util
} // namespace linglong

Q_DECLARE_METATYPE(YAML::Node);
#endif
