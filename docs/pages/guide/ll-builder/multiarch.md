# 玲珑多架构构建指南

### 支持的架构

当前玲珑打包工具支持以下 CPU 架构：

- x86_64

- arm64（aarch64）

- loong64

- loongarch64（龙芯旧世界）

- sw64（申威）

- mips64

### 构建限制说明

不支持跨架构交叉编译：目前只能在对应架构的机器上构建该架构的包

### 多架构项目结构建议

推荐采用以下目录结构管理多架构构建：

```txt
项目根目录/
├── linglong.x86_64.yaml # x86_64 架构配置文件
├── linglong.arm64.yaml # arm64 架构配置文件
├── linglong.loong64.yaml # loong64 架构配置文件
├── src/ # 共享的源代码
└── resources/ # 共享的资源文件
```

将源代码、资源文件等放在项目根目录，不同架构的构建由对应架构的配置文件决定。

### 构建命令示例

ll-builder 会优先查找当前架构的项目配置文件，因此在不同架构的机器上执行 ll-builder 会自动使用当前架构对应的配置文件。如果架构配置文件不在默认位置，可以使用下面的方法指定配置文件的位置：

```bash
# 构建 arm64 架构包
ll-builder -f arm64/linglong.yaml

# 构建 loongarch64 架构包
ll-builder -f loongarch64/linglong.yaml
```

需要注意项目配置文件（即由 -f 指定的 linglong.yaml 文件），需要在项目目录（运行 ll-builder 的当前目录）的目录或者子目录下。

### 龙芯

龙芯新旧世界的差异可查看 [咱龙了吗](https://areweloongyet.com/docs/old-and-new-worlds/)

> 可以使用 file 工具方便地检查一个二进制程序属于哪个世界。 假设你想检查 someprogram 这个文件，就执行 file someprogram，如果输出的行含有这些字样：
> interpreter /lib64/ld.so.1, for GNU/Linux 4.15.0
> 就表明这是一个旧世界程序。
> 相应地，如果输出的行含有这些字样：
> interpreter /lib64/ld-linux-loongarch-lp64d.so.1, for GNU/Linux 5.19.0

介于新旧世界差异不小，在玲珑中新旧世界被分为两个架构，新世界使用更简洁的 loong64 作为架构代号。

考虑到市面上还有部分支持龙芯的应用未适配新世界，玲珑制作了可在新世界架构运行的旧世界的 base(org.deepin.foundation/20.0.0) 和 runtime(org.deepin.Runtime/20.0.0.12)，用来将支持旧世界的应用迁移到新世界。

迁移步骤：

1. 准备一个新世界架构的机器并安装 deepin 或 uos 系统。
2. 在宿主机安装 `liblol-dkms` 内核模块 `apt install liblol-dkms`。
3. 编写一个 linglong.yaml 文件，base 和 runtime 填写上文所属的版本号。
4. 在 linglong.yaml 里解压旧世界应用的软件包，复制到 `$PREFIX` 目录。
