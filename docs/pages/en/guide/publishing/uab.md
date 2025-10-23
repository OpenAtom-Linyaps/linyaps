# UAB Application Publishing

This document supplements the relevant content for application publishing using UAB (Universal Application Bundle).

## What is UAB

UAB (Universal Application Bundle) is a cross-distribution application packaging format designed to solve the compatibility issues of Linux applications across different distributions. It packages applications and all their dependencies into a single file, enabling one-click installation and operation.

## UAB Publishing Advantages

- **Cross-distribution compatibility**: Applications packaged in UAB format can run on different Linux distributions
- **One-click installation**: Users only need to download one file to complete the installation
- **Dependency self-contained**: All dependencies are included in the package, avoiding dependency conflicts
- **Easy to distribute**: A single file is easy to transmit and share

## Creating UAB Packages

### Prerequisites

Before creating a UAB package, you need to:

1. Complete the development and testing of the application
2. Ensure the application can run normally in the Linyaps environment
3. Prepare the application's metadata information (name, version, description, etc.)

### Build Process

1. Use `ll-builder` to build the application:

```bash
ll-builder build
```

2. Export as UAB format:

```bash
ll-builder export --type=uab
```

3. The generated UAB file will be saved in the current directory, with the file name format: `{app-id}_{version}_{arch}.uab`

## UAB File Structure

The UAB file is essentially a compressed package containing:

- Application executable files and resource files
- All dependent libraries and runtime environments
- Application metadata and configuration files
- Installation scripts and desktop entry files

## Publishing UAB Applications

### Method 1: Direct Distribution

You can directly distribute the generated UAB file to users through the following methods:

- Official website download
- GitHub Releases
- Third-party software download sites
- USB flash drive or other storage media

### Method 2: Repository Publishing

Upload the UAB file to the Linyaps repository:

```bash
ll-builder push --repo=your-repo
```

### Method 3: App Store Publishing

Submit the application to the app store through the official channel, and the app store will automatically handle the UAB package and provide it to users for download.

## User Installation

Users can install UAB applications in the following ways:

### Command Line Installation

```bash
ll-cli install ./your-app.uab
```

### Graphical Interface Installation

Double-click the UAB file, and the system will automatically call the Linyaps installer to complete the installation.

## UAB Application Management

### View Installed Applications

```bash
ll-cli list
```

### Update Applications

```bash
ll-cli update your-app-id
```

### Uninstall Applications

```bash
ll-cli uninstall your-app-id
```

## Best Practices

1. **Version Management**: Use semantic versioning (Semantic Versioning) to manage application versions
2. **Testing**: Test on multiple distributions to ensure compatibility
3. **Documentation**: Provide detailed installation and usage instructions
4. **Updates**: Regularly update applications and dependencies
5. **Security**: Ensure that applications and dependencies do not contain security vulnerabilities

## Common Issues

### Installation Failure

- Check if the UAB file is complete
- Confirm that the system has Linyaps installed
- View error logs to locate the problem

### Runtime Errors

- Check if dependencies are complete
- Confirm system compatibility
- View application logs

### Update Issues

- Check network connection
- Confirm repository configuration
- View update logs

## Technical Support

If you encounter problems during the UAB application publishing process, you can:

1. View the official documentation: [Linyaps Documentation](https://linglong.dev)
2. Submit an Issue: [GitHub Issues](https://github.com/OpenAtom-Linyaps/linyaps/issues)
3. Community Support: Join the Linyaps community for discussion
