# ll-builder run

```bash
Usage: ll-builder [options] build

Options:
  -v, --verbose  show detail log
  -h, --help     Displays help on commandline options.
  --help-all     Displays help including Qt specific options.
  --exec <exec>  run exec than build script

Arguments:
  run            run project
```

Run build result after ll-builder build without install.

TODO: Now the builder can not found the right exec path, and some environment is bot work, you can create and loader script and use exec to test, see `linglong.yaml` Examples for more. 