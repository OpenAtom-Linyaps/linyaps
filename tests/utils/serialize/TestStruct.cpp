#include "utils/serialize/TestStruct.h"

static void registerCustomConveter();

QSERIALIZER_IMPL(TestStruct, { registerCustomConveter(); });

static void registerCustomConveter()
{
    static int _ = []() -> int {
        auto fn1 = [](const QList<float> &from) -> QVariantList {
            QVariantList vlist;
            for (const float &x : from) {
                vlist.push_back(x);
            }
            return vlist;
        };
        QMetaType::registerConverter<QList<float>, QVariantList>(fn1);

        auto fn2 = [](const QVariantList &from) -> QList<float> {
            QList<float> list;
            for (const QVariant &x : from) {
                list.push_back(x.toFloat());
            }
            return list;
        };
        QMetaType::registerConverter<QVariantList, QList<float>>(fn2);

        return _;
    }();
}
