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

#include <QMetaProperty>
#include <QObject>
#include <QDBusArgument>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QDBusMetaType>

#define Q_JSON_PARENT_KEY "7bdcaad1-2f27-4092-a5cf-4919ad4caf2b"

#define Q_JSON_CONSTRUCTOR(TYPE) \
public: \
    explicit TYPE(QObject *parent = nullptr) \
        : JsonSerialize(parent) \
    { \
    }

#define Q_JSON_PROPERTY(TYPE, PROP) \
    Q_JSON_ITEM_MEMBER(TYPE, PROP, PROP)

#define Q_JSON_ITEM_MEMBER(TYPE, PROP, MEMBER_NAME) \
public: \
    Q_PROPERTY(TYPE PROP MEMBER MEMBER_NAME READ get##PROP WRITE set##PROP); \
    TYPE get##PROP() const { return MEMBER_NAME; } \
    inline void set##PROP(const TYPE &val) \
    { \
        qDebug() << "call set" << #PROP << val; \
        MEMBER_NAME = val; \
    } \
\
public: \
    TYPE MEMBER_NAME = TYPE();

#define Q_JSON_PTR_PROPERTY(TYPE, PROP) \
    Q_JSON_ITEM_MEMBER_PTR(TYPE, PROP, PROP)

#define Q_JSON_ITEM_MEMBER_PTR(TYPE, PROP, MEMBER_NAME) \
public: \
    Q_PROPERTY(TYPE *PROP MEMBER MEMBER_NAME READ get##PROP WRITE set##PROP); \
    TYPE *get##PROP() const { return MEMBER_NAME; } \
    inline void set##PROP(TYPE *val) \
    { \
        MEMBER_NAME = val; \
    } \
\
public: \
    TYPE *MEMBER_NAME = nullptr;

template<typename T>
static T *fromVariant(const QVariant &v)
{
    auto map = v.toMap();
    auto parent = qvariant_cast<QObject *>(map[Q_JSON_PARENT_KEY]);
    auto m = new T(parent);
    auto mo = m->metaObject();
    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        auto k = mo->property(i).name();
        auto t = mo->property(i).type();
        if (QVariant::UserType == t) {
            switch (map[k].type()) {
            case QVariant::Map: {
                auto value = map[k].toMap();
                value[Q_JSON_PARENT_KEY] = QVariant::fromValue(m);
                m->setProperty(k, value);
                break;
            }
            default: {
                auto value = map[k].toList();
                value.push_front(QVariant::fromValue(m));
                m->setProperty(k, value);
                break;
            }
            }
        } else {
            auto value = map[k];
            m->setProperty(k, value);
        }
    }
    return m;
}

template<typename T>
inline void qJsonRegister()
{
}

#define Q_JSON_DECLARE_PTR_METATYPE(TYPE) \
    Q_DECLARE_METATYPE(TYPE *) \
    template<> \
    inline TYPE *qvariant_cast(const QVariant &v) \
    { \
        return fromVariant<TYPE>(v); \
    } \
\
    class TYPE##List : public QList<QPointer<TYPE>> \
    { \
    public: \
        inline ~TYPE##List() \
        { \
            for (auto &c : *this) { \
                if (c->parent() == nullptr) { \
                    c->deleteLater(); \
                } \
            } \
        } \
\
        static void registerMetaType(); \
\
        friend QDBusArgument &operator<<(QDBusArgument &argument, const TYPE##List &message); \
        friend const QDBusArgument &operator>>(const QDBusArgument &argument, TYPE##List &message); \
    }; \
    Q_DECLARE_METATYPE(TYPE##List) \
\
    inline void TYPE##List::registerMetaType() \
    { \
        if (false) \
            qRegisterMetaType<TYPE##List>("TYPE##List"); \
        qDBusRegisterMetaType<TYPE##List>(); \
    } \
    template<> \
    inline void qJsonRegister<TYPE>() \
    { \
        TYPE##List::registerMetaType(); \
\
        qRegisterMetaType<TYPE *>(); \
\
        QMetaType::registerConverter<QVariantMap, TYPE *>([](const QVariantMap &v) -> TYPE * { \
            return fromVariant<TYPE>(v); \
        }); \
        QMetaType::registerConverter<TYPE *, QJsonValue>([](TYPE *m) -> QJsonValue { \
            return toVariant(m).toJsonValue(); \
        }); \
\
        QMetaType::registerConverter<QVariantList, TYPE##List>([](QVariantList list) -> TYPE##List { \
            TYPE##List mountList; \
            auto parent = qvariant_cast<QObject *>(list.value(0)); \
            if (parent) { \
                list.pop_front(); \
            } \
            for (const auto &v : list) { \
                auto map = v.toMap(); \
                map[Q_JSON_PARENT_KEY] = QVariant::fromValue(parent); \
                mountList.push_back(fromVariant<TYPE>(map)); \
            } \
            return mountList; \
        }); \
\
        QMetaType::registerConverter<TYPE##List, QJsonValue>([](const TYPE##List &valueList) -> QJsonValue { \
            QVariantList list; \
            for (auto v : valueList) { \
                list.push_back(toVariant(v.data())); \
            } \
            return QVariant::fromValue(list).toJsonValue(); \
        }); \
    } \
\
    inline QDBusArgument &operator<<(QDBusArgument &argument, const TYPE##List &message) \
    { \
        argument.beginStructure(); \
        argument.beginArray(qMetaTypeId<QByteArray>()); \
        for (const auto &item : message) { \
            argument << TYPE::dump<TYPE>(item.data()); \
        } \
        argument.endArray(); \
        argument.endStructure(); \
        return argument; \
    } \
\
    inline const QDBusArgument &operator>>(const QDBusArgument &argument, TYPE##List &message) \
    { \
        message.clear(); \
\
        argument.beginStructure(); \
\
        argument.beginArray(); \
        while (!argument.atEnd()) { \
            QByteArray buf; \
            argument >> buf; \
            auto v = QJsonDocument::fromJson(buf).toVariant(); \
            auto c = QPointer<TYPE>(fromVariant<TYPE>(v)); \
            message.append(c); \
        } \
\
        argument.endArray(); \
\
        argument.endStructure(); \
        return argument; \
    }

template<typename T>
QVariant toVariant(const T *m)
{
    QVariantMap map;
    //    auto obj = qobject_cast<T *>(m);
    //    map[Q_JSON_PARENT_KEY] = QVariant::fromValue(obj);
    //    qCritical() << "toVariant" << map[Q_JSON_PARENT_KEY];
    auto mo = m->metaObject();
    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        auto k = mo->property(i).name();
        map[k] = m->property(k).toJsonValue();
    }
    return map;
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

    template<typename T>
    static QJsonValue toJson(T *m)
    {
        return toVariant<T>(m).toJsonValue();
    }

    template<typename T>
    static QByteArray dump(T *m)
    {
        QJsonDocument doc(toJson(m).toObject());
        return doc.toJson();
    }
};
