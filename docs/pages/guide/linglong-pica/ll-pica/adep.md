## 添加依赖

玲珑应用可能缺少包依赖，目前可以通过在 linglong.yaml 文件添加对应的包依赖。

`ll-pica adep` 命令用来给 linglong.yaml 文件添加包依赖。

查看 `ll-pica adep` 命令的帮助信息：

```bash
ll-pica adep --help
```

`ll-pica adep` 命令的帮助信息如下：

```bash
Add dependency packages to linglong.yaml

Usage:
  ll-pica adep [flags]

Flags:
  -d, --deps string   dependencies to be added, separator is ','
  -h, --help          help for adep
  -p, --path string   path to linglong.yaml (default "linglong.yaml")

Global Flags:
  -V, --verbose   verbose output
```

```bash
ll-pica adep -d "dep1,dep2" -p /path/to/linglong.yaml
```

如果在 linglong.yaml 所在路径中执行不用添加 -p 参数。
