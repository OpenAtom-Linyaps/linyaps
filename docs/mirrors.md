# 玲珑仓库 mirror 功能设计

在ll-cli和ll-builder中添加了enable-mirror和disable-mirror命令，用于启用和禁用镜像功能。

在对某个仓库启用mirror功能时，会更新ostree的config文件，添加contenturl配置项：

```diff
url=$repo_url/repos/$repo_name
gpg-verify=false
http2=false
+ contenturl=mirrorlist=$repo_url/api/v2/mirrors/$repo_name
```

当添加contenturl配置项后，ostree会优先使用contenturl下载静态文件如files, url仅用于获取元数据如summary, contenturl支持file://、http://、https://三种协议。

如果在url前面添加mirrorlist=，则ostree会先从url获取按行分割的镜像列表，然后从镜像列表中选择镜像用于下载文件，具体逻辑见 [ostree_pull](#ostree-pull-步骤) 章节。

/api/v2/mirrors/$repo_name 是玲珑服务器的一个API接口，通过客户端IP获取客户端所在国家，从配置文件获取对应国家的镜像列表，然后返回给ostree，这样就实现了根据用户所在国家自动分流仓库文件下载的功能更。

## 镜像站配置

玲珑的镜像站只提供仓库静态文件的https访问即可，玲珑仓库支持rsync和ostree两种同步协议。

### 使用 rsync 同步配置

优点是同步速度快，缺点是需从支持rsync协议的镜像站或官方仓库同步仓库。

```bash
rsync rsync://rsync.linyaps.org.cn/repos/stable $www_root/repos/stable
```

### 使用 ostree 同步配置

优点是无需协议支持，可从任意镜像站同步仓库，缺点是同步速度较慢。

保存下面脚本，并命名为 sync.sh，然后执行`sh sync.sh https://mirror-repo-linglong.deepin.com/repos/stable/ $www_root/repos/stable`

```bash
#!/bin/bash
set -e
url=$1
dir=$2
echo sync $url to $dir
sleep 3
ostree init --repo=$dir --mode archive
ostree --repo=$dir remote add --if-not-exists --no-sign-verify remote $url
for ref in $(ostree --repo=$dir remote refs remote); do
    echo pull $ref;
    ostree --repo=$dir pull --mirror $ref;
done
```

## ostree pull 步骤

### 判断镜像是否可用

在pull时，ostree 先从contenturl获取镜像列表，然后从每个url获取/config文件，如果获取不到/config文件，则认为该mirror不可用，如果获取到/config文件，则认为该mirror可用。如果没有可用mirror，pull失败。

### 获取summary文件

ostree会从url获取summary文件，如果获取不到summary文件，或者summary文件不存在ref，pull失败。

### delta-indexes文件获取

ostree会在每个可用的mirror中获取delta-indexes，如果mirror服务器返回4xx或5xx，则在下一个mirror中获取delta-indexes，如果最后的mirror返回5xx，则pull失败，如果最后的mirror返回4xx，则跳过dalta步骤直接拉取files。

### files文件获取

ostree会按顺序从可用的mirror中获取files，如果mirror服务器返回403, 404， 410，则认为错误不可恢复，pull失败，如果mirror服务器返回其他错误码，则使用下一个mirror获取files。如果所有mirror都无法获取files，则pull失败。
