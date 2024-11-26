## Add dependency

linyaps applications may lack package dependencies, which can currently
 be addressed by adding the corresponding package dependencies in the `linglong.yaml` file.

The `ll-pica adep` command is used to add package dependencies to the `linglong.yaml` file.

View the help information for the `ll-cli adep` command:

```bash
ll-pica adep --help
```

Here is the output:

```bash
Add dependency packages to linglong.yaml

Usage:
  ll-pica adep [flags]

Flags:
  -d, --deps string   dependencies to be added, separator is ','
  -h, --help          help for adep
  -p, --path string   path to linglong.yaml (default "linglong.yaml")

Global Flags:
  -V, --verbose   verbose output
```

```bash
ll-pica adep -d "dep1,dep2" -p /path/to/linglong.yaml
```

If executing within the same path where the `linglong.yaml` file resides, there is no need to include the `-p` parameter.
