# Manifests

The `package.yaml` file serves as the foundation for ll-pica to convert packages into DEB format. It encompasses essential information such as the base and runtime versions used in the build process, as well as the DEB package that is to be converted.

## Project directory structure

```bash
{workdir}
├── package
│   └── {appid}
│       ├── linglong
│       ├── linglong.yaml
│       └── start.sh
└── package.yaml
```

## Field definitions

### Build environment

Conversion build environment for DEB packages to linyaps packages.

```bash
runtime:
  version: 23.0.1
  base_version: 23.0.0
  source: https://community-packages.deepin.com/beige/
  distro_version: beige
  arch: amd64
```

| name           | description                                                                                |
| -------------- | ------------------------------------------------------------------------------------------ |
| version        | Runtime version, A three-digit number can be loosely matched with a potential fourth digit |
| base_version   | Base version, A three-digit number can be loosely matched with a potential fourth digit    |
| source         | Obtain the sources used by the dependencies of a deb package.                              |
| distro_version | The codename of a distribution."                                                           |
| arch           | The architecture required by a deb package.                                                |

### Deb package informationeb

```bash
file:
  deb:
    - type: local
      id: com.baidu.baidunetdisk
      name: com.baidu.baidunetdisk
      ref: /tmp/com.baidu.baidunetdisk_4.17.7_amd64.deb
```

| name | description                                                                                                                |
| ---- | -------------------------------------------------------------------------------------------------------------------------- |
| type | The method of acquisition: 'local' requires specifying a reference, while 'repo' does not require specifying a reference." |
| id   | Unique name of the build product                                                                                           |
| name | Specify the correct package name that apt can search for.                                                                  |
| ref  | The path of the deb package on the host machine.                                                                           |
