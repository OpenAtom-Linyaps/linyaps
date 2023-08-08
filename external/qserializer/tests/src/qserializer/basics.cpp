#include "gtest/gtest.h" // for Message, TestPartResult

#include <string>      // for allocator
#include <type_traits> // for enable_if_t

#include <QJsonDocument>   // for QJsonDocument
#include <QJsonObject>     // for QJsonObject
#include <QJsonParseError> // for QJsonParseError
#include <QJsonValue>      // for QJsonValue
#include <QList>           // for QList
#include <QMap>            // for QMap
#include <QSharedPointer>  // for QSharedPointer
#include <QString>         // for QString
#include <QStringList>     // for QStringList
#include <QVariant>        // for QVariant

#include "qserializer/test_types/Book.h" // for Book
#include "qserializer/test_types/Page.h" // for Page
#include "qserializer/test_utils.h"      // IWYU pragma: keep

template <class T>
class QSharedPointer;

namespace qserializer
{

TEST(QSerializer, Basic)
{
        auto sourceJson = QString(R"({
                "title": "Some title",
                "pages": [
                        { "number": 1 },
                        { "number": 2 }
                ],
                "dict": {
                        "page1": { "number": 1 },
                        "page2": { "number": 2 }
                },
                "list": [
                        "string1",
                        "string2",
                        "string3"
                ],
                "list2": [
                        "string1",
                        "string2",
                        "string3"
                ],
                "dict2": {
                        "111": {},
                        "222": {}
                }
        })")
                                  .toUtf8();

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(sourceJson, &err);
        ASSERT_EQ(err.error, QJsonParseError::NoError);

        QVariant v = doc.object().toVariantMap();
        auto book = v.value<QSharedPointer<test_types::Book>>();

        ASSERT_NE(book, nullptr);
        ASSERT_EQ(book->m_title, "Some title");
        ASSERT_EQ(book->m_pages.length(), 2);
        ASSERT_EQ(book->m_pages[0]->m_number, 1);
        ASSERT_EQ(book->m_pages[1]->m_number, 2);
        ASSERT_EQ(book->m_dict.size(), 2);
        ASSERT_EQ(book->m_dict["page1"]->m_number, 1);
        ASSERT_EQ(book->m_dict["page2"]->m_number, 2);
        ASSERT_EQ(book->m_list1.size(), 3);
        ASSERT_EQ(book->m_list1[0], "string1");
        ASSERT_EQ(book->m_list1[1], "string2");
        ASSERT_EQ(book->m_list1[2], "string3");
        ASSERT_EQ(book->m_list2.size(), 3);
        ASSERT_EQ(book->m_list2[0], "string1");
        ASSERT_EQ(book->m_list2[1], "string2");
        ASSERT_EQ(book->m_list2[2], "string3");
        ASSERT_EQ(book->m_dict2.size(), 2);
        ASSERT_EQ(book->m_dict2["111"]->m_title, "");

        auto vmap = v.fromValue(book).toMap();
        ASSERT_EQ(vmap.value("title").toString(), "Some title");
        ASSERT_EQ(vmap.value("pages").toList().size(), 2);
        ASSERT_EQ(
                vmap.value("pages").toList()[0].toMap().value("number").toInt(),
                1);
        ASSERT_EQ(
                vmap.value("pages").toList()[1].toMap().value("number").toInt(),
                2);
        ASSERT_EQ(vmap.value("dict")
                          .toMap()
                          .value("page1")
                          .toMap()
                          .value("number")
                          .toInt(),
                  1);
        ASSERT_EQ(vmap.value("dict")
                          .toMap()
                          .value("page2")
                          .toMap()
                          .value("number")
                          .toInt(),
                  2);
        ASSERT_EQ(vmap.value("list").toStringList().size(), 3);
        ASSERT_EQ(vmap.value("list").toStringList()[0], "string1");
        ASSERT_EQ(vmap.value("list").toStringList()[1], "string2");
        ASSERT_EQ(vmap.value("list").toStringList()[2], "string3");
        ASSERT_EQ(vmap.value("list2").toStringList().size(), 3);
        ASSERT_EQ(vmap.value("list2").toStringList()[0], "string1");
        ASSERT_EQ(vmap.value("list2").toStringList()[1], "string2");
        ASSERT_EQ(vmap.value("list2").toStringList()[2], "string3");
        ASSERT_EQ(vmap.value("dict2").toMap().size(), 2);
        ASSERT_EQ(vmap.value("dict2").toMap()["111"].toMap().size(), 7);

        auto jsonObject = QJsonObject::fromVariantMap(vmap);

        auto expectedJson = QString(R"({
                "base": "",
                "title": "Some title",
                "pages": [
                        { "number": 1, "base": "" },
                        { "number": 2, "base": "" }
                ],
                "dict": {
                        "page1": { "number": 1, "base": "" },
                        "page2": { "number": 2, "base": "" }
                },
                "list": [
                        "string1",
                        "string2",
                        "string3"
                ],
                "list2": [
                        "string1",
                        "string2",
                        "string3"
                ],
                "dict2": {
                        "111": {
                                "base": "",
                                "title": "",
                                "pages": [],
                                "dict": {},
                                "list": [],
                                "list2": [],
                                "dict2": {}
                        },
                        "222": {
                                "base": "",
                                "title": "",
                                "pages": [],
                                "dict": {},
                                "list": [],
                                "list2": [],
                                "dict2": {}
                        }
                }
        })")
                                    .toUtf8();
        doc = QJsonDocument::fromJson(expectedJson, &err);
        ASSERT_EQ(err.error, QJsonParseError::NoError);

        ASSERT_EQ(QJsonValue(jsonObject), QJsonValue(doc.object()));

        v = jsonObject.toVariantMap();

        auto book2 = v.value<QSharedPointer<test_types::Book>>();

        ASSERT_NE(book2, nullptr);
        ASSERT_EQ(book2->m_title, "Some title");
        ASSERT_EQ(book2->m_pages.length(), 2);
        ASSERT_EQ(book2->m_pages[0]->m_number, 1);
        ASSERT_EQ(book2->m_pages[1]->m_number, 2);
        ASSERT_EQ(book2->m_dict.size(), 2);
        ASSERT_EQ(book2->m_dict["page1"]->m_number, 1);
        ASSERT_EQ(book2->m_dict["page2"]->m_number, 2);
        ASSERT_EQ(book->m_list1.size(), 3);
        ASSERT_EQ(book2->m_list1[0], "string1");
        ASSERT_EQ(book2->m_list1[1], "string2");
        ASSERT_EQ(book2->m_list1[2], "string3");
        ASSERT_EQ(book->m_list2.size(), 3);
        ASSERT_EQ(book2->m_list2[0], "string1");
        ASSERT_EQ(book2->m_list2[1], "string2");
        ASSERT_EQ(book2->m_list2[2], "string3");
}

}
