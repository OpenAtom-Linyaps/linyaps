#include <QDBusConnection>    // for QDBusConnection
#include <QDBusError>         // for operator<<, QDBusError
#include <QDBusPendingReply>  // for QDBusPendingReply
#include <QDebug>             // for QDebug
#include <QMap>               // for QMap
#include <QMessageLogContext> // for qCritical
#include <QSharedPointer>     // for QSharedPointer
#include <QtGlobal>           // for qDebug

#include "Response.h"                        // for Response
#include "dbusgen/TestDBusServerInterface.h" // for TestDBusServer

int main()
{
        io::github::black_desk::TestDBusServer server(
                io::github::black_desk::TestDBusServer::staticInterfaceName(),
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
