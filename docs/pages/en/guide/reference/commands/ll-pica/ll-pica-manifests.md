# Conversion Configuration File Introduction

`package.yaml` is the basic information file for `ll-pica` to convert deb packages. It contains information such as the base and runtime versions to be built, and the deb packages that need to be converted.

## Project Directory Structure

```bash
{workdir}
├── package
│   └── {appid}
│       ├── linglong
│       ├── linglong.yaml
│       └── start.sh
└── package.yaml
```

## Field Definitions

### Build Environment

The build environment for converting deb packages to Linglong packages.

```bash
runtime:
  version: 23.0.1
  base_version: 23.0.0
  source: https://community-packages.deepin.com/beige/
  distro_version: beige
  arch: amd64
```

| Name           | Description                                                               |
| -------------- | ------------------------------------------------------------------------- |
| runtime        | Runtime environment                                                       |
| version        | Runtime version, three-digit version can fuzzy match the fourth digit     |
| base_version   | Base version number, three-digit version can fuzzy match the fourth digit |
| source         | Source used when obtaining deb package dependencies                       |
| distro_version | Distribution code name                                                    |
| arch           | Architecture required for obtaining deb packages                          |

### Deb Package Information

```bash
file:
  deb:
    - type: local
      id: com.baidu.baidunetdisk
      name: com.baidu.baidunetdisk
      ref: /tmp/com.baidu.baidunetdisk_4.17.7_amd64.deb
```

| Name | Description                                                                              |
| ---- | ---------------------------------------------------------------------------------------- |
| type | Acquisition method, local requires specifying ref, repo does not require specifying ref. |
| id   | Unique name of the build product                                                         |
| name | Specify the correct package name that apt can search for                                 |
| ref  | Path of the deb package on the host machine                                              |
