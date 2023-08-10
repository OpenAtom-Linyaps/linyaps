#ifndef LINGLONG_API_V1_TYPES_EXECUTEOPTIONS_H_
#define LINGLONG_API_V1_TYPES_EXECUTEOPTIONS_H_

#include <qserializer/dbus.h>

#include <QString>

namespace linglong::api::v1::types {

class ExecuteOptions
{
    Q_GADGET;

public:
    Q_PROPERTY(QString containerID MEMBER containerID);
    QString containerID;

    Q_PROPERTY(QList<QString> command MEMBER command);
    QList<QString> command;

    Q_PROPERTY(QList<QString> env MEMBER env);
    QList<QString> env;

    Q_PROPERTY(QString cwd MEMBER cwd);
    QString cwd;
};

} // namespace linglong::api::v1::types

QSERIALIZER_DECLARE_DBUS(linglong::api::v1::types::ExecuteOptions);

#endif
