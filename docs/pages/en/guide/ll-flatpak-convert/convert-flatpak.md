## Conversion Flatpak application

The `ll-pica-flatpak convert` command is used to convert Flatpak applications into Linglong applications.

View the help information for the `ll-pica-flatpak convert` command:

```bash
ll-pica-flatpak --help
```

Here is the output:

```bash
Convert the flatpak to uab. For example:
Simple:
        ll-pica-flatpak convert [flatpak name] --build

Usage:
  ll-pica-flatpak [command]

Available Commands:
  convert     Convert flatpak to uab
  help        Help about any command

Flags:
  -h, --help      help for ll-pica-flatpak
```

Convert Flatpak application

```bash
ll-pica-flatpak convert org.videolan.VLC --build
```

:::tip

The package name for Flatpak is org.videolan.VLC. It can be found by visiting [https://flathub.org/](https://flathub.org/) => clicking on the app => selecting the Install drop-down menu => viewing the package name.

ll-pica-flatpak uses ostree commands to retrieve the application data of org.videolan.VLC, and generates the corresponding Linglong base environment based on the runtime defined in the metadata

:::

The application defaults to generating a uab file. To export a layer file, you need to add the --layer parameter.

```bash
ll-pica-flatpak convert org.videolan.VLC --build --layer
```

To specify the version of the Linglong application to be generated, you need to add the --version parameter.

```bash
ll-pica-flatpak convert org.videolan.VLC --version "1.0.0.0" --build --layer
```

To specify the base environment and version of the Linglong application, you need to add the --base and --base-version parameters.

```bash
ll-pica-flatpak convert org.videolan.VLC --base "org.deepin.base.flatpak.kde" --base-version "6.7.0.2" --build --layer
```

The constructed products are as follows:

```bash
├── org.videolan.VLC
│   ├── org.videolan.VLC_1.0.0.0_x86_64_develop.layer
│   ├── org.videolan.VLC_1.0.0.0_x86_64_binary.layer
    or
│   ├── org.videolan.VLC_x86_64_1.0.0.0_main.uab
```

Layer files are divided into two categories: `binary` and `develop`. The `binary` includes the application's execution environment, while the `develop` layer, built upon the `binary`, retains the debugging environment.

The uab file is an offline distribution format used by the LingLong software package, which is not suitable for systems that can normally connect to the LingLong repository. Instead, one should utilize the delta transfer scheme provided by the LingLong software repository to reduce the network transmission size.

Installing Layer Files and Running the Application Reference：[Install Linglong Apps](../ll-cli/install.md)
