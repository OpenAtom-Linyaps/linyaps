# QSerializer

A simple header-only qt helper library for serializing/deserializing
QSharedPointer\<class with Q_GADGET\> into/from QVariantMap.

It have to be a QSharedPointer of a class with  Q_GADGET
but not a QObject or a pointer to a QObject.
That's because this library rely on writeOnGadget,
witch will not work on QObject.

Also support serializing/deserializing

- QList\<QSharedPointer\<class with Q_GADGET\>\>
- QMap\<QString, QSharedPointer\<class with Q_GADGET\>\>

into/from

- QVariantList
- QVariantMap

As QVariantMap and QVariantList can easily
be converted to/from Qt JSON types and QDBusArgument,
this library should be good enough for common usage.

Check [tests](tests/tests.cpp) for coding examples.

---

[`QSerializerDBus.h`](include/QSerializer/QSerializerDBus.h)
can working with QDBusArgument,
check [the example](examples/dbus_message) for details of that.
