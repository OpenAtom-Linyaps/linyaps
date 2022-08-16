# Ref

## 定义

组件：组件一般等价于其他系统中的 artifacts/component/module/package 等，在玲珑中，我们使用 layer 来表示一个组件。

## 格式

ref 是组件在存储仓库中的唯一标识（存储仓库中的索引信息）。一条完整的 ref 信息包括仓库名、渠道、ID、版本号、架构、分支名等信息。

ref 格式如下：

```bash
${repo}/${channel}:${id}/${version}/${arch}
```

示例：

```bash
deepin/main:org.deepin.runtime/20/x86_64

deepin/project1:org.deepin.calculator/1.2.2/x86_64
```

其中 deepin 为软件包所属的远端仓库名称。main 和 project1 为分发渠道，默认可以为空。

org.deepin.calculator 为 ID，1.2.2 为版本，x86_64 为架构。

### 范围

| 标识    | 取值范围                                                                                   |
| ------- | ------------------------------------------------------------------------------------------ |
| repo    | 小写字母 + 下划线                                                                            |
| channel | 小写字母 + 下划线                                                                            |
| id      | 英文倒置域名                                                                               |
| version | 4 位数字，使用`.`分隔。不足 4 位的补 0，超过 4 位的将后续的数字字符串拼接到第 4 位数字中。 |
| arch    | 架构描述字符串，目前支持 x86_64/arm/loongarch                                              |

## 使用

Ref 包含两个部分，远程地址标识和本地标识。其中`:`前用分割远程标识和本地标识。

运程标识主要用于定义这个 layer 是如何获取的。其中 channel 暂时没有用到，可以为空。repo 表示远程仓库（URL）在本地的映射的别名。

在安装过程中，使用完整的 ref 可以唯一标识一个 layer。例如：

```bash
ll-cli install deepin/main:org.deepin.calculator/1.2.2/x86_64
```

一旦 layer 被安装后，所以针对 layer 的本地操作都不会改变其远程属性。特别是进行升级的时候。

```bash
# 从 deepin/main升级org.deepin.calculator，其版本会发生变化，但是repo和channel不会变化
# 注意下这里install会更新包
ll-cli install org.deepin.calculator/1.2.2/x86_64

# 这里会使用deepin/project这个channel的layer替换本地的org.deepin.calculator/1.2.2/x86_64
ll-cli install deepin/project:org.deepin.calculator/1.2.2/x86_64
```

注意：

- 切换 repo 或 channel 时，本地版本都需要被删除，特别是 id 下有多个版本时。（底层可以考虑复用，但是对上层逻辑上需要保证 layers 版本都在对应的 channel 下面）。

### 简写

Ref 支持进行简写，主要规则如下：

1. 默认 repo 为 deepin
2. 默认 channel 为 main（当前未实现 channel 支持，暂时不考虑）
3. 默认版本为远程最新版本或者本地的最新版本，根据场景进行推断，如果有歧义则报错。
4. 默认架构为当前 ll-cli 运行架构

如果有显示指定 ref 中任意一个字段，那么根据这个字段为准。

## 实现

目前，layers 存储路径为：

```bash
/deepin/linglong/layers
```

这个路径后面一定会被替换，所以请务必不要有太多地方硬编码这个字段的地方。
