% ll-pica-adep 1

## NAME

ll\-pica\-adep - 给 linglong.yaml 文件添加包依赖

## SYNOPSIS

**ll-pica adep** [*flags*]

## DESCRIPTION

ll-pica adep 命令用于给 linglong.yaml 文件添加包依赖。玲珑应用可能缺少包依赖，可以通过此命令在 linglong.yaml 文件添加对应的包依赖。

## OPTIONS

**-d, --deps** _string_
: 要添加的依赖项，分隔符为 ','

**-p, --path** _string_
: linglong.yaml 的路径（默认为 "linglong.yaml"）

**-V, --verbose**
: 详细输出

## EXAMPLES

添加多个依赖项：

```bash
ll-pica adep -d "dep1,dep2" -p /path/to/linglong.yaml
```

在 linglong.yaml 所在路径中执行（不需要添加 -p 参数）：

```bash
ll-pica adep -d "dep1,dep2"
```

## SEE ALSO

**[ll-pica(1)](ll-pica.md)**

## HISTORY

2024年，由 UnionTech Software Technology Co., Ltd. 开发
