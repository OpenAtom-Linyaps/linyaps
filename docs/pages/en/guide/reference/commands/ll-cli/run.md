% ll-cli-run 1

## NAME

ll\-cli\-run - Run applications

## SYNOPSIS

**ll-cli run** [*options*] *app* [*command*...]

## DESCRIPTION

The `ll-cli run` command can start a Linyaps application. This command supports running applications by name, or executing commands in the container instead of running the application.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help information

**--file** *FILES:FILE*...
: Pass files to the application running in the sandbox

**--url** *URLS*...
: Pass URLs to the application running in the sandbox

**--env** *ENV*...
: Set environment variables for the application (format: KEY=VALUE)

**--base** *REF*
: Specify the base environment used for application execution

**--runtime** *REF*
: Specify the runtime used for application execution

**--extensions** *REF*...
: Specify extensions used for application execution (multiple extensions separated by commas)

## POSITIONAL ARGUMENTS

**APP** *TEXT* *REQUIRED*
: Specify the application name

**COMMAND** *TEXT*...
: Run commands in the running sandbox

## EXAMPLES

Run application by name:

```bash
ll-cli run org.deepin.demo
```

Execute commands in the container instead of running the application:

```bash
ll-cli run org.deepin.demo bash
ll-cli run org.deepin.demo -- bash
ll-cli run org.deepin.demo -- bash -x /path/to/bash/script
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-ps(1)](./ps.md)**, **[ll-cli-exec(1)](./exec.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
