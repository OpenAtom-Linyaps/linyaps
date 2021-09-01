/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QObject>
#include <QMetaProperty>
#include <QJsonDocument>

#define Q_JSON_PROPERTY(TYPE, PROP) \
    Q_JSON_ITEM_MEMBER(TYPE, PROP, PROP)

#define Q_JSON_ITEM_MEMBER(TYPE, PROP, MEMBER_NAME) \
public: \
    Q_PROPERTY(TYPE PROP MEMBER MEMBER_NAME READ get##PROP WRITE set##PROP); \
    TYPE get##PROP() const { return PROP; } \
    inline void s1et##PROP(QVariant v) \
    { \
        MEMBER_NAME = v.value<TYPE>(); \
    } \
    inline void set##PROP(TYPE &val) \
    { \
        qDebug() << "call set" << #PROP << val; \
        MEMBER_NAME = val; \
    } \
\
public: \
    TYPE MEMBER_NAME;

#define Q_JSON_DECLARE_PTR_METATYPE(TYPE) \
    typedef QList<TYPE *> TYPE##List; \
    Q_DECLARE_METATYPE(TYPE *) \
    Q_DECLARE_METATYPE(TYPE##List) \
    template<> \
    inline TYPE *qvariant_cast(const QVariant &v) \
    { \
        return fromVariant<TYPE>(v); \
    }

template<typename T>
static T *fromVariant(const QVariant &v)
{
    // FIXME: memory leak...
    auto m = new T;
    auto mo = m->metaObject();
    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        auto k = mo->property(i).name();
        auto value = v.toMap()[k];
        m->setProperty(k, value);
    }
    return m;
}

template<typename T>
QVariant toVariant(const T *m)
{
    QVariantMap v;
    auto mo = m->metaObject();
    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        auto k = mo->property(i).name();
        v[k] = m->property(k).toJsonValue();
    }
    return v;
}

class JsonSerialize : public QObject
{
    Q_OBJECT
protected:
    explicit JsonSerialize(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

public:
    QStringList keys() const
    {
        QStringList jsonKeys;
        auto mo = this->metaObject();
        for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
            jsonKeys.push_back(mo->property(i).name());
        }
        return jsonKeys;
    }

    template<class T>
    QJsonValue toJson() const
    {
        toVariant<T>(this).toJsonValue();
    }
};

template<typename To>
bool qJsonRegisterListConverter()
{
    QMetaType::registerConverter<QVariantList, QList<To *>>([](const QVariantList &list) -> QList<To *> {
        QList<To *> mountList;
        for (const auto &v : list) {
            mountList.push_back(fromVariant<To>(v));
        }
        return mountList;
    });

    QMetaType::registerConverter<QList<To *>, QJsonValue>([](QList<To *> valueList) -> QJsonValue {
        QVariantList list;
        for (auto v : valueList) {
            list.push_back(toVariant(v));
        }
        return QVariant::fromValue(list).toJsonValue();
    });
    return true;
}

template<typename To>
bool qJsonRegisterConverter()
{
    qRegisterMetaType<To *>();
    QMetaType::registerConverter<QVariantMap, To *>([](const QVariant &v) -> To * {
        return fromVariant<To>(v);
    });
    QMetaType::registerConverter<To *, QJsonValue>([](To *m) -> QJsonValue {
        return toVariant(m).toJsonValue();
    });
    return qJsonRegisterListConverter<To>();
}