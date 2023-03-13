#include "object.h"

#include "module/util/qserializer/qserializer.h"

namespace linglong {
namespace test {

namespace QSerializerPrivateNamespaceTestObject {
int init()
{
    static int _ = []() -> int {
        QSerializer<TestObject>::registerConverters();
        QMetaType::registerConverter<QMap<QString, QString>, QVariantMap>(
                [](const QMap<QString, QString> &from) -> QVariantMap {
                    QVariantMap map;
                    for (auto it = from.begin(); it != from.end(); it++) {
                        map.insert(it.key(), it.value());
                    }
                    return map;
                });
        QMetaType::registerConverter<QVariantMap, QMap<QString, QString>>(
                [](const QVariantMap &from) -> QMap<QString, QString> {
                    QMap<QString, QString> map;
                    for (auto it = from.begin(); it != from.end(); it++) {
                        map.insert(it.key(), it.value().toString());
                    }
                    return map;
                });
        return 0;
    }();
    return _;
}
}; // namespace QSerializerPrivateNamespaceTestObject

} // namespace test
} // namespace linglong
