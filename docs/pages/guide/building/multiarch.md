# 玲珑多架构构建指南

## 支持的架构

当前玲珑打包工具支持以下 CPU 架构：

- x86_64

- arm64（aarch64）

- loong64

- loongarch64（龙芯旧世界）

- sw64（申威）

- mips64

## 构建配置文件的指定

`ll-builder build` 时会按照下面的顺序查找项目配置文件，并使用第一个找到的项目配置文件。

1. 如果指定了 `-f` 参数，使用 `-f` 指定的项目配置文件，如果文件不存在则报错退出。
2. 根据当前构建环境的机器架构，在当前目录查找架构相关的项目配置文件，名字为 `linglong.<arch>.yaml`，比如 `linglong.arm64.yaml`。
3. 查找当前目录下的 `linglong.yaml` 文件。

如果依然找不到项目配置文件则报错退出。

需要注意手动指定的项目配置文件（即由 `-f` 指定的 linglong.yaml 文件），需要在项目目录（运行 ll-builder 的当前目录）或其子目录下。

## 多架构项目结构建议

根据项目实际情况确定项目结构，可以采用以下目录结构：

```txt
项目根目录/
├── linglong.x86_64.yaml # x86_64 架构配置文件
├── linglong.arm64.yaml # arm64 架构配置文件
├── linglong.loong64.yaml # loong64 架构配置文件
├── linglong.yaml # 其他架构配置文件
├── resources # 共享的资源文件
└── src # 共享的源代码
```

或者

```txt
项目根目录/
├── arm64
│   └── src_arm
│   └── linglong.yaml # arm64 架构配置文件
├── linglong.yaml # 其他架构配置文件
├── loong64
│   └── src_loong64
│   └── linglong.yaml # loong64 架构配置文件
├── resources # 共享的资源文件
└── src # 共享的源代码
```

将公共源代码、资源文件等放在项目根目录，不同架构的资源和配置文件放入到对应架构的子目录中。


## 构建命令示例

`ll-builder` 会按照规则使用项目配置文件。

如果当前目录存在 `linglong.<arch>.yaml` 文件或者 `linglong.yaml`，则直接运行 `ll-builder build` 即可，`ll-builder` 会根据构建环境的架构选择合适的项目配置文件，并构建对应架构的玲珑包。

如果默认规则不满足需要，可以使用下面的方法指定配置文件的位置：

```bash
# 构建 arm64 架构包
ll-builder build -f arm64/linglong.yaml

# 构建 loong64 架构包
ll-builder build -f loong64/linglong.yaml
```

## 交叉构建

如果使用交叉构建（比如在 x86_64 架构机器上构建 arm64 玲珑包），则可以手动指定项目配置文件，并在项目配置文件 `package` 配置块中的 `architecture` 填写构建的目标架构。需要注意的是，玲珑构建环境架构仍然是当前运行 `ll-builder` 机器的架构，如果从源码构建，需要安装配置交叉编译器。比如：

``` yaml
# linglong.arm64.yaml
version: "1"

package:
  id: org.deepin.example.cross_build
  name: your name #set your application name
  version: 0.0.0.1 #set your version
  kind: app
  description: |
    your description #set a brief text to introduce your application.
  architecture: arm64

command: [program] #the commands that your application need to run.

base: org.deepin.base/25.2.1

build: |
  cat > program.c << 'EOF'
  #include <stdio.h>
  
  int main() {
      printf("From ll-builder!\n");
      return 0;
  }
  EOF
  
  # cross compile
  aarch64-linux-gnu-gcc-12 program.c -o program

  rm -f program.c

  mkdir -p $PREFIX/bin
  cp program $PREFIX/bin/

buildext:
  apt:
    build-depends: [gcc-12-aarch64-linux-gnu]
```

1. 构建目标架构玲珑包：`ll-builder build -f linglong.arm64.yaml`
2. 导出目标架构玲珑包：`ll-builder export --ref main:org.deepin.example.cross_build/0.0.0.1/arm64` 或者 `ll-builder export --layer -f linglong.arm64.yaml`。

## 龙芯

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
