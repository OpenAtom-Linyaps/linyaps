/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtest/gtest.h>
#include <typeinfo>

#include "src/service/impl/qdbus_retmsg.h"

TEST(RetMessage, json2mesg)
{
    std::string tpl_json = R"json({
    "state" : true,
    "code" : 100,
    "message" : "demo message"})json";

    auto json = QJsonDocument::fromJson(QByteArray(tpl_json.c_str(), tpl_json.length()));
    std::cout << "input json:" << json.toJson().toStdString() << std::endl;

    QVariant variant = json.toVariant();

    auto rb = variant.value<RetMessage *>();
    std::cout << "state:" << rb->state << std::endl;
    std::cout << "code:" << rb->code << std::endl;
    std::cout << "message:" << rb->message.toStdString() << std::endl;

    EXPECT_EQ(rb->state, true);
    EXPECT_EQ(rb->code, 100);
    EXPECT_EQ(rb->message.toStdString(), "demo message");

    RetMessage vb;

    vb.state = true;
    vb.code = 100;
    vb.message = "demo message";

    auto msg_var = toVariant<RetMessage>(&vb);
    auto var_msg = msg_var.value<RetMessage *>();

    EXPECT_EQ(vb.state, var_msg->state);
    EXPECT_EQ(vb.code, var_msg->code);
    EXPECT_EQ(vb.message, var_msg->message);

    RetMessageList retmsg_list;

    for (int i = 0; i < 3; i++) {
        auto c = QPointer<RetMessage>(new RetMessage);
        c->state = false;
        c->code = 404;
        c->message = "not found!";
        retmsg_list.push_back(c);
    }

    // free
    if (var_msg) {
        delete var_msg;
    }

    // free
    if (rb) {
        delete rb;
    }
}

TEST(RetMessage, json2mesg2)
{
    std::string tpl_json = R"json({
    "state" : "abc",
    "code" : "100a",
    "message" : 1111})json";

    auto json = QJsonDocument::fromJson(QByteArray(tpl_json.c_str(), tpl_json.length()));
    std::cout << "input json:" << json.toJson().toStdString() << std::endl;

    QVariant variant = json.toVariant();

    auto rb = variant.value<RetMessage *>();

    std::cout << "state:" << rb->state << "\ttype:" << typeid(rb->state).name() << std::endl;
    std::cout << "code:" << rb->code << "\ttype:" << typeid(rb->code).name() << std::endl;
    std::cout << "message:" << rb->message.toStdString() << "\ttype:" << typeid(rb->message).name() << std::endl;

    EXPECT_EQ(typeid(rb->state).name(), typeid(true).name());
    EXPECT_EQ(typeid(rb->code).name(), typeid(quint32).name());

    auto vt_msg = QString("1");
    EXPECT_EQ(typeid(rb->message).name(), typeid(vt_msg).name());
    // free
    if (rb) {
        delete rb;
    }
}
