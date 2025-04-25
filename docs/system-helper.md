# System Helper

User with no privilege can mount erofs to dir with:

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
```
