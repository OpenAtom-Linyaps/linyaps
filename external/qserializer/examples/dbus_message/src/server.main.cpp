#include "Server.h" // for Server

#include <QCoreApplication> // for QCoreApplication
#include <QDBusConnection>  // for QDBusConnection
#include <QMetaType>        // for QMetaType
#include <QSharedPointer>   // for QSharedPointer
#include <QtCore>           // for Q_ASSERT

#include "dbusgen/TestDBusServerApaptor.h" // for TestDBusServerAdaptor

constexpr auto DBusInterfaceClassInfoName = "D-Bus Interface";
template <typename T>
const char *GetClassInfo(const char *name)
{
        auto mo = QMetaType::fromType<T>().metaObject();
        auto index = mo->indexOfClassInfo(name);
        return mo->classInfo(index).value();
}

int main(int argc, char **argv)
{
        Server s;

        auto adaptor = QSharedPointer<TestDBusServerAdaptor>::create(&s);
        Q_ASSERT(QDBusConnection::sessionBus().registerObject("/", &s));
        Q_ASSERT(QDBusConnection::sessionBus().registerService(
                GetClassInfo<TestDBusServerAdaptor *>(
                        DBusInterfaceClassInfoName)));
        QCoreApplication app(argc, argv);
        return app.exec();
}
