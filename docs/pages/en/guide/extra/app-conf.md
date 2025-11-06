# Application Runtime Configuration

Application runtime configuration is the file that describes how the Linyaps runtime module loads the application. The configuration is machine-specific, and the user can set mount paths and permission control files for the application.

## File Format

Must be a YAML file.

## Storage Location

The configuration is highly user-specific and should be stored at `~/.linglong/{appid}/app.yaml`

If it doesn't exist, it should be created first from `info.json`.

## Content

```yaml
# Should be 1.0 now
version: 1.0

package:
  ref: com.qq.weixin.deepin/3.2.1.154deepin8/x86_64

runtime:
  ref: org.deepin.Runtime/20/x86_64

# The section can only be set by developers and only works when debug is enabled
debug:
  # Override entrypoint
  args:
    - /opt/apps/com.qq.weixin.work.deepin/files/run.sh
    - --debug
  # Override the base, carefully set the debug args
  mount-bottom:
    - /usr:/usr

permissions:
  # Mount after all runtime container builds
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
