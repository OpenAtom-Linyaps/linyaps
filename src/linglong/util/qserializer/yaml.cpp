/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/util/qserializer/yaml.h"

#include <yaml-cpp/yaml.h>

#include <mutex>

namespace YAML {

// QString
template<>
struct convert<QString>
{
    static Node encode(const QString &rhs) { return Node(rhs.toStdString()); }

    static bool decode(const Node &node, QString &rhs)
    {
        if (!node.IsScalar())
            return false;
        rhs = QString::fromStdString(node.Scalar());
        return true;
    }
};

template<>
struct convert<QVariantMap>
{
    static Node encode(const QVariantMap &rhs);
    static bool decode(const Node &node, QVariantMap &rhs);
};

template<>
struct convert<QVariantList>
{
    static Node encode(const QVariantList &rhs);
    static bool decode(const Node &node, QVariantList &rhs);
};

// QVariant
template<>
struct convert<QVariant>
{
    static Node encode(const QVariant &rhs)
    {
        Node n;
        switch (rhs.type()) {
        case QVariant::Map:
            n = convert<QVariantMap>::encode(rhs.toMap());
            break;
        case QVariant::List:
            n = convert<QVariantList>::encode(rhs.toList());
            break;
        default:
            return Node(rhs.toString());
        }
        return n;
    }

    static bool decode(const Node &node, QVariant &rhs)
    {
        try {
            switch (node.Type()) {
            case NodeType::Undefined:
            case NodeType::Null:
                rhs = QVariant(QVariant::Invalid);
                break;
            case NodeType::Scalar:
                rhs = QVariant::fromValue(QString::fromStdString(node.Scalar()));
                break;
            case NodeType::Sequence:
                rhs = node.as<QVariantList>();
                break;
            case NodeType::Map:
                rhs = node.as<QVariantMap>();
                break;
            }
            return true;
        } catch (...) {
            return false;
        }
    }
};

Node convert<QVariantMap>::encode(const QVariantMap &rhs)
{
    Node node;
    for (auto it = rhs.begin(); it != rhs.end(); ++it) {
        node[it.key()] = it.value();
    }
    return node;
}

bool convert<QVariantMap>::decode(const Node &node, QVariantMap &rhs)
{
    try {
        for (auto it = node.begin(); it != node.end(); ++it) {
            rhs[it->first.as<QString>()] = it->second.as<QVariant>();
        }
        return true;
    } catch (...) {
        return false;
    }
}

Node convert<QVariantList>::encode(const QVariantList &rhs)
{
    Node node;
    for (auto it = rhs.begin(); it != rhs.end(); ++it) {
        node.push_back(it->value<QVariant>());
    }
    return node;
}

bool convert<QVariantList>::decode(const Node &node, QVariantList &rhs)
{
    try {
        for (const auto &i : node) {
            QVariant v = i.as<QVariant>();
            rhs.push_back(v);
        }
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace YAML

namespace linglong {
namespace util {

std::tuple<QVariantMap, Error> mapFromYAML(const QByteArray &bytes)
{
    try {
        auto node = YAML::Load(bytes.constData());
        auto map = node.as<QVariantMap>();
        return { map, {} };
    } catch (const std::exception &e) {
        return std::make_tuple<QVariantMap, Error>({}, NewError(-1, e.what()));
    }
}

std::tuple<QVariantList, Error> listFromYAML(const QByteArray &bytes)
{
    try {
        auto node = YAML::Load(bytes.constData());
        auto list = node.as<QVariantList>();
        return { list, {} };
    } catch (const std::exception &e) {
        return std::make_tuple<QVariantList, Error>({}, NewError(-1, e.what()));
    }
}

namespace QSerializerPrivateNamespaceYAMLNode {
int init()
{
    static std::once_flag flag;
    std::call_once(flag, []() {
        QMetaType::registerConverter<QVariantMap, YAML::Node>(
          [](const QVariantMap &in) -> YAML::Node {
              YAML::Node node;
              node = in;
              return node;
          });
        QMetaType::registerConverter<YAML::Node, QVariantMap>(
          [](const YAML::Node &in) -> QVariantMap {
              return in.as<QVariantMap>();
          });
        QMetaType::registerConverter<QVariantList, YAML::Node>(
          [](const QVariantList &in) -> YAML::Node {
              YAML::Node node;
              node = in;
              return node;
          });
        QMetaType::registerConverter<YAML::Node, QVariantList>(
          [](const YAML::Node &in) -> QVariantList {
              return in.as<QVariantList>();
          });
    });
    return 0;
}
} // namespace QSerializerPrivateNamespaceYAMLNode

} // namespace util
} // namespace linglong
