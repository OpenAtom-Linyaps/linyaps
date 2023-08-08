#ifndef LINGLONG_SRC_API_V1_TYPES_COMMONOPTIONS_H_
#define LINGLONG_SRC_API_V1_TYPES_COMMONOPTIONS_H_

#include <qserializer/dbus.h>

#include <QString>

namespace linglong::api::v1::types {

class CommonOptions
{
    Q_GADGET;

public:
    Q_PROPERTY(QString appId MEMBER appId);
    QString appId;

    Q_PROPERTY(QString version MEMBER version);
    QString version;

    Q_PROPERTY(QString arch MEMBER version);
    QString arch;

    Q_PROPERTY(QString channel MEMBER channel);
    QString channel;

    Q_PROPERTY(QString appModule MEMBER appModule);
    QString appModule;
};

} // namespace linglong::api::v1::types

QSERIALIZER_DECLARE_DBUS(linglong::api::v1::types::CommonOptions);

#endif
