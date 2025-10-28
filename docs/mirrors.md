# 玲珑仓库 Mirror 功能设计

在 ll-cli 和 ll-builder 中添加了 enable-mirror 和 disable-mirror 命令，用于启用和禁用镜像功能。

在对某个仓库启用 mirror 功能时，会更新 ostree 的 config 文件，添加 contenturl 配置项：

```diff
url=$repo_url/repos/$repo_name
gpg-verify=false
http2=false
+ contenturl=mirrorlist=$repo_url/api/v2/mirrors/$repo_name
```

当添加 contenturl 配置项后，ostree 会优先使用 contenturl 下载静态文件如 files，url 仅用于获取元数据如 summary。contenturl 支持 file://、http://、https:// 三种协议。

如果在 url 前面添加 mirrorlist=，则 ostree 会先从 url 获取按行分割的镜像列表，然后从镜像列表中选择镜像用于下载文件，具体逻辑见 [ostree pull 步骤](#ostree-pull-步骤) 章节。

/api/v2/mirrors/$repo_name 是玲珑服务器的一个 API 接口，通过客户端 IP 获取客户端所在国家，从配置文件获取对应国家的镜像列表，然后返回给 ostree，这样就实现了根据用户所在国家自动分流仓库文件下载的功能。

## 镜像站配置

玲珑的镜像站只需提供仓库静态文件的 HTTPS 访问即可，玲珑仓库支持 rsync 和 ostree 两种同步协议。

### 使用 rsync 同步配置

优点是同步速度快，缺点是需要从支持 rsync 协议的镜像站或官方仓库同步仓库。

```bash
rsync rsync://rsync.linyaps.org.cn/repos/stable $www_root/repos/stable
```

### 使用 ostree 同步配置

优点是无须协议支持，可从任意镜像站同步仓库，缺点是同步速度较慢。

保存下面脚本，并命名为 sync.sh，然后执行 `sh sync.sh https://mirror-repo-linglong.deepin.com/repos/stable/ $www_root/repos/stable`

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

在 pull 时，ostree 先从 contenturl 获取镜像列表，然后从每个 url 获取 /config 文件。如果获取不到 /config 文件，则认为该 mirror 不可用；如果获取到 /config 文件，则认为该 mirror 可用。如果没有可用 mirror，pull 失败。

### 获取 summary 文件

ostree 会从 url 获取 summary 文件。如果获取不到 summary 文件，或者 summary 文件中不存在 ref，pull 失败。

### delta-indexes 文件获取

ostree 会在每个可用的 mirror 中获取 delta-indexes。如果 mirror 服务器返回 4xx 或 5xx，则在下一个 mirror 中获取 delta-indexes。如果最后的 mirror 返回 5xx，则 pull 失败；如果最后的 mirror 返回 4xx，则跳过 delta 步骤直接拉取 files。

### files 文件获取

ostree 会按顺序从可用的 mirror 中获取 files。如果 mirror 服务器返回 403、404、410，则认为错误不可恢复，pull 失败；如果 mirror 服务器返回其他错误码，则使用下一个 mirror 获取 files。如果所有 mirror 都无法获取 files，则 pull 失败。
