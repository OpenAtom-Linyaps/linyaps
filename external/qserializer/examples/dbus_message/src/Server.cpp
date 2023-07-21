#include "Server.h"

#include <initializer_list> // for initializer_list

#include <QSharedPointer> // for QSharedPointer
#include <QString>        // for QString

#include "Response.h" // for Response

template <class T>
class QSharedPointer;

Server::Server(QObject *parent)
        : QObject(parent)
{
}

Server::~Server() = default;

QMap<QString, QSharedPointer<Response>> Server::ping()
{
        auto resp = QSharedPointer<Response>(new Response);
        resp->msg = "message";
        resp->code = 2;
        return { { "resp", resp } };
}
