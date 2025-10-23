# Repository

The official Linyaps repository is the primary mechanism for publishing applications so that users can install applications more conveniently. Currently, after installing the Linyaps components, this repository is used by default.

- Some basic concepts of repositories have been introduced in [Repository Basic Concepts](../reference/basic-concepts.md);
- Adding, updating, and deleting repositories can be done using the `ll-cli repo` command. For specific usage, please refer to [ll-cli-repo(1)](../reference/commands/ll-cli/repo.md);
- Using self-hosted repositories: To be supplemented;

## Publishing Updates

Linyaps repositories are similar to Git repositories in that they store each version of an application by recording the differences between each version. This makes updates efficient because when performing an update, only the differences (or "deltas") between the two versions need to be downloaded.

When a new version of an application is added to the repository, it immediately becomes available to users. The app store can automatically check for and install new versions. Using the command line requires manually running `ll-cli list --upgradable` to check and use `ll-cli update` to update installed applications to the new version.
