/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_QSERIALIZER_QSERIALIZER_H
#define LINGLONG_SRC_MODULE_UTIL_QSERIALIZER_QSERIALIZER_H

// copy from https://github.com/black-desk/qserializer f1191887bb657eb154b942e5e8977ce257e3719d

#include <QDebug>
#include <QMap>
#include <QMetaObject>
#include <QMetaProperty>
#include <QMetaType>
#include <QSharedPointer>

#include <mutex>

template<typename T>
class QSerializer
{
public:
    static void registerConverters();

private:
    static const QMetaObject *const qObjectMetaObject;
    static const QMetaObject *const metaObject;
    static QVariantMap TtoQVariantMap(QSharedPointer<T> from);
    static QSharedPointer<T> QVariantMapToT(const QVariantMap &map);

    static QVariantList QListTToQVariantList(QList<QSharedPointer<T> > list);

    static QList<QSharedPointer<T> > QVariantListToQListT(QVariantList list);

    static QVariantMap QMapTToQVariantMap(QMap<QString, QSharedPointer<T> > map);
    static QMap<QString, QSharedPointer<T> > QVariantMapToQMapT(QVariantMap map);
};

template<typename T>
void QSerializer<T>::registerConverters()
{
    QMetaType::registerConverter<QSharedPointer<T>, QVariantMap>(TtoQVariantMap);
    QMetaType::registerConverter<QVariantMap, QSharedPointer<T> >(QVariantMapToT);

    QMetaType::registerConverter<QList<QSharedPointer<T> >, QVariantList>(QListTToQVariantList);
    QMetaType::registerConverter<QVariantList, QList<QSharedPointer<T> > >(QVariantListToQListT);

    QMetaType::registerConverter<QMap<QString, QSharedPointer<T> >, QVariantMap>(
            QMapTToQVariantMap);
    QMetaType::registerConverter<QVariantMap, QMap<QString, QSharedPointer<T> > >(
            QVariantMapToQMapT);
}

template<typename T>
const QMetaObject *const QSerializer<T>::qObjectMetaObject =
        QMetaType::fromType<QObject *>().metaObject();

template<typename T>
const QMetaObject *const QSerializer<T>::metaObject = QMetaType::fromType<T *>().metaObject();

template<typename T>
QVariantMap QSerializer<T>::TtoQVariantMap(QSharedPointer<T> from)
{
    auto ret = QVariantMap{};
    for (int i = qObjectMetaObject->propertyCount(); i < metaObject->propertyCount(); i++) {
        const char *k = metaObject->property(i).name();
        QVariant v = metaObject->property(i).read(from.data());
        if (v.canConvert<QString>()) {
            ret.insert(k, v);
            continue;
        }
        if (v.canConvert<QVariantList>()) {
            ret.insert(k, v.value<QVariantList>());
            continue;
        }
        if (v.canConvert<QVariantMap>()) {
            ret.insert(k, v.value<QVariantMap>());
            continue;
        }
        qWarning().noquote() << QString("Failed to insert \"%1\", maybe missing converter").arg(k);
    }
    return ret;
}

template<typename T>
QSharedPointer<T> QSerializer<T>::QVariantMapToT(const QVariantMap &map)
{
    QSharedPointer<T> ret(new T());
    for (int i = metaObject->propertyOffset(); i < metaObject->propertyCount(); i++) {
        QMetaProperty metaProp = metaObject->property(i);

        const char *metaPropName = metaProp.name();
        QVariantMap::ConstIterator it = map.find(metaPropName);
        if (it == map.end()) {
            continue;
        }

        if (!metaProp.write(ret.data(), it.value())) {
            qWarning().noquote()
                    << QString("Failed to write \"%1\", maybe missing converter").arg(metaPropName);
        }
    }
    return ret;
}

template<typename T>
QVariantList QSerializer<T>::QListTToQVariantList(QList<QSharedPointer<T> > list)
{
    auto ret = QVariantList{};
    for (auto const &item : list) {
        ret.push_back(TtoQVariantMap(item));
    }
    return ret;
}

template<typename T>
QList<QSharedPointer<T> > QSerializer<T>::QVariantListToQListT(QVariantList list)
{
    auto ret = QList<QSharedPointer<T> >{};
    for (auto const &item : list) {
        ret.push_back(QVariantMapToT(item.toMap()));
    }
    return ret;
}

template<typename T>
QVariantMap QSerializer<T>::QMapTToQVariantMap(QMap<QString, QSharedPointer<T> > map)
{
    auto ret = QVariantMap{};
    for (auto it = map.begin(); it != map.end(); it++) {
        ret.insert(it.key(), TtoQVariantMap(it.value()));
    }
    return ret;
}

template<typename T>
QMap<QString, QSharedPointer<T> > QSerializer<T>::QVariantMapToQMapT(QVariantMap map)
{
    auto ret = QMap<QString, QSharedPointer<T> >{};
    for (auto it = map.begin(); it != map.end(); it++) {
        ret.insert(it.key(), QVariantMapToT(it.value().toMap()));
    }
    return ret;
}

#define Q_DECLARE_SERIALIZER(T)            \
  Q_DECLARE_METATYPE(T *);                 \
  namespace QSerializerPrivateNamespace##T \
  {                                        \
    int init();                            \
    static int _ = init();                 \
  };

#define Q_REGISTER_SERIALIZER(T, ...)      \
  namespace QSerializerPrivateNamespace##T \
  {                                        \
    int init()                             \
    {                                      \
      static std::once_flag __flag;        \
      std::call_once(__flag, []() {        \
QSerializer<T>::registerConverters();      \
__VA_ARGS__;                               \
      });                                  \
      return 0;                            \
    }                                      \
  }

#endif
