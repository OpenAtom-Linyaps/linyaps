# 常见问题

1. ll-pica 生成的 linglong.yaml 文件的默认配置在哪里？

   ll-pica 配置文件在 `~/.pica/config.json`。
2. ll-pica 无法转换，wine、安卓、输入法、安全类软件吗？

   玲珑应用目前不支持这类应用，ll-pica 也无法转换。
3. 为什么需要使用音频的软件没有声音？

   提示 not found libpulsecommon-12.2.so 可以在linglong.yaml 文件build 字段的里面，添加一行 `mv $PREFIX/lib/$TRIPLET/pulseaudio/* $PREFIX/lib/$TRIPLET`。
4. 为什么生成出来的 linglong.yaml 文件中 command 字段是空的？

   ll-pica 是通过读取，deb 包中的 desktop 文件里来获取 Exec 字段，如果 command 为空，请检查 deb 包中的 desktop 文件路径是否在以下路径。

   - /opt/apps/$appid/entries/applications
   - /usr/share/applications
