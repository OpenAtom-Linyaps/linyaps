# 简单使用
## 远程仓库

TODO

当前remote仓库地址如下：

```plain
https://linglong-api-dev.deepin.com/ostree
```
## 应用查询

通过query命令可以从远程repo中找到应用程序信息，如:

```plain
ll-cli query music
```
该命令将返回appid（appid是应用唯一标识）中包含music关键词的所有应用程序信息，包含完整的appid、应用程序名称、版本、平台及应用描述信息。
## 应用安装

要安装应用，请运行install命令，如：

```plain
ll-cli install org.deepin.music
```
install命令需要输入应用完整的appid，若仓库有多个版本则会默认安装最高版本；指定版本安装需在appid后附加对应版本号，如：
```plain
ll-cli install org.deepin.music/5.1.2
```
## 应用运行

当应用被正常安装后，使用run命令及appid即可启动：

```plain
ll-cli run org.deepin.music
```
## 应用更新

TODO

## 已装列表

要查看已安装的runtime和应用，运行list命令：

```plain
ll-cli list
```
## 应用卸载

删除应用程序，使用uninstall命令：

```plain
ll-cli uninstall org.deepin.music
```
该命令执行成功后，应用程序将从系统中被移除，但并不包含用户使用该应用过程中产生的数据信息。
