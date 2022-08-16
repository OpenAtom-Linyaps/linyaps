# Application run conf

Application run conf is the file how linglong runtime module load the app,

and the conf is machine related, the user can set mount path, permisson control file to the app.

## File Format

Should be a yaml file.

## Storage

The config is highly user related, it should store in ~/.linglong/{appid}/app.yaml

If not exist, create it first from info.json.

## Content

```yaml
# should be 1.0 now
version: 1.0

package:
  ref: com.qq.weixin.deepin/3.2.1.154deepin8/x86_64

runtime:
  ref: org.deepin.Runtime/20/x86_64

# the section only can set by develop, and only work with debug enable
debug:
  # override entrypoint
  args:
    - /opt/apps/com.qq.weixin.work.deepin/files/run.sh
    - --debug
  # override the base, casefully set the debug args
  mount-bottom:
    - /usr:/usr

permissions:
  # mount after all runtime container build
  mounts:
    - /etc/passwd:/etc/passwd
    - /etc/group:/etc/group
    - ${HOME}/Documents:${HOME}/Documents
    - ${HOME}/Desktop:${HOME}/Desktop
    - ${HOME}/Downloads:${HOME}/Downloads
  # todo: session and system bus
  dbus:
    owner:
      - com.qq.weixin.work.deepin
    talk:
      - org.freedesktop.Notifications
```
