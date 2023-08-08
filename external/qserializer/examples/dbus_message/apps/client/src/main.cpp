#include <QDBusConnection>    // for QDBusConnection
#include <QDBusError>         // for operator<<, QDBusError
#include <QDBusPendingReply>  // for QDBusPendingReply
#include <QDebug>             // for QDebug
#include <QMap>               // for QMap
#include <QMessageLogContext> // for qCritical
#include <QSharedPointer>     // for QSharedPointer
#include <QtGlobal>           // for qDebug

#include "qserializer/examples/dbus_message/Response.h" // for Response
#include "qserializer/examples/dbus_message/TestDBusServerInterface.h" // for TestDBusServer

int main()
{
        qserializer::examples::dbus_message::TestDBusServerInterface server(
                qserializer::examples::dbus_message::TestDBusServerInterface::
                        staticInterfaceName(),
                "/", QDBusConnection::sessionBus());
        auto resp = server.ping();
        auto value = resp.value();
        if (resp.isError() || !resp.isValid()) {
                qCritical() << resp.error();
                return -1;
        }
        qDebug() << value["resp"]->msg << value["resp"]->code;
        return 0;
}
