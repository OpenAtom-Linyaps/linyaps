#pragma once

#include <QMap>    // for QMap
#include <QObject> // for QObject, Q_OBJECT, slots
#include <QString> // for QString

#include "Response.h" // for Response

class Response;
template <class T>
class QSharedPointer;
class QString;

class Server : public QObject {
        Q_OBJECT;

    public:
        Server(QObject *parent = nullptr);
        virtual ~Server();
    public slots:
        QMap<QString, QSharedPointer<Response>> ping();
};
