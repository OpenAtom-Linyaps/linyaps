#pragma once

#include <QMetaType>      // for qRegisterMetaType
#include <QObject>        // for Q_GADGET, Q_PROPERTY
#include <QSharedPointer> // for QSharedPointer

#include "qserializer/core.h"            // for QSERIALIZER_DECLARE
#include "qserializer/test_types/Base.h" // for Base

namespace qserializer::test_types
{

class Page : public Base {
        Q_GADGET;
        Q_PROPERTY(int number MEMBER m_number);

    public:
        int m_number;
};

}

QSERIALIZER_DECLARE(qserializer::test_types::Page);
