% ll-builder-run 1

## NAME

ll-builder-run - Run the built application

## SYNOPSIS

**ll-builder run** [*options*] [*command*...]

## DESCRIPTION

The `ll-builder run` command reads the runtime environment information related to the program based on the configuration file, constructs a container environment, and executes the program in the container without installation.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

**-f, --file** _file_ [./linglong.yaml]
: Path to the linglong.yaml file

**--modules** _modules_...
: Run specified modules. For example: --modules binary,develop

**--debug**
: Run in debug mode (enable development modules)

**--extensions** _extensions_
: Specify extensions used by the application at runtime

**COMMAND** _TEXT_...
: Enter container to execute commands instead of running application

## EXAMPLES

### Basic Usage

Run the built application, enter the project directory:

```bash
ll-builder run
```

### Debug Mode

For debugging purposes, you can enter the container to execute shell commands instead of running the application:

```bash
ll-builder run /bin/bash
```

Using this command, `ll-builder` will create a container and then enter the `bash` terminal, allowing you to execute other operations within the container.

### Run Specific Modules

Run specified modules instead of the default binary module:

```bash
ll-builder run --modules binary,develop
```

### Run Application with Extensions

Run the application using specified extensions:

```bash
ll-builder run --extensions org.deepin.extension1,org.deepin.extension2
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
