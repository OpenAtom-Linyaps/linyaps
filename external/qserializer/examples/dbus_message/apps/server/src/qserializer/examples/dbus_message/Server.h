#pragma once

#include <QMap>    // for QMap
#include <QObject> // for QObject, Q_OBJECT, slots
#include <QString> // for QString

#include "qserializer/examples/dbus_message/Response.h" // for Response

template <class T>
class QSharedPointer;
class QString;

namespace qserializer::examples::dbus_message
{

class Response;

class Server : public QObject {
        Q_OBJECT;

    public:
        Server(QObject *parent = nullptr);
        virtual ~Server();
    public slots:
        QMap<QString, QSharedPointer<Response>> ping();
};

}
