/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_QSERIALIZER_DEPRECATED_H_
#define LINGLONG_SRC_MODULE_UTIL_QSERIALIZER_DEPRECATED_H_

#include "module/util/qserializer/qserializer.h"

#include <qcompilerdetection.h>

#include <QFile>
#include <QJsonDocument>

class QObject;
typedef QObject JsonSerialize;
typedef QObject Serialize;
#define Q_JSON_CONSTRUCTOR Q_SERIALIZE_CONSTRUCTOR
#define Q_SERIALIZE_CONSTRUCTOR(type)      \
  public:                                  \
  explicit type(QObject *parent = nullptr) \
      : Serialize(parent)                  \
  {                                        \
  }
#define Q_JSON_PROPERTY Q_SERIALIZE_PROPERTY
#define Q_JSON_PTR_PROPERTY(type, prop) Q_SERIALIZE_PROPERTY(QSharedPointer<type>, prop)
#define Q_SERIALIZE_PROPERTY(type, prop) Q_SERIALIZE_ITEM_MEMBER(type, prop, prop)
#define Q_JSON_ITEM_MEMBER Q_SERIALIZE_ITEM_MEMBER
#define Q_SERIALIZE_ITEM_MEMBER(type, prop, memberName) \
  public:                                               \
  Q_PROPERTY(type prop MEMBER memberName);              \
  type memberName;

#define Q_JSON_DECLARE_PTR_METATYPE(type) Q_DECLARE_SERIALIZER(type);
#define Q_SERIALIZE_DECLARE_LIST_MAP(type) Q_DECLARE_SERIALIZER(type);
#define Q_JSON_DECLARE_PTR_METATYPE_NM(ns, type) \
  namespace ns {                                 \
  Q_DECLARE_SERIALIZER_INIT(type);               \
  }

namespace linglong {
namespace util {

template<typename T>
QSharedPointer<T> loadJson(const QString &filepath)
{
    QFile jsonFile(filepath);
    jsonFile.open(QIODevice::ReadOnly);
    auto json = QJsonDocument::fromJson(jsonFile.readAll());
    return json.toVariant().value<QSharedPointer<T>>();
}

template<typename T>
QSharedPointer<T> loadJsonString(const QString &jsonString)
{
    auto json = QJsonDocument::fromJson(jsonString.toLocal8Bit());
    return json.toVariant().value<QSharedPointer<T>>();
}

template<typename T>
QSharedPointer<T> loadJsonBytes(const QByteArray &jsonString)
{
    auto json = QJsonDocument::fromJson(jsonString);
    return json.toVariant().value<QSharedPointer<T>>();
}

} // namespace util
} // namespace linglong

#endif
