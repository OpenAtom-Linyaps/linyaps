# crp平台从源码构建玲珑应用

`crp`平台已集成`ll-builder`，支持一键从源码构建出玲珑应用并推送到玲珑测试仓库。

主要步骤:

1. 源码工程目录下新增`linglong.yaml`配置文件

2. `crp`平台配置

{'build': 'll-builder'}  构建参数这里需要填写前面这个参数用户shuttle节点判断构建使用, 如下图:

![构建参数列表](../images/build_parameter_list.png)

打包时，节点需要填写`llbuilder`指定对应节点（只有该节点有`ll-builder`运行环境）才可进行玲珑构建。如下图：

![节点](../images/node.png)
