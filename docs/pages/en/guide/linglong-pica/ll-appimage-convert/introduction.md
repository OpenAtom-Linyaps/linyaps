# ll-appimage-convert Introduction

This tool currently provides the capability to convert `appimage` packages into Linglong packages. Generate the required `linglong.yaml` file for building Linglong applications and rely on `ll-builder` to implement application build and export.

View the help information for the `ll-appimage-convert` command:

```bash
ll-appimage-convert --help
```

Here is the output:

```bash
Convert the appimage to uab. For example:
 Simple:
         ll-appimage-convert  convert -f xxx.appimage -i "io.github.demo" -n "io.github.demo" -v "1.0.0.0" -d "this is a appimage convert demo" -b
         ll-appimage-convert help

Usage:
  ll-appimage-convert [command]

Available Commands:
  convert     Convert appimage to uab
  help        Help about any command

Flags:
  -h, --help      help for ll-appimage-convert
  -V, --verbose   verbose output
  -v, --version   version for ll-appimage-convert

Use "ll-appimage-convert [command] --help" for more information about a command.
```
