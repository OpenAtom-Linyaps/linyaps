#include "utils/serialize/yaml.h"

#include <mutex>

namespace linglong::utils::serialize::yaml {
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
} // namespace linglong::utils::serialize::yaml
