#ifndef LINGLONG_SRC_API_V1_TYPES_DBUSPROXYOPTIONS_H_
#define LINGLONG_SRC_API_V1_TYPES_DBUSPROXYOPTIONS_H_

#include <QSerializer/QSerializerDBus.h>

namespace linglong::api::v1::types {

class DBusProxySessionBusOptions;
class DBusProxyFilterOptions;

class DBusProxyOptions
{
    Q_GADGET;

public:
    Q_PROPERTY(bool enable MEMBER enable);
    bool enable;

    Q_PROPERTY(QSharedPointer<DBusProxySessionBusOptions> sessionBus MEMBER sessionBus);
    QSharedPointer<DBusProxySessionBusOptions> sessionBus;
};


class DBusProxySessionBusOptions
{
    Q_GADGET;

public:
    Q_PROPERTY(QSharedPointer<DBusProxyFilterOptions> filter MEMBER filter);
    QSharedPointer<DBusProxyFilterOptions> filter;
};

class DBusProxyFilterOptions
{
    Q_GADGET;

public:
    Q_PROPERTY(QString name MEMBER name);
    QString name;

    Q_PROPERTY(QString path MEMBER path);
    QString path;

    Q_PROPERTY(QString interface MEMBER interface);
    QString interface;
};

} // namespace linglong::api::v1::types

QSERIALIZER_DECLARE_DBUS(linglong::api::v1::types::DBusProxyFilterOptions);
QSERIALIZER_DECLARE_DBUS(linglong::api::v1::types::DBusProxySessionBusOptions);
QSERIALIZER_DECLARE_DBUS(linglong::api::v1::types::DBusProxyOptions);

#endif
