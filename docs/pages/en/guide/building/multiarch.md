# Linyaps Multi-Architecture Build Guide

## Supported Architectures

The current Linyaps packaging tool supports the following CPU architectures:

- x86_64

- arm64 (aarch64)

- loong64

- loongarch64 (Loongson old world)

- sw64 (Sunway)

- mips64

## Build Limitation Description

Cross-architecture cross-compilation is not supported: currently can only build packages for that architecture on machines of the corresponding architecture

## Multi-Architecture Project Structure Recommendation

It is recommended to use the following directory structure to manage multi-architecture builds:

```txt
project-root/
├── arm64
│   └── linglong.yaml # arm64 architecture configuration file
├── linglong.yaml # x86_64 architecture configuration file
├── loong64
│   └── linglong.yaml # loong64 architecture configuration file
├── resources # Shared resource files
└── src # Shared source code
```

Place source code, resource files, etc. in the project root directory. The build for different architectures is determined by the configuration files for the respective architectures.

## Build Command Examples

ll-builder will prioritize finding the project configuration file for the current architecture. Therefore, executing ll-builder on machines of different architectures will automatically use the configuration file corresponding to the current architecture. If the architecture configuration file is not in the default location, you can use the following method to specify the configuration file location:

```bash
# Build arm64 architecture package
ll-builder -f arm64/linglong.yaml

# Build loong64 architecture package
ll-builder -f loong64/linglong.yaml
```

Note that the project configuration file (the linglong.yaml file specified by -f) needs to be in the directory or subdirectory of the project directory (the current directory where ll-builder is run).

## Loongson

For differences between Loongson's old and new worlds, see [Are We Loong Yet](https://areweloongyet.com/docs/old-and-new-worlds/)

> You can use the file tool to conveniently check which world a binary program belongs to. Suppose you want to check a file called someprogram, just execute file someprogram. If the output line contains these phrases:
> interpreter /lib64/ld.so.1, for GNU/Linux 4.15.0
> it indicates this is an old world program.
> Accordingly, if the output line contains these phrases:
> interpreter /lib64/ld-linux-loongarch-lp64d.so.1, for GNU/Linux 5.19.0

Given that the differences between old and new worlds are quite significant, Linyaps divides old and new worlds into two architectures. The new world uses the more concise loong64 as the architecture code name.

Considering that there are still some applications supporting Loongson on the market that have not been adapted to the new world, Linyaps has created old world bases (org.deepin.foundation/20.0.0) and runtime (org.deepin.Runtime/20.0.0.12) that can run on new world architectures to migrate applications supporting old world to new world.

Migration steps:

1. Prepare a new world architecture machine and install deepin or uos system.
2. Install the `liblol-dkms` kernel module on the host machine: `apt install liblol-dkms`.
3. Write a linglong.yaml file, fill in the base and runtime versions mentioned above.
4. Extract the old world application software package in linglong.yaml and copy it to the `$PREFIX` directory.
