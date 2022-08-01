/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <yaml-cpp/yaml.h>

#include <QString>
#include <QVariant>
#include <QJsonValue>
#include <QDebug>

#include "module/util/json.h"

namespace YAML {

// QString
template<>
struct convert<QString> {
    static Node encode(const QString &rhs) { return Node(rhs.toStdString()); }

    static bool decode(const Node &node, QString &rhs)
    {
        if (!node.IsScalar())
            return false;
        rhs = QString::fromStdString(node.Scalar());
        return true;
    }
};

// QVariant
template<>
struct convert<QVariant> {
    static Node encode(const QVariant &rhs)
    {
        Node n;
        QVariantList list;
        QVariantMap map;
        switch (rhs.type()) {
        case QVariant::Map:
            map = rhs.toMap();
            for (QVariantMap::iterator it = map.begin(); it != map.end(); ++it) {
                n[it.key()] = it.value();
            }
            break;
        case QVariant::List:
            list = rhs.toList();
            for (auto &it : list) {
                n.push_back(it.value<QVariant>());
            }
            break;
        default:
            return Node(rhs.toString());
        }
        return n;
    }

    static bool decode(const Node &node, QVariant &rhs)
    {
        QVariantList list;
        QVariantMap map;
        switch (node.Type()) {
        case NodeType::Undefined:
        case NodeType::Null:
            rhs = QVariant(QVariant::Invalid);
            break;
        case NodeType::Scalar:
            rhs = QVariant::fromValue(QString::fromStdString(node.Scalar()));
            break;
        case NodeType::Sequence:
            for (const auto &i : node) {
                QVariant v = i.as<QVariant>();
                list.push_back(v);
            }
            rhs = list;
            break;
        case NodeType::Map:
            for (YAML::const_iterator it = node.begin(); it != node.end(); ++it) {
                map[it->first.as<QString>()] = it->second.as<QVariant>();
            }
            rhs = map;
            break;
        }

        return true;
    }
};

// QJsonValue
template<>
struct convert<QJsonValue> {
    static Node encode(const QJsonValue &rhs) { return Node(rhs.toVariant()); }

    static bool decode(const Node &node, QJsonValue &rhs)
    {
        Q_ASSERT(false);
        return false;
    }
};

} // namespace YAML

template<typename T>
T *formYaml(const YAML::Node &doc)
{
    auto m = new T;
    auto mo = m->metaObject();
    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        auto k = mo->property(i).name();
        auto t = mo->property(i).type();

        if (!doc[k]) {
            continue;
        }

        QVariant v = doc[k].template as<QVariant>();
        // set parent
        if (QVariant::UserType == t) {
            switch (v.type()) {
            case QVariant::Map: {
                auto map = v.toMap();
                map[Q_SERIALIZE_PARENT_KEY] = QVariant::fromValue(m);
                m->setProperty(k, map);
                break;
            }
            case QVariant::List: {
                auto list = v.toList();
                list.push_front(QVariant::fromValue(m));
                m->setProperty(k, list);
                break;
            }
            default:
                m->setProperty(k, v);
            }
        } else {
            m->setProperty(k, v);
        }
    }

    m->onPostSerialize();
    return m;
}

template<typename T>
YAML::Node toYaml(T *m)
{
    YAML::Node n;
    auto mo = m->metaObject();
    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        auto k = mo->property(i).name();
        n[k] = m->property(k).toJsonValue();
    }
    return n;
}