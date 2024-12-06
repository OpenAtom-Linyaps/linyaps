# ll-flatpak-convert introduction

This tool is provided by the `linglong-pica` package.which provides the ability to convert flatpak packages into linyaps packages, generate the linglong.yaml file required to build linyaps applications, and rely on ll-builder to implement application building and export.

:::tip

The conversion tool is merely an auxiliary tool and does not guarantee
that the converted application will definitely run. It's possible that
the software depends on libraries installed in paths or other
configuration paths that do not align with those inside linyaps's
internal structure, leading to the inability to execute. In such cases,
you would need to use the command `ll-builder run --exec bash` to enter the container for debugging purposes.
:::

View the help information for the `ll-flatpak-convert` command:

```bash
ll-flatpak-convert --help
```

Here is the output:

```bash
Convert the flatpak to uab. For example:
Simple:
        ll-pica-flatpak convert [flatpak name] --build

Usage:
  ll-pica [command]

Available Commands:
  convert     Convert flatpak to uab
  help        Help about any command

Flags:
  -h, --help      help for ll-pica-flatpak
```
