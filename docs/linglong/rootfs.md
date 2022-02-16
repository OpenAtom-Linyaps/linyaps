# Rootfs

As oci runtime specification, the runtime start from an rootfs, but in linglong, it prepare rootfs in ll-box. so we put the information of construction rootfs in annotations to ll-box.

The example of annotations is：

```json
  "annotations": {
    "container_root_path": "/run/user/1000/linglong/375f5681145f4f4f9ffeb3a67aebd444",
    "native": {
      "mounts": [
        {
          "destination": "/usr",
          "options": [
            "ro",
            "rbind"
          ],
          "source": "/usr",
          "type": "bind"
        },
        {
          "destination": "/runtime",
          "options": [
            "ro",
            "rbind"
          ],
          "source": "/deepin/linglong/layers/org.deepin.Runtime/20/x86_64",
          "type": "bind"
        },
        {
          "destination": "/opt/apps/org.deepin.music",
          "options": [
            "ro",
            "rbind"
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
            "ro",
            "rbind"
          ],
          "source": "/usr",
          "type": "bind"
        },
        {
          "destination": "/usr",
          "options": [
            "ro",
            "rbind"
          ],
          "source": "/usr",
          "type": "bind"
        },
        {
          "destination": "/opt",
          "options": [
            "ro",
            "rbind"
          ],
          "source": "/opt",
          "type": "bind"
        },
        {
          "destination": "/opt/apps/com.qq.weixin.deepin",
          "options": [
            "ro",
            "rbind"
          ],
          "source": "/opt/apps/com.qq.weixin.work.deepin/",
          "type": "bind"
        }
      ]
    }
  }
```

The ll-box support two method to build rootfs, use native ro bind mount or with overlayfs. 

`container_root_path` is the work directory of ll-box.
