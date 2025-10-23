# Linyaps Basic Concepts Introduction

## Base

Base can be understood as a lightweight "minimal system image" that contains core operating system components (such as glibc, bash, and other basic toolchains), providing a consistent underlying dependency environment for applications across different distributions. It ensures that applications running on different Linux distributions (such as Debian, Ubuntu, etc.) do not need to rely on the host system's libraries, avoiding compatibility issues caused by environmental differences.

For detailed information, see: [Base Component](./runtime.md).

## Runtime

Runtime is the environment that applications depend on during execution, containing specific framework libraries required for application operation. For example, some applications may depend on specific graphical interface frameworks (such as DTK), browser engines (such as QT WebEngine), etc. Runtime provides the corresponding runtime environment support for these applications, ensuring they can start and run properly. It works together with the Base environment to ensure applications can run stably and efficiently across different Linux distributions. Runtime and Base use a hierarchical dependency relationship - applications must first select an appropriate Base, then choose a compatible Runtime. This design ensures cross-distribution compatibility for applications.

For detailed information, see: [Runtime Component](./runtime.md).

## Sandbox

When using Linyaps, each application is built and runs in an isolated environment called a "sandbox". Each sandbox contains Base, Runtime, and the application itself.
Out of necessity, some resources within the sandbox need to be exported to the host system for use. These exported resources include the application's desktop files and icons.

## Repository

Linyaps applications and runtimes are typically stored and distributed using repositories, which behave very similarly to Git repositories: a Linyaps repository can contain multiple objects, each of which is versioned, allowing for upgrades or even downgrades.
Each system using Linyaps can be configured to access any number of remote repositories. Once a system is configured to access a 'remote', it can inspect, search the contents of the remote repository, and use it as a source for applications and runtimes.
When performing updates, new versions of applications and runtimes are downloaded from the relevant remote repositories. Similar to Git, only the parts that have changed between versions are downloaded, making the process very efficient.
