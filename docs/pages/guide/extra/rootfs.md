# Rootfs

根据 OCI 运行时规范，运行时从 rootfs 开始，但在玲珑中，它在 ll-box 中准备 rootfs。因此我们将构建 rootfs 的信息放在 ll-box 的注释中。

注释的示例是：

```json
  "annotations": {
    "container_root_path": "/run/user/1000/linglong/375f5681145f4f4f9ffeb3a67aebd444",
    "native": {
      "mounts": [
        {
          "destination": "/usr",
          "options": [
            "ro"
          ],
          "source": "/usr",
          "type": "bind"
        },
        {
          "destination": "/runtime",
          "options": [
            "ro"
          ],
          "source": "/deepin/linglong/layers/org.deepin.Runtime/20/x86_64",
          "type": "bind"
        },
        {
          "destination": "/opt/apps/org.deepin.music",
          "options": [
            "ro"
          ],
          "source": "/deepin/linglong/layers/org.deepin.music/6.0.1.54/x86_64/",
          "type": "bind"
        }
      ]
    },
    "overlayfs": {
      "lower_parent": "/run/user/1000/linglong/375f5681145f4f4f9ffeb3a67aebd444/.overlayfs/lower_parent",
      "upper": "/run/user/1000/linglong/375f5681145f4f4f9ffeb3a67aebd444/.overlayfs/upper",
      "workdir": "/run/user/1000/linglong/375f5681145f4f4f9ffeb3a67aebd444/.overlayfs/workdir",
      "mounts": [
        {
          "destination": "/usr",
          "options": [
            "ro"
          ],
          "source": "/usr",
          "type": "bind"
        },
        {
          "destination": "/usr",
          "options": [
            "ro"
          ],
          "source": "/usr",
          "type": "bind"
        },
        {
          "destination": "/opt",
          "options": [
            "ro"
          ],
          "source": "/opt",
          "type": "bind"
        },
        {
          "destination": "/opt/apps/com.qq.weixin.deepin",
          "options": [
            "ro"
          ],
          "source": "/opt/apps/com.qq.weixin.work.deepin/",
          "type": "bind"
        }
      ]
    }
  }
```

ll-box 支持两种构建 rootfs 的方法，使用原生只读绑定挂载或使用 overlayfs。

`container_root_path` 是 ll-box 的工作目录。
