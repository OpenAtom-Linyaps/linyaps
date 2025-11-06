# Reference (Ref)

## Definition

Component: A component is generally equivalent to artifacts/component/module/package in other systems. In Linyaps, we use "layer" to represent a component.

## Format

Ref is the unique identifier of a component in the storage repository (index information in the storage repository). A complete ref includes repository name, channel, ID, version number, architecture, branch name and other information.

The ref format is as follows:

```bash
${repo}/${channel}:${id}/${version}/${arch}
```

Examples:

```bash
deepin/main:org.deepin.runtime/20/x86_64

deepin/project1:org.deepin.calculator/1.2.2/x86_64
```

Where:

- `deepin` is the name of the remote repository to which the software package belongs
- `main` and `project1` are distribution channels, which can be empty by default
- `org.deepin.calculator` is the ID
- `1.2.2` is the version
- `x86_64` is the architecture

### Value Range

| Field   | Allowed Values                                                                                                  |
| ------- | --------------------------------------------------------------------------------------------------------------- |
| repo    | Lowercase letters + underscore                                                                                  |
| channel | Lowercase letters + underscore                                                                                  |
| id      | Reverse domain name                                                                                             |
| version | 4 digits separated by `.`. Pad with 0 if less than 4 digits, append subsequent numeric strings to the 4th digit |
| arch    | Architecture description string, currently supports x86_64/arm/loongarch                                        |

## Usage

Ref contains two parts: remote address identifier and local identifier. The `:` separates the remote identifier from the local identifier.

The remote identifier is mainly used to define how this layer is obtained. The channel is not currently used and can be empty. The repo represents an alias for the mapping of the remote repository (URL) locally.

During installation, using a complete ref can uniquely identify a layer. For example:

```bash
ll-cli install deepin/main:org.deepin.calculator/1.2.2/x86_64
```

Once a layer is installed, all local operations on the layer will not change its remote properties, especially during upgrades.

```bash
# Upgrade org.deepin.calculator from deepin/main, its version will change, but repo and channel will not
# Note: install will update the package here
ll-cli install org.deepin.calculator/1.2.2/x86_64

# This will use the layer from deepin/project channel to replace the local org.deepin.calculator/1.2.2/x86_64
ll-cli install deepin/project:org.deepin.calculator/1.2.2/x86_64
```

Notes:

- When switching repo or channel, local versions need to be deleted, especially when there are multiple versions under an ID (the underlying layer can be considered for reuse, but logically ensure that layer versions are under the corresponding channel).

### Shorthand

Ref supports shorthand with the following main rules:

1. Default repo is `deepin`
2. Default channel is `main` (currently not implemented, temporarily not considered)
3. Default version is the remote latest version or local latest version, inferred according to the scenario, error if ambiguous
4. Default architecture is the current ll-cli runtime architecture

If any field in ref is explicitly specified, it takes precedence based on that field.

## Implementation

Currently, the layers storage path is:

```bash
/deepin/linglong/layers
```

This path will definitely be replaced later, so please avoid hardcoding this field in too many places.
