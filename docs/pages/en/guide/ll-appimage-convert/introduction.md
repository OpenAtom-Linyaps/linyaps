# ll-appimage-convert introduction

This tool is provided by the `linglong-pica` package.which provides the ability to convert appimage packages into linyaps packages, generate the linglong.yaml file required to build linyaps applications, and rely on ll-builder to implement application building and export.

:::tip

The conversion tool is merely an auxiliary tool and does not guarantee
that the converted application will definitely run. It's possible that
the software depends on libraries installed in paths or other
configuration paths that do not align with those inside linyaps's
internal structure, leading to the inability to execute. In such cases,
you would need to use the command `ll-builder run --exec bash` to enter the container for debugging purposes.
:::

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
