# OCI configurations for linglong containers

*NOTO*: [config.json] and generators has been builtin to program itself. all generator could be override by user.

[config.json] is the initial OCI configuration file
for linglong desktop containers.

Directory [config.d] contains OCI configuration patches and generators.
They will be applied in file name order.

[config.json]: ./config.json

[config.d]: ./config.d

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
