# linglong user guide

1. 添加v23内网源地址

    ```plain
    deb [by-hash=force trusted=yes] http://ci.uniontech.com/sniperepo/snipe/snipe-develop/ develop main
    ```

2. 安装玲珑环境

    ```bash
    sudo apt install linglong-builder linglong-box linglong-dbus-proxy linglong-bin
    ```

3. 建议在v23镜像中使用玲珑工具：

    ```plain
    linglong-builder : ll-builder ll-loader
    linglong-box : ll-box
    linglong-dbus-proxy : ll-dbus-proxy
    linglong-bin : ll-cli ll-service
    ```
