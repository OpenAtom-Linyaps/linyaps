# codegen

[codegen.sh](../codegen.sh) use [quicktype] to
generate json (de)serializing code in libs/runtime/include
from [runtime-spec] and some extra types we need.

[quicktype]: https://github.com/glideapps/quicktype
[runtime-spec]: https://github.com/opencontainers/runtime-spec

The generated code needs to apply a little [patch](./fix.patch),
as [quicktype] is not working very well.

You can recreate that patch follow these instructions:

1. Remove old patch

   ```bash
   rm ./fix.patch
   ```

2. Run codegen.sh

   ```bash
   ./codegen.sh
   ```

3. Run create-patch.sh, it will start a new shell.

   ```bash
   ./create-patch.sh
   ```

4. Change the code as your wish and leave `libs/runtime/include.orig` untouched

5. Exit

   ```bash
   exit
   ```
