% ll-cli-kill 1

## NAME

ll\-cli\-kill - Stop running applications

## SYNOPSIS

**ll-cli kill** [*options*] _app_

## DESCRIPTION

The `ll-cli kill` command can forcefully exit running Linyaps applications. This command is used to terminate running application processes.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

**-s, --signal** _SIGNAL_ [*SIGTERM*]
: Specify the signal to send to the application

## POSITIONAL ARGUMENTS

**APP** _TEXT_ _REQUIRED_
: Specify the name of the running application

## EXAMPLES

Use the `ll-cli kill` command to forcefully exit running Linyaps applications:

```bash
ll-cli kill org.deepin.calculator
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-ps(1)](ps.md)**, **[ll-cli-run(1)](run.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
