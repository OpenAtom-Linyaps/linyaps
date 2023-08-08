# Use QSerializer with DBus message

This is an example to show how to use QSerializer to
serialize a Q_GADGET class to a DBus message with type signature `a{sv}`.

- `src` include a Q_GADGET class `Response`;
- `apps/server` is a DBus service working on session bus,
  using `Response` as a DBus reply;
- `apps/client` is a program call DBus method provided by `apps/server`,
  and print the `Response` it received in DBus method-return message.
