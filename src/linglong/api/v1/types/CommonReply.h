#ifndef LINGLONG_API_V1_TYPES_COMMONREPLY_H_
#define LINGLONG_API_V1_TYPES_COMMONREPLY_H_

#include <qserializer/dbus.h>

#include <QString>

namespace linglong::api::v1::types {

class CommonReply
{
    Q_GADGET;

public:
    Q_PROPERTY(int code MEMBER code);
    int code;

    Q_PROPERTY(QString message MEMBER message);
    QString message;
};

} // namespace linglong::api::v1::types

QSERIALIZER_DECLARE_DBUS(linglong::api::v1::types::CommonReply);

#endif
