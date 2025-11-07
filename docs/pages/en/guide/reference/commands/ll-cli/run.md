% ll-cli-run 1

## NAME

ll\-cli\-run - Run applications

## SYNOPSIS

**ll-cli run** [*options*] _app_ [*command*...]

## DESCRIPTION

The `ll-cli run` command can start a Linyaps application. This command supports running applications by name, or executing commands in the container instead of running the application.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help information

**--file** _FILES:FILE_...
: Pass files to the application running in the sandbox

**--url** _URLS_...
: Pass URLs to the application running in the sandbox

**--env** _ENV_...
: Set environment variables for the application (format: KEY=VALUE)

**--base** _REF_
: Specify the base environment used for application execution

**--runtime** _REF_
: Specify the runtime used for application execution

**--extensions** _REF_...
: Specify extensions used for application execution (multiple extensions separated by commas)

## POSITIONAL ARGUMENTS

**APP** _TEXT_ _REQUIRED_
: Specify the application name

**COMMAND** _TEXT_...
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

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-ps(1)](./ps.md)**, **[ll-cli-exec(1)](./enter.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
