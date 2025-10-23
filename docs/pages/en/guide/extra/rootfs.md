# Root Filesystem (Rootfs)

As per the OCI runtime specification, the runtime starts from a rootfs, but in Linyaps, it prepares rootfs in ll-box. Therefore, we put the information for constructing rootfs in annotations to ll-box.

The example of annotations is:

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

The ll-box supports two methods to build rootfs: using native read-only bind mounts or with overlayfs.

`container_root_path` is the working directory of ll-box.
