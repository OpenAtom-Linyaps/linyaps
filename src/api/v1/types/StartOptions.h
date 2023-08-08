#ifndef LINGLONG_SRC_API_V1_TYPES_STARTOPTIONS_H_
#define LINGLONG_SRC_API_V1_TYPES_STARTOPTIONS_H_

#include "api/v1/types/CommonOptions.h"
#include "api/v1/types/DBusProxyOptions.h"

#include <qserializer/dbus.h>
#include <QString>

namespace linglong::api::v1::types {

class StartOptions : public CommonOptions
{
    Q_GADGET;

public:
    Q_PROPERTY(QList<QString> command MEMBER command);
    QList<QString> command;

    Q_PROPERTY(QList<QString> env MEMBER env);
    QList<QString> env;

    Q_PROPERTY(QString cwd MEMBER cwd);
    QString cwd;

    Q_PROPERTY(QSharedPointer<DBusProxyOptions> dbusProxy MEMBER dbusProxy);
    QSharedPointer<DBusProxyOptions> dbusProxy;
};

} // namespace linglong::api::v1::types

QSERIALIZER_DECLARE_DBUS(linglong::api::v1::types::StartOptions);

#endif
