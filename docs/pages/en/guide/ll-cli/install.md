<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Install linyaps Apps

Use `ll-cli install` to install linyaps apps.

View the help information for the `ll-cli install` command:

```bash
ll-cli install --help
```

Here is the output:

```text
Installing an application or runtime
Usage: ll-cli install [OPTIONS] APP

Example:
# install application by appid
ll-cli install org.deepin.demo
# install application by linyaps layer
ll-cli install demo_0.0.0.1_x86_64_binary.layer
# install application by linyaps uab
ll-cli install demo_x86_64_0.0.0.1_main.uab
# install specified module of the appid
ll-cli install org.deepin.demo --module=binary
# install specified version of the appid
ll-cli install org.deepin.demo/0.0.0.1
# install application by detailed reference
ll-cli install stable:org.deepin.demo/0.0.0.1/x86_64


Positionals:
  APP TEXT REQUIRED           Specify the application ID, and it can also be a .uab or .layer file

Options:
  -h,--help                   Print this help message and exit
  --help-all                  Expand all help
  --module MODULE             Install a specify module
  --force                     Force install the application
  -y                          Automatically answer yes to all questions

If you found any problems during use,
You can report bugs to the linyaps team under this project: https://github.com/OpenAtom-Linyaps/linyaps/issues
```

Example of the `ll-cli install` command to install a linyaps app:

```bash
ll-cli install <org.deepin.calculator>
```

Enter the complete `appid` after `ll-cli install`. If the repository has multiple versions, the highest version will be installed by default.

To install a specified version, append the corresponding version number after `appid`:

```bash
ll-cli install <org.deepin.calculator/5.1.2>
```

Here is the output of `ll-cli install org.deepin.calculator`:

```text
Install main:org.deepin.calculator/5.7.21.4/x86_64 success:100%
```

After the application is installed, the installation result will be displayed.

The layer or uab files we export using the `ll-builder export` command can be installed using the `ll-cli install` command.

`.layer`
```bash
ll-cli install ./com.baidu.baidunetdisk_4.17.7.0_x86_64_runtime.layer
```

`.uab`
There are two ways to install uab files
- use `ll-cli install` to install
```bash
ll-cli install com.baidu.baidunetdisk_x86_64_4.17.7.0_main.uab
```

- Execute `uab` on a machine with linyaps environment to install the application.
```bash
./com.baidu.baidunetdisk_x86_64_4.17.7.0_main.uab
```

You can use the command `ll-cli list | grep com.baidu.baidunetdisk` to check if it has been installed successfully.

Run the application using the following command.

```bash
ll-cli run com.baidu.baidunetdisk
```
