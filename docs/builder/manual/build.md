# ll-builder build

```bash
Usage: ll-builder [options] build

Options:
  -v, --verbose  show detail log
  -h, --help     Displays help on commandline options.
  --help-all     Displays help including Qt specific options.
  --exec <exec>  run exec than build script

Arguments:
  build          build project
```

Build the project, this must run at the top of project, and with an linglong.yaml build file.

## Introspection coustom shell

use `--exec={path}` could run an command before build.

This should only for debug the build script, NEVER use it in production environment.

TODO: pre-build-exec/post-build-exec is better than only exec before build.