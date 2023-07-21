#pragma once

#include <QMetaType>                     // for qRegisterMetaType
#include <QObject>                       // for Q_PROPERTY, Q_GADGET
#include <QSerializer/QSerializerDBus.h> // for move_in, move_out, QSERIALI...
#include <QSharedPointer>                // for QSharedPointer
#include <QString>                       // for QString

class Response {
        Q_GADGET;
        Q_PROPERTY(QString msg MEMBER msg);
        Q_PROPERTY(int code MEMBER code);

    public:
        QString msg;
        int code;
};

QSERIALIZER_DECLARE_DBUS(Response);
