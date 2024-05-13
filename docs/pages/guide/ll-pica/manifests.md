# 转换配置文件简介

package.yaml 是 `ll-pica` 转换 deb 包的基础信息。如构建的 base、runtime 的版本，需要被转换的 deb 包。

## 工程目录结构

```bash
{workdir}
├── package
│   └── {appid}
│       └── amd64
│           ├── linglong
│           ├── linglong.yaml
│           └── start.sh
└── package.yaml
```

## 字段定义

### 构建环境

deb 包转玲珑包的构建环境。

```bash
runtime:
  version: 23.0.1
  base_version: 23.0.0
  source: https://community-packages.deepin.com/beige/
  distro_version: beige
  arch: amd64
```

| 名称           | 描述                                            |
| -------------- | ----------------------------------------------- |
| runtime        | 运行时（runtime）                               |
| version        | 运行时（runtime）版本，三位数可以模糊匹配第四位 |
| base_version   | base 的版本号, 三位数可以模糊匹配第四位        |
| source         | 获取 deb 包依赖时使用的源                       |
| distro_version | 发行版的代号                                    |
| arch           | 获取 deb 包需要的架构                           |

### deb 包信息

```bash
file:
  deb:
    - type: local
      id: com.baidu.baidunetdisk
      name: com.baidu.baidunetdisk
      ref: /tmp/com.baidu.baidunetdisk_4.17.7_amd64.deb
```

| 名称 | 描述                                                  |
| ---- | ----------------------------------------------------- |
| type | 获取的方式，local 需要指定 ref，repo 不需要指定 ref。 |
| id   | 构建产物的唯一名称                                    |
| name | 指定 apt 能搜索到的正确包名                           |
| ref  | deb 包在宿主机的路径                                  |
