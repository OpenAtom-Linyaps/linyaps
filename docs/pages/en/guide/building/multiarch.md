# Linyaps Multi-Architecture Build Guide

## Supported Architectures

The current Linyaps packaging tool supports the following CPU architectures:

- x86_64

- arm64 (aarch64)

- loong64

- loongarch64 (Loongson old world)

- sw64 (Sunway)

- mips64

## Specifying the Build Configuration File

When running `ll-builder build`, the tool searches for the project configuration file in the following order and uses the first one it finds:

1. If the `-f` parameter is specified, it uses the configuration file specified by `-f`. If the file does not exist, it will report an error and exit.
2. Based on the machine architecture of the current build environment, it searches the current directory for an architecture-specific configuration file named `linglong.<arch>.yaml` (e.g., `linglong.arm64.yaml`).
3. It searches for a `linglong.yaml` file in the current directory.

If it still cannot find a project configuration file, it will report an error and exit.

Please note that manually specified project configuration files (i.e., the `linglong.yaml` file specified by `-f`) must be located within the project directory (the current directory where `ll-builder` is executed) or one of its subdirectories.

## Recommendations for Multi-Architecture Project Structure

Depending on the actual needs of your project, you can adopt one of the following directory structures:

```txt
project_root/
├── linglong.x86_64.yaml # x86_64 architecture config file
├── linglong.arm64.yaml # arm64 architecture config file
├── linglong.loong64.yaml # loong64 architecture config file
├── linglong.yaml # Config file for other architectures
├── resources # Shared resource files
└── src # Shared source code
```

Or

```txt
project_root/
├── arm64
│   └── src_arm
│   └── linglong.yaml # arm64 architecture config file
├── linglong.yaml # Config file for other architectures
├── loong64
│   └── src_loong64
│   └── linglong.yaml # loong64 architecture config file
├── resources # Shared resource files
└── src # Shared source code
```

Place common source code and shared resource files in the project root directory, while keeping architecture-specific resources and configuration files in their respective subdirectories.

## Build Command Examples

`ll-builder` uses project configuration files according to its internal rules.

If a `linglong.<arch>.yaml` or `linglong.yaml` file exists in the current directory, you can simply run `ll-builder build`. `ll-builder` will automatically select the appropriate configuration file based on the build environment's architecture and build the corresponding Linyaps package.

If the default rules do not meet your needs, you can specify the location of the configuration file using the following methods:

```bash
# Build an arm64 package
ll-builder build -f arm64/linglong.yaml

# Build a loong64 package
ll-builder build -f loong64/linglong.yaml
```

## Cross-Building

If you are cross-building (e.g., building an `arm64` Linyaps package on an `x86_64` machine), you can manually specify the project configuration file and fill in the target architecture in the `architecture` field within the `package` block of the configuration file.

Please note that the architecture of the Linyaps build environment remains the architecture of the host machine running `ll-builder`. If you are building from source, you will need to install and configure a cross-compiler. For example:

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

1. Build the target architecture Linyaps package: `ll-builder build -f linglong.arm64.yaml`
2. Export the target architecture Linyaps package: `ll-builder export --ref main:org.deepin.example.cross_build/0.0.0.1/arm64` or `ll-builder export --layer -f linglong.arm64.yaml`.

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
