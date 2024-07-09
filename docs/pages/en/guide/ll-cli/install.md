<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Install Linglong Apps

Use `ll-cli install` to install Linglong apps.

View the help information for the `ll-cli install` command:

```bash
ll-cli install --help
```

Here is the output:

```text
Usage: ll-cli [options] install com.deepin.demo

Options:
  -h, --help                           Displays help on commandline options.
  --help-all                           Displays help including Qt specific
                                       options.
  --repo-point <--repo-point=flatpak>  app repo type to use
  --nodbus                             execute cmd directly, not via dbus(only
                                       for root user)
  --channel <--channel=linglong>       the channel of app
  --module <--module=runtime>          the module of app

Arguments:
  install                              install an application
  appId                                application id
```

Example of the `ll-cli install` command to install a Linglong app:

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
install org.deepin.calculator , please wait a few minutes...
org.deepin.calculator is installing...
message: install org.deepin.calculator, version:5.7.21.4 success
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

- Install by running `.uab` directly
```bash
./com.baidu.baidunetdisk_x86_64_4.17.7.0_main.uab
```

You can use the command `ll-cli list | grep com.baidu.baidunetdisk` to check if it has been installed successfully.

Run the application using the following command.

```bash
ll-cli run com.baidu.baidunetdisk
```
