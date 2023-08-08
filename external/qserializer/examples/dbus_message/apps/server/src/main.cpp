#include <QCoreApplication> // for QCoreApplication
#include <QDBusConnection>  // for QDBusConnection
#include <QMetaType>        // for QMetaType
#include <QSharedPointer>   // for QSharedPointer
#include <QtCore>           // for Q_ASSERT

#include "qserializer/examples/dbus_message/Server.h" // for Server
#include "qserializer/examples/dbus_message/TestDBusServerAdaptor.h" // for TestDBusServerAdaptor

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
        qserializer::examples::dbus_message::Server s;

        auto adaptor =
                QSharedPointer<qserializer::examples::dbus_message::
                                       TestDBusServerAdaptor>::create(&s);
        Q_ASSERT(QDBusConnection::sessionBus().registerObject("/", &s));
        Q_ASSERT(QDBusConnection::sessionBus().registerService(
                GetClassInfo<qserializer::examples::dbus_message::
                                     TestDBusServerAdaptor *>(
                        DBusInterfaceClassInfoName)));
        QCoreApplication app(argc, argv);
        return app.exec();
}
