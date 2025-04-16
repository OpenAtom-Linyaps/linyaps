# Linglong Application Versioning Standard

Linglong applications have fully adopted the [SemVer 2.0.0 (Semantic Versioning)](https://semver.org/) standard, with extensions for security updates to enhance the standardization and security of version management.

## Versioning Specification

The new version number format is as follows:

```
MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD_METADATA[.SECURITY_UPDATE]]
```

- **MAJOR version**: Increased when incompatible API changes are made.
- **MINOR version**: Increased when functionality is added in a backward-compatible manner.
- **PATCH version**: Increased when backward-compatible bug fixes are made.
- **Pre-release label** (optional): For example, `-alpha`, `-beta.1`, or `-rc.2`, indicating development or testing stages.
- **Build metadata** (optional): For example, `+20250416`, used to tag build information.
- **Security update** (optional): Appended directly after build metadata, used to indicate security patch numbers or security-related updates (e.g., `+security.1`).

### Typical Examples

- Stable release: `1.0.0`

- Beta release: `2.1.3-beta.2+20250416`

- Security update release: `2.1.3+20250416.security.1`

## Additional Notes

1. The `security` extension field is used exclusively for security update scenarios; regular releases do not require this marker.

2. With the adoption of the SemVer standard, version management becomes more transparent and standardized. Users can:
    - Clearly recognize API compatibility changes
    - Accurately identify security update versions
    - Easily distinguish between stable and testing releases

3. Developer guidelines:
    - Changing the MAJOR version indicates incompatible updates
    - Changing the MINOR version indicates new, backward-compatible features
    - Changing the PATCH version indicates bug fixes
