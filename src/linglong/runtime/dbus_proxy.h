#include <QString>
#include <QStringList>

class DBusProxyConfig
{
public:
    bool enable;
    QString busType;
    QString appId;
    QString proxyPath;
    QStringList name;
    QStringList path;
    QStringList interface;
};