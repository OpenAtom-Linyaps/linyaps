# uab Build

Creating a runnable uab file from a Linyaps-oriented project requires the following configuration files:

1. **linglong.yaml**: The Linyaps project configuration file that determines how the package will be compiled and installed;
2. **loader**: An executable file that serves as the entry point for the uab.
3. **extra-files.txt**: A text file that specifies which files the application depends on from base and runtime. These files will be copied to the uab package and placed in their corresponding locations during application runtime. If the application can run without depending on other files from runtime and base, this file can be omitted.

If you only want to export the project as a uab for submitting applications to the Linyaps store and don't need it to be runnable, you don't need to provide the loader and extra-files.txt.

Example loader code:

```bash
#!/bin/env sh
APPID=org.deepin.demo
./ll-box $APPID $PWD /opt/apps/$APPID/files/bin/demo
```

The last parameter passed to ll-box is the binary that needs to run in the container.
