#ifndef LINGLONG_UTILS_SERIALIZE_YAML_H
#define LINGLONG_UTILS_SERIALIZE_YAML_H

#include "linglong/util/error.h"

#include <yaml-cpp/yaml.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSharedPointer>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <tuple>

namespace YAML {

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
    static Node encode(const QVariantMap &rhs)
    {

        Node node;
        for (auto it = rhs.begin(); it != rhs.end(); ++it) {
            node[it.key()] = it.value();
        }
        return node;
    }

    static bool decode(const Node &node, QVariantMap &rhs)
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
};

template<>
struct convert<QVariantList>
{
    static Node encode(const QVariantList &rhs)
    {
        Node node;
        for (auto it = rhs.begin(); it != rhs.end(); ++it) {
            node.push_back(it->value<QVariant>());
        }
        return node;
    }

    static bool decode(const Node &node, QVariantList &rhs)
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
};

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
} // namespace YAML

namespace linglong::utils::serialize::yaml {

namespace helper {

template<typename>
struct isQList : std::false_type
{
};

template<typename T>
struct isQList<QList<T>> : std::true_type
{
};

template<typename>
struct isQStringMap : std::false_type
{
};

template<typename T>
struct isQStringMap<QMap<QString, T>> : std::true_type
{
};

} // namespace helper

template<typename T>
std::tuple<T, util::Error> deserialize(const QByteArray &data)
{
    try {
        auto node = YAML::Load(data.constData());
        QVariant v;
        if (helper::isQList<T>::value) {
            v = QVariant::fromValue(node.as<QVariantList>());
        } else {
            v = QVariant::fromValue(node.as<QVariantMap>());
        }
        if (v.canConvert<T>())
            return std::make_tuple<T, util::Error>(v.value<T>(), {});

        return std::make_tuple<T, util::Error>(
                {},
                NewError(-1, QString("deserialize value from YAML: convert failed")));
    } catch (const std::exception &e) {
        return std::make_tuple<T, util::Error>(
                {},
                NewError(-1, QString("deserialize value from YAML: ") + e.what()));
    } catch (...) {
        return std::make_tuple<T, util::Error>(
                {},
                NewError(-1, QString("deserialize value from YAML failed")));
    }
}

template<typename T>
std::tuple<QByteArray, util::Error> serialize(const T &x)
{
    try {
        YAML::Node node;
        QVariant v = QVariant::fromValue(x);

        if (helper::isQList<T>::value) {
            v = v.toList();
        } else {
            v = v.toMap();
        }

        if (!v.canConvert<YAML::Node>()) {
            return std::make_tuple<QByteArray, util::Error>(
                    {},
                    NewError(-1, "serialize value to YAML: convert failed"));
        }

        node = v.value<YAML::Node>();

        if (helper::isQList<T>::value) {
            if (!node.IsSequence()) {
                return std::make_tuple<QByteArray, util::Error>(
                        {},
                        NewError(-1, "serialize value to YAML: node is not a sequence"));
            }
        } else {
            if (!node.IsMap()) {
                return std::make_tuple<QByteArray, util::Error>(
                        {},
                        NewError(-1, "serialize value to YAML: node is not a map"));
            }
        }

        YAML::Emitter emitter;
        emitter << node;
        return { QByteArray(emitter.c_str()), {} };

    } catch (const std::exception &e) {
        return std::make_tuple<QByteArray, util::Error>(
                {},
                NewError(-1, QString("serialize value to YAML: ") + e.what()));
    } catch (...) {
        return std::make_tuple<QByteArray, util::Error>(
                {},
                NewError(-1, QString("serialize value to YAML failed")));
    }
}

int init();
static int _ = init();

} // namespace linglong::utils::serialize::yaml

Q_DECLARE_METATYPE(YAML::Node);

#endif
