/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_QSERIALIZER_YAML_H
#define LINGLONG_SRC_MODULE_UTIL_QSERIALIZER_YAML_H

#include "linglong/util/error.h"
#include "linglong/util/qserializer/qserializer.h"

#include <yaml-cpp/yaml.h>

#include <QFile>
#include <QVariantList>
#include <QVariantMap>

namespace linglong {
namespace util {

std::tuple<QVariantMap, Error> mapFromYAML(const QByteArray &bytes);
std::tuple<QVariantList, Error> listFromYAML(const QByteArray &bytes);

template<typename T>
typename std::enable_if<!isQList<T>::value, std::tuple<T, Error>>::type
fromYAML(const QByteArray &bytes)
{
    auto [result, err] = mapFromYAML(bytes);
    if (err) {
        return std::make_tuple<T, Error>({}, WrapError(err, ""));
    }
    QVariant v = result;
    if (!v.canConvert<T>()) {
        return std::make_tuple<T, Error>(
          {},
          NewError(-1,
                   QString("Failed to convert QVariantMap to %1.")
                     .arg(QString(QMetaType::fromType<T>().name()))));
    }
    return { v.value<T>(), {} };
}

template<typename T>
std::tuple<T, Error> fromYAML(const QString &filePath)
{
    QFile file(filePath);
    file.open(QIODevice::ReadOnly);
    auto bytes = file.readAll();
    return fromYAML<T>(bytes);
}

template<typename T>
typename std::enable_if<isQList<T>::value, std::tuple<T, Error>>::type
fromYAML(const QByteArray &bytes)
{
    auto result = listFromYAML(bytes);
    auto err = std::get<1>(result);
    if (err) {
        return { {}, WrapError(err, "") };
    }
    QVariant v = std::get<0>(result);
    if (!v.canConvert<T>()) {
        return { {},
                 NewError(-1,
                          QString("Failed to convert QVariantMap to %1.")
                            .arg(QString(QMetaType::fromType<T>().name()))) };
    }
    return { v.value<T>(), {} };
}

template<typename T>
std::tuple<QByteArray, Error> toYAML(const T &x)
{
    auto v = QVariant::fromValue<T>(x);
    if (!v.template canConvert<QVariantMap>()) {
        return std::make_tuple<QByteArray, Error>(
          {},
          NewError(-1,
                   QString("Failed to convert %1 to QVariantMap.")
                     .arg(QString(QMetaType::fromType<T>().name()))));
    }

    v = v.toMap();
    if (!v.template canConvert<YAML::Node>()) {
        return std::make_tuple<QByteArray, Error>(
          {},
          NewError(-1, "Failed to convert QVariantMap to YAML::Node."));
    }

    auto node = v.template value<YAML::Node>();
    if (!node.IsMap()) {
        return std::make_tuple<QByteArray, Error>(
          {},
          NewError(-1, "Failed to convert QVariantList to YAML::Node as a map."));
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
                            .arg(QString(QMetaType::fromType<T>().name()))) };
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
