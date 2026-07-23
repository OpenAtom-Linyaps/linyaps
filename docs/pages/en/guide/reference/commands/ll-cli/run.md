% ll-cli-run 1

## NAME

ll\-cli\-run - Run applications

## SYNOPSIS

**ll-cli run** [*options*] _app_ [*command*...]

## DESCRIPTION

The `ll-cli run` command can start a Linyaps application. This command supports running applications by name, or executing commands in the container instead of running the application.

Advanced users can use the [`ll-cli` runtime configuration](../../../extra/runtime_config.md) to persist environment variables, mounts, devices, and instances globally or for one application.

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

**--workdir** _PATH_
: Specify the application's working directory

**--extensions** _REF_...
: Specify extensions used for application execution (multiple extensions separated by commas)

**--enable-xdp**, **--disable-xdp**
: Enable or disable xdg-desktop-portal integration in the sandbox; if both are specified, the last option takes effect

**--enable-pipewire**
: Mount the PipeWire socket in the sandbox

**--cdi-spec-dir** _DIR_...
: Specify CDI specification directories, defaulting to `/etc/cdi,/var/run/cdi`; separate multiple directories with commas

**--device** _DEVICE_...
: Add CDI devices, separated by commas

**--device-mode** _MODE_...
: Specify device modes; `passthru` is currently supported. Separate multiple modes with commas

**--instance** _NAME_
: Specify a container instance name to identify or reuse an instance

**--debug**
: Start the application with gdbserver

**--debug-listen** _ADDR_ [*:2345*]
: Specify the gdbserver listen address; must be used with `--debug`

**--debug-debuginfod** _URLS_
: Specify debuginfod URLs for debugging; must be used with `--debug`

**--debug-symbol-dir** _DIR_
: Specify the directory from which GDB loads debug symbols; must be used with `--debug`

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

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-ps(1)](./ps.md)**, **[ll-cli-enter(1)](./enter.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
