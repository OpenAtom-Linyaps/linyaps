% ll-pica-adep 1

## NAME

ll\-pica\-adep - Add package dependencies to linglong.yaml file

## SYNOPSIS

**ll-pica adep** [*flags*]

## DESCRIPTION

The ll-pica adep command is used to add package dependencies to the linglong.yaml file. Linyaps applications may lack package dependencies, and this command can be used to add corresponding package dependencies to the linglong.yaml file.

## OPTIONS

**-d, --deps** _string_
: Dependencies to be added, separated by ','

**-p, --path** _string_
: Path to linglong.yaml (defaults to "linglong.yaml")

**-V, --verbose**
: Verbose output

## EXAMPLES

Add multiple dependencies:

```bash
ll-pica adep -d "dep1,dep2" -p /path/to/linglong.yaml
```

Execute in the path where linglong.yaml is located (no need to add -p parameter):

```bash
ll-pica adep -d "dep1,dep2"
```

## SEE ALSO

**[ll-pica(1)](ll-pica.md)**

## HISTORY

Developed in 2024 by UnionTech Software Technology Co., Ltd.
