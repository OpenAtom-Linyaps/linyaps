# Initialization configuration

The `ll-pica init` command is used to initialize the configuration information for the conversion package.

View the help information for the `ll-cli init` command:

```bash
ll-pica init --help
```

Here is the output:

```bash
init config template

Usage:
  ll-pica init [flags]

Flags:
  -a, --arch string      runtime arch
  -c, --config string    config file
      --dv string        distribution Version
  -h, --help             help for init
      --pi string        package id
      --pn string        package name
  -s, --source string    runtime source
  -t, --type string      get type
  -v, --version string   runtime version
  -w, --workdir string   work directory

Global Flags:
  -V, --verbose   verbose output
```

The specific command as follows:

```bash
ll-pica init -w w --pi com.baidu.baidunetdisk --pn com.baidu.baidunetdisk -t repo
```

- -w working directory
- --pi specifies the appid used by the Linglong application.
- --pn specifies the correct package name that apt can search for.
- -t specifies the type to retrieve, `repo` fetches from the apt repository.

The specific configuration is as follows:

```bash
runtime:
  version: 23.0.1
  base_version: 23.0.0
  source: https://community-packages.deepin.com/beige/
  distro_version: beige
  arch: amd64
file:
  deb:
    - type: repo
      id: com.baidu.baidunetdisk
      name: com.baidu.baidunetdisk
```

Detailed Field Reference: [Manifests](../manifests.md)

:::tip
The default configuration file `~/.pica/config.json`
 is set to use Deepin v23. If you need to specify UOS 20 as the BASE and
 RUNTIME, modify the default configuration using the following command.
Please update the link below, [https://professional-packages.chinauos.com/desktop-professional](https://professional-packages.chinauos.com/desktop-professional), to a version that does not require authentication.
:::

```bash
ll-pica init --rv "20.0.0" --bv "20.0.0" -s "https://professional-packages.chinauos.com/desktop-professional" --dv "eagle/1070"
```

If it needs to be used on arm64, the default architecture needs to be modified.

```bash
ll-pica init -a "arm64"
```
