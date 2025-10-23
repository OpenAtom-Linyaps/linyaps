<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Module Splitting

When building with Linyaps, an application can be split into multiple modules, each containing part of the build artifacts. For example, debug symbols go to the `develop` module, translation files go to `lang-*` modules, etc. This allows users to install on demand and reduce application size.

## Module Files

Developers add a modules field in linglong.yaml and configure as follows:

```yaml
modules:
  - name: module1
    files:
      - /path/to/file1
      - /path/to/file2
      - ^/path/to/dir1/.*
  - name: module2
    files:
      - /path/to/files
```

Module names can be customized but cannot be duplicated. Some reserved module names have special meanings, such as `binary`, `develop`, `lang-*`, etc. It is recommended that developer-customized modules start with `x-` to avoid conflicts.

`files` is a string array, each entry writes one file path, supporting regular expressions. File paths will automatically have the application installation path `$PREFIX` added as a prefix. So if a module wants to include the `$PREFIX/bin/demo` file, only `/bin/demo` needs to be written, and Linyaps will automatically convert it to paths like `/opt/apps/org.deepin.demo/files/bin/demo`.

If the same file path is included by multiple modules, the file will only be moved to the first module (in the order of modules). When writing regular expressions in files, they need to start with `^`, otherwise they will be considered as ordinary file paths. Regular expressions will automatically add `$PREFIX` as a prefix after `^`, and packagers don't need to add it repeatedly.

## Reserved Module Names

### binary module

_This is the default module and does not need to be declared in modules._ The binary module saves build artifacts not used by other modules. When the modules field does not exist, binary saves all build artifacts.

When users install applications using ll-cli, the binary module is installed by default. Other modules can only be installed after installing the binary module. Uninstalling the binary module will uninstall other modules at the same time.

### develop module

This module can store debug symbols and development tools for applications. Linyaps will separate build artifact debug symbols to directories like `$PREFIX/lib/debug` and `$PREFIX/lib/include` after building. These directories are particularly suitable for storing in this module. So when Linyaps builds, if there is no modules field or the modules do not have a develop module, it will automatically create a develop module and use the following default values:

```yaml
modules:
  - name: develop
    files:
      # Separated debug symbols
      - ^/lib/debug/.+
      # Header files
      - ^/lib/include/.+
      # Static link libraries
      - ^/lib/.+\.a$
```

If you need to customize the develop module, you can add develop module configuration in the `modules` field to override the default values.

### Packaging Testing

The `ll-builder run` command can run compiled applications. You can use the `--modules` parameter to control which modules are used at runtime, which is convenient for packagers to test module combinations. Examples:

Load multiple language modules: `ll-builder run --modules lang-zh_CN,lang-ru_RU`

Load develop module: `ll-builder run --modules develop`, only as an example, not recommended for actual use, because the application's develop only has debug symbols without debugging tools. If you need to debug applications, you should use the --debug parameter, see [Debugging Applications in IDE](../debug/debug.md).

### User Installation of Modules

The `ll-cli install` command can install applications. By default, it will install the binary module of the application. If you need to install other modules, you can use the `--module` parameter. Note that unlike `ll-builder run --modules`, ll-cli can only install one module at a time. Examples:

Install language module: `ll-cli install --module=lang-zh_CN org.xxx.xxx`

Install develop module: `ll-cli install --module=develop org.xxx.xxx`
