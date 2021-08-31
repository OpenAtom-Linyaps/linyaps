# 玲珑

玲珑（Linglong） is the container application toolkit of deepin.

玲珑是玲珑塔的简称，既表示容器能对应用有管控作用，也表明了应用/运行时/系统向塔一样分层的思想。

## Feature

- [x] No root daemon, use setuid
- [x] Standard oci runtime

## Roadmap

### Current

- [ ] OCI Standard Box
- [ ] App Manager Service
    - [ ] Offline package installer
    - [ ] Download && Install

## Dependencies

```bash
#For release based on debian
sudo apt-get install cmake build-essential libyaml-cpp-dev nlonmann-json3-dev libgtest-dev qt5-qmake qtbase5-dev
```

## Installation

Install from web:

```bash
curl https://sh.linglong.space | sh
```

Or you can install form source: [INSTALL.md](INSTALL.md)

## Getting help

Any usage issues can ask for help via

- [Gitter](https://gitter.im/orgs/linuxdeepin/rooms)
- [IRC channel](https://webchat.freenode.net/?channels=deepin)
- [Forum](https://bbs.deepin.org)
- [WiKi](https://wiki.deepin.org/)

## Getting involved

We encourage you to report issues and contribute changes

- [Contribution guide for developers](https://github.com/linuxdeepin/developer-center/wiki/Contribution-Guidelines-for-Developers-en)
  . (English)
- [开发者代码贡献指南](https://github.com/linuxdeepin/developer-center/wiki/Contribution-Guidelines-for-Developers) (中文)

## License

This project is licensed under [GPLv3]().

## Credits and references

- [OStree](https://github.com/ostreedev/ostree)
