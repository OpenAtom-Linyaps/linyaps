# ll-box

ll-box is a standard oci runtime.

## Feature

- [x] No root daemon, use setuid
- [x] Standard oci runtime

## Roadmap

### Current

- [ ] Configuration
    - [x] Root
    - [ ] Mount
        - [ ] Cgroup
    - [x] Hostname
- [ ] Linux Container
    - [x] Default Filesystems
    - [x] Namespaces
        - [x] Network
    - [x] User namespace mappings
    - [ ] Devices
    - [ ] Default Devices
    - [ ] Control groups
    - [ ] IntelRdt
    - [ ] Sysctl
    - [ ] Seccomp
    - [ ] Rootfs Mount Propagation
    - [ ] Masked Paths
    - [ ] Readonly Paths
    - [ ] Mount Label
- [ ] Extend
    - [x] Container manager
    - [x] Debug
    - [ ] DBus proxy permission control
    - [ ] Filesystem permission control base fuse
    - [ ] X11&&Wayland security

### Next

- [ ] TODO

## Dependencies

- [ ] C++11/STL
- [ ] nlonmann-json3
- [ ] gtest

```bash
#For release based on debian
sudo apt-get cmake build-essential \
  nlonmann-json3-dev \
  libgtest-dev
```

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

Deepin Boot Maker is licensed under GPLv3.

## Credits and references

- [OStree](https://github.com/ostreedev/ostree)
