<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 模块拆分

在玲珑构建时，一个应用可以拆分为多个模块，每个模块包含部分构建产物，例如调试符号放到 `develop` 模块，翻译文件放到 `lang-*` 模块等。这样用户可以按需安装，缩减应用体积。

## 模块文件

开发者在 linglong.yaml 添加 modules 字段，配置如下：

```yaml
modules:
    - name: 模块1
      files: |
      /path/to/file1
      /path/to/file2
      ^/path/to/dir1/.*
    - name: 模块2
      files: |
      /path/to/files

```

模块名可自定义，但一些保留的模块名有特殊含义，例如 `binary`、`develop`、`lang-*` 等，建议开发者自定义的模块以`x-`开头，以避免冲突。

files 每行都是一个文件路径（支持正则表达式），文件会自动添加应用安装路径 `$PREFIX` 作为前缀，所以如果模块要包含 `$PREFIX/bin/demo` 文件仅需写 /bin/demo，玲珑会自动转换成 /opt/apps/org.deepin.demo/files/bin/demo 这种路径。

如果同一个文件路径被多个模块包含，仅会复制到第一个模块中(按模块名排序)，在 files 中写正则表达式需要以^开头，否则会被认为是普通文件路径，正则也会自动添加 `$PREFIX` 作为前缀，打包者无需重复添加。

## 保留模块名

### binary 模块

_这是默认模块，无需在 modules 中声明_，binary 会保存其他模块不使用的构建产物。当 modules 字段不存在时，binary 就保存所有构建产物。

用户在使用 ll-cli 安装应用时默认安装该模块，其他模块需要在安装 binary 模块后再能安装，写在 binary 模块会同时卸载其他模块。

### develop 模块

该模块可以存放用于应用的调试符号和开发工具。玲珑会在构建后将构建产物的调试符号剥离到 `$PREFIX/lib/debug` 和 `$PREFIX/lib/include` 这些目录就特别合适存放到该模块。推荐写法：

```yaml
modules:
  - name: develop
    files: |
      ^/lib/debug/.+
      ^/lib/include/.+
```

### 打包测试

`ll-builder run` 命令可以运行编译后的应用。可使用 `--modules` 参数控制运行时使用哪些模块，方便打包人员测试模块组合的工作情况。例子：

加载多个语言模块：`ll-builder run --modules lang-zh_CN,lang-ru_RU`

加载 develop 模块：`ll-builder run --modules develop`，仅作示例，不建议这样使用，因为应用的 develop 只有调试符号没有调试工具，如果需要调试应用应该使用 --debug 参数，见[IDE 中调试应用](../debug/debug.html)。

### 用户安装模块

`ll-cli install`命令可以安装应用，默认会安装应用的 binary 模块，如需安装其他模块，可以使用 `--module` 参数，注意和 `ll-builder run --modules` 不一样，ll-cli 一次仅能安装一个模块。例子：

安装语言模块：`ll-cli install --module=lang-zh_CN org.xxx.xxx/1.x.x`

安装 develop 模块：`ll-cli install --module=develop org.xxx.xxx/1.x.x`
