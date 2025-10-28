# Application Run Configuration

Application run configuration is the file that the Linglong runtime module uses to load the app.

The configuration is machine-related, and the user can set mount paths and permission control files for the app.

## File Format

Should be a yaml file.

## Storage

The config is highly user related, it should be stored in ~/.linglong/{appid}/app.yaml

If not exist, create it first from info.json.

## Content

```yaml
# should be 1.0 now
version: 1.0

package:
  ref: com.qq.weixin.deepin/3.2.1.154deepin8/x86_64

runtime:
  ref: org.deepin.Runtime/20/x86_64

  # This section can only be set by developers and only works with debug enabled
  debug:
    # Override entrypoint
    args:
      - /opt/apps/com.qq.weixin.work.deepin/files/run.sh
      - --debug
    # Override the base, carefully set the debug args
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
  # TODO: session and system bus
  dbus:
    owner:
      - com.qq.weixin.work.deepin
    talk:
      - org.freedesktop.Notifications
```
