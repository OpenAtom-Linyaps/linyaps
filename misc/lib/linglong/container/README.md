# OCI configurations for linglong containers

Directory [config.d] contains OCI configuration patches and generators.
They will be applied in file name order.

## generator

Files in [config.d] that is executable for linglong runtime program
are treated as OCI configuration generators for linglong.

They will be executed by linglong runtime program
with the constructing OCI configuration writed into their stdin.
They should print **FULL** content of
that modified OCI configuration to their **stdout**,
and print error message or warning to their **stderr**.

If anything goes wrong, for example:

- the OCI configuration read from stdout failed to be parsed;
- generator exit with non-zero return code;
- generator crashed;
- ...

That generator will be ignored.

## OCI configuration patches

Files in [config.d] that is **NOT** executable for linglong runtime program
and ends with `.json` are treated as OCI configuration patches.

They will be read by linglong runtime program,
then the items in `patch` field will be parsed as [JSON Patch]
then be applied one by one.

[JSON Patch]: https://jsonpatch.com/

If anything goes wrong, for example:

- the json file failed to be parsed as OCI configuration patch;
- the `ociVersion` field is not equals to constructing configuration;
- ...

That patch will be ignored.

The json schema definition of OCI configuration patch file
can be found at [/api/schema/v1.yaml].

[/api/schema/v1.yaml]: ../../../../api/schema/v1.yaml


## Application-specific patches

Patches whose filenames match the application ID are treated as application-specific.
These patches are applied after global patches.

## Examples

- Global patch:   `99-dump-conf`
- App patch:     `com.example.app.json` (matches app ID)

com.example.app.json:

```json
{
  "ociVersion": "1.0.1",
  "patch": [
    {
      "op": "add",
      "path": "/mounts/-",
      "value": {
        "destination": "/opt/host-apps/",
        "type": "bind",
        "source": "/opt/apps",
        "options": [
          "rbind"
        ]
      }
    }
  ]
}
```

com.example.app.json add an extra mounts, which bind host's `/opt/apps` to container's `/opt/host-apps`,
this patch will applied after 99-dump-conf.

99-dump-conf can write following content to print the container's configuration:

``` bash
#!/bin/sh

content=$(cat -)
echo $content >&2
echo ${content}
```
