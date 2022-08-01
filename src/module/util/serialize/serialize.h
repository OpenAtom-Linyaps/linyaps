/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_SERIALIZE_SERIALIZE_H
#define LINGLONG_SRC_MODULE_UTIL_SERIALIZE_SERIALIZE_H

#include <QMetaProperty>
#include <QObject>
#include <QDBusArgument>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QDBusMetaType>
#include <QDebug>
#include <QFile>

#define Q_SERIALIZE_PARENT_KEY "7bdcaad1-2f27-4092-a5cf-4919ad4caf2b"

#define Q_SERIALIZE_CONSTRUCTOR(TYPE) \
public: \
    explicit TYPE(QObject *parent = nullptr) \
        : Serialize(parent) \
    { \
    }

#define Q_SERIALIZE_PROPERTY(TYPE, PROP) Q_SERIALIZE_ITEM_MEMBER(TYPE, PROP, PROP)

#define Q_SERIALIZE_ITEM_MEMBER(TYPE, PROP, MEMBER_NAME) \
public: \
    Q_PROPERTY(TYPE PROP MEMBER MEMBER_NAME READ get##PROP WRITE set##PROP); \
    TYPE get##PROP() const \
    { \
        return MEMBER_NAME; \
    } \
    inline void set##PROP(const TYPE &val) \
    { \
        MEMBER_NAME = val; \
    } \
\
public: \
    TYPE MEMBER_NAME = TYPE();

#define Q_SERIALIZE_PTR_PROPERTY(TYPE, PROP) Q_SERIALIZE_ITEM_MEMBER_PTR(TYPE, PROP, PROP)

#define Q_SERIALIZE_ITEM_MEMBER_PTR(TYPE, PROP, MEMBER_NAME) \
public: \
    Q_PROPERTY(TYPE *PROP MEMBER MEMBER_NAME READ get##PROP WRITE set##PROP); \
    TYPE *get##PROP() const \
    { \
        return MEMBER_NAME; \
    } \
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
    auto parent = qvariant_cast<QObject *>(map[Q_SERIALIZE_PARENT_KEY]);
    auto m = new T(parent);
    auto mo = m->metaObject();
    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        auto k = mo->property(i).name();
        auto t = mo->property(i).type();
        if (QVariant::UserType == t) {
            switch (map[k].type()) {
            case QVariant::Map: {
                auto value = map[k].toMap();
                value[Q_SERIALIZE_PARENT_KEY] = QVariant::fromValue(m);
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

    m->onPostSerialize();
    return m;
}

template<typename LIST>
static LIST fromVariantList(const QVariant &l)
{
    LIST list;
    using TP = typename std::decay<decltype((*list.begin()).data())>::type;
    using T = typename std::remove_pointer<TP>::type;

    auto varList = l.toList();
    for (auto v : varList) {
        auto o = fromVariant<T>(v);
        list.push_back(o);
    }
    return list;
}

template<typename T>
inline void qSerializeRegister()
{
}

#define Q_SERIALIZE_DECLARE(TYPE) Q_SERIALIZE_DECLARE_LIST_MAP(TYPE)

#define Q_SERIALIZE_DECLARE_LIST_MAP(TYPE) \
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
        static void registerMetaType(); \
        friend QDBusArgument &operator<<(QDBusArgument &argument, const TYPE##List &message); \
        friend const QDBusArgument &operator>>(const QDBusArgument &argument, TYPE##List &message); \
    }; \
    typedef QMap<QString, QPointer<TYPE>> TYPE##StrMap; \
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
        argument.beginStructure(); \
        argument.beginArray(); \
        while (!argument.atEnd()) { \
            QByteArray buf; \
            argument >> buf; \
            auto v = QJsonDocument::fromJson(buf).toVariant(); \
            auto c = QPointer<TYPE>(fromVariant<TYPE>(v)); \
            message.append(c); \
        } \
        argument.endArray(); \
        argument.endStructure(); \
        return argument; \
    }

#define Q_SERIALIZE_REGISTER_TYPE(TYPE, TYPE_LIST, TYPE_STR_MAP) \
    Q_DECLARE_METATYPE(TYPE *) \
    Q_DECLARE_METATYPE(TYPE_LIST) \
    Q_DECLARE_METATYPE(TYPE_STR_MAP) \
    template<> \
    inline TYPE *qvariant_cast(const QVariant &v) \
    { \
        return fromVariant<TYPE>(v); \
    } \
    inline void TYPE_LIST::registerMetaType() \
    { \
        qDBusRegisterMetaType<TYPE_LIST>(); \
    } \
    template<> \
    inline void qSerializeRegister<TYPE>() \
    { \
        TYPE_LIST::registerMetaType(); \
        qRegisterMetaType<TYPE *>(); \
\
        QMetaType::registerConverter<QVariantMap, TYPE *>( \
            [](const QVariantMap &v) -> TYPE * { return fromVariant<TYPE>(v); }); \
        QMetaType::registerConverter<TYPE *, QJsonValue>( \
            [](TYPE *m) -> QJsonValue { return toVariant(m).toJsonValue(); }); \
\
        QMetaType::registerConverter<QVariantMap, TYPE_STR_MAP>([](QVariantMap variantMap) -> TYPE_STR_MAP { \
            TYPE_STR_MAP vMap; \
            auto parent = qvariant_cast<QObject *>(variantMap.take(Q_SERIALIZE_PARENT_KEY)); \
\
            for (const auto &key : variantMap.keys()) { \
                auto v = variantMap[key].toMap(); \
                v[Q_SERIALIZE_PARENT_KEY] = QVariant::fromValue(parent); \
                vMap[key] = fromVariant<TYPE>(v); \
            } \
            return vMap; \
        }); \
        QMetaType::registerConverter<TYPE_STR_MAP, QJsonValue>([](TYPE_STR_MAP map) -> QJsonValue { \
            QVariantMap variantList; \
            for (auto key : map.keys()) { \
                variantList[key] = toVariant(map[key].data()); \
            } \
            return QVariant::fromValue(variantList).toJsonValue(); \
        }); \
\
        QMetaType::registerConverter<QVariantList, TYPE_LIST>([](QVariantList list) -> TYPE_LIST { \
            TYPE_LIST vList; \
            auto parent = qvariant_cast<QObject *>(list.value(0)); \
            if (parent) { \
                list.pop_front(); \
            } \
            for (const auto &v : list) { \
                auto map = v.toMap(); \
                map[Q_SERIALIZE_PARENT_KEY] = QVariant::fromValue(parent); \
                vList.push_back(fromVariant<TYPE>(map)); \
            } \
            return vList; \
        }); \
        QMetaType::registerConverter<TYPE_LIST, QJsonValue>([](const TYPE_LIST &valueList) -> QJsonValue { \
            QVariantList list; \
            for (auto v : valueList) { \
                list.push_back(toVariant(v.data())); \
            } \
            return QVariant::fromValue(list).toJsonValue(); \
        }); \
    }

#define Q_SERIALIZE_DECLARE_METATYPE_NM(NM, TYPE) \
    namespace NM { \
    Q_SERIALIZE_DECLARE_LIST_MAP(TYPE) \
    } \
    Q_SERIALIZE_REGISTER_TYPE(NM::TYPE, NM::TYPE##List, NM::TYPE##StrMap)

#define Q_SERIALIZE_DECLARE_METATYPE(TYPE) \
    Q_SERIALIZE_DECLARE_LIST_MAP(TYPE) \
    Q_SERIALIZE_REGISTER_TYPE(TYPE, TYPE##List, TYPE##StrMap)

template<typename T>
QVariant toVariant(const T *m)
{
    QVariantMap map;
    //    auto obj = qobject_cast<T *>(m);
    //    map[Q_SERIALIZE_PARENT_KEY] = QVariant::fromValue(obj);
    if (nullptr == m) {
        return {};
    }

    auto mo = m->metaObject();
    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        auto k = mo->property(i).name();
        if (QVariant::fromValue((QObject *const)nullptr) == m->property(k)) {
            continue;
        }
        map[k] = m->property(k).toJsonValue();
    }
    return map;
}

#endif // LINGLONG_SRC_MODULE_UTIL_SERIALIZE_SERIALIZE_H
