#pragma once

#include <QMetaType>                 // for qRegisterMetaType
#include <QObject>                   // for Q_GADGET, Q_PROPERTY
#include <QSerializer/QSerializer.h> // for QSERIALIZER_DECLARE
#include <QSharedPointer>            // for QSharedPointer
#include <QString>                   // for QString

class Base {
        Q_GADGET;
        Q_PROPERTY(QString base MEMBER m_base);

    public:
        QString m_base;
};

QSERIALIZER_DECLARE(Base);
