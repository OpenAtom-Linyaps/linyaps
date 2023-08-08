#pragma once

#include <QList>          // for QList
#include <QMap>           // for QMap
#include <QMetaType>      // for qRegisterMetaType
#include <QObject>        // for Q_PROPERTY, Q_GADGET
#include <QSharedPointer> // for QSharedPointer
#include <QString>        // for QString
#include <QStringList>    // for QStringList

#include "qserializer/core.h"            // for QSERIALIZER_DECLARE
#include "qserializer/test_types/Base.h" // for Base
#include "qserializer/test_types/Page.h" // for Page

template <class T>
class QSharedPointer;

namespace qserializer::test_types
{

class Page;

class Book : public Base {
        Q_GADGET;
        Q_PROPERTY(QString title MEMBER m_title);
        // NOTE:
        // Qt5 can not found convertor
        // if type referenced in Q_PROPERTY is not global.
        Q_PROPERTY(QList<QSharedPointer<qserializer::test_types::Page> > pages
                           MEMBER m_pages);
        Q_PROPERTY(QMap<QString, QSharedPointer<qserializer::test_types::Page> >
                           dict MEMBER m_dict);
        Q_PROPERTY(QStringList list MEMBER m_list1);
        Q_PROPERTY(QList<QString> list2 MEMBER m_list2);
        Q_PROPERTY(QMap<QString, QSharedPointer<qserializer::test_types::Book> >
                           dict2 MEMBER m_dict2);

    public:
        QString m_title;
        QList<QSharedPointer<Page> > m_pages;
        QMap<QString, QSharedPointer<Page> > m_dict;
        QStringList m_list1;
        // NOTE:
        // Qt5 cannot convert QList<QString> to QVariantList by default.
        // You need a custom converter registered to make QSerializer work,
        // check Book.cpp for details.
        QList<QString> m_list2;
        QMap<QString, QSharedPointer<Book> > m_dict2;
};

}

QSERIALIZER_DECLARE(qserializer::test_types::Book);
