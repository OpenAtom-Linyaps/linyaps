#pragma once

#include <type_traits> // for is_base_of, is_pointer

#include <QDebug>           // for QDebug
#include <QList>            // for QList
#include <QLoggingCategory> // for qCCritical, Q_LOGGING_CATEGORY
#include <QMap>             // for QMap, operator!=, operator==
#include <QMessageLogger>   // for QMessageLogger
#include <QMetaObject>      // for QMetaObject
#include <QMetaProperty>    // for QMetaProperty
#include <QMetaType>        // for QMetaType
#include <QSharedPointer>   // for QSharedPointer
#include <QString>          // for QString
#include <QVariant>         // for QVariant
#include <QVariantList>     // for QVariantList
#include <QVariantMap>      // for QVariantMap

class QObject;
template <class T>
class QSharedPointer;

namespace qserializer::detail
{

static inline Q_LOGGING_CATEGORY(log, "qserializer");

template <typename T>
class QSerializer {
    public:
        static_assert(!std::is_pointer<T>::value,
                      "QSerializer shouldn't instantiation with a pointer.");
        static_assert(!std::is_base_of<QObject, T>::value,
                      "QSerializer shouldn't instantiation with a QObject.");

        static void registerConverters();

    private:
        typedef QSharedPointer<T> P;

        static QVariantMap PToQVariantMap(P from);
        static P QVariantMapToP(const QVariantMap &map);

        static QVariantList PListToQVariantList(QList<P> list);

        static QList<P> QVariantListToPList(QVariantList list);

        static QVariantMap PStrMapToQVariantMap(QMap<QString, P> map);
        static QMap<QString, P> QVariantMapToPStrMap(QVariantMap map);
};

template <typename T>
void QSerializer<T>::registerConverters()
{
        QMetaType::registerConverter<P, QVariantMap>(PToQVariantMap);
        QMetaType::registerConverter<QVariantMap, P>(QVariantMapToP);

        QMetaType::registerConverter<QList<P>, QVariantList>(
                PListToQVariantList);
        QMetaType::registerConverter<QVariantList, QList<P> >(
                QVariantListToPList);

        QMetaType::registerConverter<QMap<QString, P>, QVariantMap>(
                PStrMapToQVariantMap);
        QMetaType::registerConverter<QVariantMap, QMap<QString, P> >(
                QVariantMapToPStrMap);
}

template <typename T>
QVariantMap QSerializer<T>::PToQVariantMap(P from)
{
        auto ret = QVariantMap{};
        if (from.isNull()) {
                return ret;
        }

        static const QMetaObject *const metaObject =
                QMetaType::fromType<T *>().metaObject();

        for (int i = 0; i < metaObject->propertyCount(); i++) {
                const char *k = metaObject->property(i).name();
                QVariant v = metaObject->property(i).readOnGadget(from.data());
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
                qCCritical(log).noquote()
                        << QString("Failed to insert \"%1\", maybe missing converter")
                                   .arg(k);
        }
        return ret;
}

template <typename T>
QSharedPointer<T> QSerializer<T>::QVariantMapToP(const QVariantMap &map)
{
        P ret(new T());

        static const QMetaObject *const metaObject =
                QMetaType::fromType<T *>().metaObject();

        for (int i = 0; i < metaObject->propertyCount(); i++) {
                QMetaProperty metaProp = metaObject->property(i);

                const char *metaPropName = metaProp.name();
                QVariantMap::ConstIterator it = map.find(metaPropName);
                if (it == map.end()) {
                        continue;
                }

                if (metaProp.writeOnGadget(ret.data(), it.value())) {
                        continue;
                }

                qCCritical(log).noquote()
                        << QString("Failed to write \"%1\" [%2] , maybe missing converter")
                                   .arg(metaPropName)
                                   .arg(metaProp.typeName());
        }
        return ret;
}

template <typename T>
QVariantList QSerializer<T>::PListToQVariantList(QList<P> list)
{
        auto ret = QVariantList{};
        for (auto const &item : list) {
                ret.push_back(PToQVariantMap(item));
        }
        return ret;
}

template <typename T>
QList<QSharedPointer<T> > QSerializer<T>::QVariantListToPList(QVariantList list)
{
        auto ret = QList<P>{};
        for (auto const &item : list) {
                ret.push_back(QVariantMapToP(item.toMap()));
        }
        return ret;
}

template <typename T>
QVariantMap QSerializer<T>::PStrMapToQVariantMap(QMap<QString, P> map)
{
        auto ret = QVariantMap{};
        for (auto it = map.begin(); it != map.end(); it++) {
                ret.insert(it.key(), PToQVariantMap(it.value()));
        }
        return ret;
}

template <typename T>
QMap<QString, QSharedPointer<T> >
QSerializer<T>::QVariantMapToPStrMap(QVariantMap map)
{
        auto ret = QMap<QString, P>{};
        for (auto it = map.begin(); it != map.end(); it++) {
                ret.insert(it.key(), QVariantMapToP(it.value().toMap()));
        }
        return ret;
}
}
