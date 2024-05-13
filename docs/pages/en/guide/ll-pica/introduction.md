# ll-pica Introduction

This tool currently provides the capability to convert DEB packages into Linglong packages. Only software packages that comply with the app store packaging standards are supported for conversion. Packages related to Wine, Android emulators, input methods, and security software cannot be converted at this time.

View the help information for the `ll-pica` command:

```bash
ll-pica --help
```

Here is the output:

```bash
Convert the deb to uab. For example:
Simple:
 ll-pica init -c package -w work-dir
 ll-pica convert -c package.yaml -w work-dir
 ll-pica help

Usage:
  ll-pica [command]

Available Commands:
  adep        Add dependency packages to linglong.yaml
  convert     Convert deb to uab
  help        Help about any command
  init        init config template

Flags:
  -h, --help      help for ll-pica
  -V, --verbose   verbose output
  -v, --version   version for ll-pica

Use "ll-pica [command] --help" for more information about a command.
```
