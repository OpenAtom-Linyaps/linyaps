#pragma once

#include <QMetaType>                 // for qRegisterMetaType
#include <QObject>                   // for Q_GADGET, Q_PROPERTY
#include <QSerializer/QSerializer.h> // for QSERIALIZER_DECLARE
#include <QSharedPointer>            // for QSharedPointer

#include "Base.h" // for Base

class Page : public Base {
        Q_GADGET;
        Q_PROPERTY(int number MEMBER m_number);

    public:
        int m_number;
};

QSERIALIZER_DECLARE(Page);
