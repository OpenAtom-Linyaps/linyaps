/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_QSERIALIZER_QSERIALIZER_H
#define LINGLONG_SRC_MODULE_UTIL_QSERIALIZER_QSERIALIZER_H

// copy from https://github.com/black-desk/qserializer d93f15d

#include <QDebug>
#include <QMap>
#include <QMetaObject>
#include <QMetaProperty>
#include <QMetaType>
#include <QSharedPointer>

template<typename T>
class QSerializer
{
public:
    static void registerConverters();

private:
    typedef QSharedPointer<T> P;

    static const QMetaObject *const qObjectMetaObject;
    static const QMetaObject *const metaObject;
    static QVariantMap PToQVariantMap(P from);
    static P QVariantMapToP(const QVariantMap &map);

    static QVariantList PListToQVariantList(QList<P> list);

    static QList<P> QVariantListToPList(QVariantList list);

    static QVariantMap PStrMapToQVariantMap(QMap<QString, P> map);
    static QMap<QString, P> QVariantMapToPStrMap(QVariantMap map);
};

template<typename T>
void QSerializer<T>::registerConverters()
{
    QMetaType::registerConverter<P, QVariantMap>(PToQVariantMap);
    QMetaType::registerConverter<QVariantMap, P>(QVariantMapToP);

    QMetaType::registerConverter<QList<P>, QVariantList>(PListToQVariantList);
    QMetaType::registerConverter<QVariantList, QList<P> >(QVariantListToPList);

    QMetaType::registerConverter<QMap<QString, P>, QVariantMap>(PStrMapToQVariantMap);
    QMetaType::registerConverter<QVariantMap, QMap<QString, P> >(QVariantMapToPStrMap);
}

template<typename T>
const QMetaObject *const QSerializer<T>::qObjectMetaObject =
        QMetaType::fromType<QObject *>().metaObject();

template<typename T>
const QMetaObject *const QSerializer<T>::metaObject = QMetaType::fromType<T *>().metaObject();

template<typename T>
QVariantMap QSerializer<T>::PToQVariantMap(P from)
{
    auto ret = QVariantMap{};
    if (from.isNull()) {
        return ret;
    }

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
        Q_ASSERT(false);
        qWarning().noquote() << QString("Failed to insert \"%1\", maybe missing converter").arg(k);
    }
    return ret;
}

template<typename T>
QSharedPointer<T> QSerializer<T>::QVariantMapToP(const QVariantMap &map)
{
    P ret(new T());
    for (int i = metaObject->propertyOffset(); i < metaObject->propertyCount(); i++) {
        QMetaProperty metaProp = metaObject->property(i);

        const char *metaPropName = metaProp.name();
        QVariantMap::ConstIterator it = map.find(metaPropName);
        if (it == map.end()) {
            continue;
        }

        if (!metaProp.write(ret.data(), it.value())) {
            Q_ASSERT(false);
            qWarning().noquote()
                    << QString("Failed to write \"%1\", maybe missing converter").arg(metaPropName);
        }
    }
    return ret;
}

template<typename T>
QVariantList QSerializer<T>::PListToQVariantList(QList<P> list)
{
    auto ret = QVariantList{};
    for (auto const &item : list) {
        ret.push_back(PToQVariantMap(item));
    }
    return ret;
}

template<typename T>
QList<QSharedPointer<T> > QSerializer<T>::QVariantListToPList(QVariantList list)
{
    auto ret = QList<P>{};
    for (auto const &item : list) {
        ret.push_back(QVariantMapToP(item.toMap()));
    }
    return ret;
}

template<typename T>
QVariantMap QSerializer<T>::PStrMapToQVariantMap(QMap<QString, P> map)
{
    auto ret = QVariantMap{};
    for (auto it = map.begin(); it != map.end(); it++) {
        ret.insert(it.key(), PToQVariantMap(it.value()));
    }
    return ret;
}

template<typename T>
QMap<QString, QSharedPointer<T> > QSerializer<T>::QVariantMapToPStrMap(QVariantMap map)
{
    auto ret = QMap<QString, P>{};
    for (auto it = map.begin(); it != map.end(); it++) {
        ret.insert(it.key(), QVariantMapToP(it.value().toMap()));
    }
    return ret;
}

#define QSERIALIZER_DECLARE(T)             \
  namespace QSerializerPrivateNamespace##T \
  {                                        \
    char init();                           \
    static char _ = init();                \
  };

#define QSERIALIZER_IMPL(T, ...)           \
  namespace QSerializerPrivateNamespace##T \
  {                                        \
    char init()                            \
    {                                      \
      static char _ = []() -> char {       \
QSerializer<T>::registerConverters();      \
__VA_ARGS__;                               \
return 0;                                  \
      }();                                 \
      return _;                            \
    }                                      \
  }

#endif
