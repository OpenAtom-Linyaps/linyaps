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
本项目依赖json-struct包，此包只在玲珑项目临时仓库里面，安装依赖，请先添加一下仓库，如下：
deb  [trusted=yes] http://aptly.uniontech.com/pkg/eagle-1040/release-candidate/546y54-R5LiA5pyf6aG555uu5Li76aKYMjAyMS0wOS0xNyAyMDoxODowNg  unstable main
#For release based on debian
sudo apt update
sudo apt-get install cmake build-essential libyaml-cpp-dev nlohmann-json3-dev libgtest-dev qt5-qmake qtbase5-dev libarchive-dev libcurl4-gnutls-dev libglib2.0-dev libostree-dev libgdk-pixbuf2.0-dev
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

## Test
add --coverage in CMakeLists.txt
```bash
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
```

use gcov、lcov to generate convert html report, make shure at the top level of the project
```bash
./code_coverage.sh
```

## Doxygen生成pdf接口文档
1. 安装中文latex环境
```bash
sudo apt-get install doxygen graphviz texlive-full latex-cjk-chinese* cjk-latex
```
2. 生成配置文件
```bash
Doxygen –g
```
3. 打开Doxyfile，递归遍历当前目录的子目录，寻找被文档化的程序源文件，修改如下：
```plain
RECURSIVE              = YES
```
4. 文档生成
```bash
doxygen Doxyfile
```
5.进入到latex目录
```bash
cd latex
```
6. 修改前面doxygen Doxyfile生成的latex文件：refman.tex
将其中的:
```plain
\begin{document}
```
改为：
```plain
\usepackage{CJKutf8}
\begin{document}
\begin{CJK}{UTF8}{gbsn}
```
并将其中的:
```plain
\end{document}
```
改为：
```plain
\end{CJK}
\end{document}
```
7. 生成pdf文件
```bash
make -j
```
8. 简化上述流程，使用脚本一键在工程顶级目录下生成pdf接口文档
```bash
./doxygen_pdf.sh
```