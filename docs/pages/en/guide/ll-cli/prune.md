<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Remove the unused base or runtime

Use `ll-cli prune` to remove the unused base or runtime.

View the help information for the `ll-cli prune` command:

```bash
ll-cli prune --help
```

Here is the output:

```text
Remove the unused base or runtime
Usage: ll-cli prune [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  --help-all                  Expand all help

If you found any problems during use,
You can report bugs to the linyaps team under this project: https://github.com/OpenAtom-Linyaps/linyaps/issues
```

Use `ll-cli prune` to remove the unused base or runtime

Here is the output:

```text
Unused base or runtime:
main:org.deepin.Runtime/23.0.1.2/x86_64
main:org.deepin.foundation/20.0.0.27/x86_64
main:org.deepin.Runtime/23.0.1.0/x86_64
main:org.deepin.foundation/23.0.0.27/x86_64
main:org.deepin.Runtime/20.0.0.8/x86_64
5 unused base or runtime have been removed.
```
