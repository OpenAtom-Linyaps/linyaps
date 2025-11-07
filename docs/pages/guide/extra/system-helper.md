# 系统助手

无特权的用户可以通过以下方式将 erofs 挂载到目录：

```bash
busctl --system call org.deepin.linglong.SystemHelper \
    /org/deepin/linglong/FilesystemHelper \
    org.deepin.linglong.FilesystemHelper \
    Mount \
    sssa{sv} \
    '/var/lib/linglong/vfs/blobs/6f6b34b09e8a72ec52013407289064f5' \
    '/run/user/1000/linglong/vfs/layers/com.qq.music/1.1.3/x86_64/' \
    'erofs' \
    0
