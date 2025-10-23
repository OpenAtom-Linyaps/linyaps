# Adapting Qt5-Based Open Source Application -- qBittorrent to Support Automated Linyaps Application Build Projects

Continuing from the previous chapter, we will explore how to adapt the `linglong.yaml` to support automated build versions.
Since we have already determined the build rules that can successfully complete the compilation and build tasks in the previous section, this section mainly discusses how to use the `git` function to complete the following automated operations:

1. Automatically pull open source project git repository resources
2. Automatically apply custom patch content

## Modifying `linglong.yaml`

In the previous section, we obtained a `linglong.yaml` that can successfully compile local source code. Since it's the same application project, we can directly modify it based on the existing configuration.

For `linglong.yaml`, the two core parts are mainly the `sources` and `build` modules. This time we will explain these two parts separately.

### `sources` Module

According to the [Linyaps Application Build Project `linglong.yaml` Specification], when the build project's source files are git repositories (i.e., pulling source code repositories through git clone), the source file type `kind` of `sources` should be set to `git` and fill in the relevant information of the repository.

According to the [Linyaps Application Build Project `linglong.yaml` Specification] `Git Pull Source Code Compilation Mode` specification, we need to master the following information about the source code repository:

1. url, which is the address used for git clone, can use mirror addresses like ordinary git clone
2. version, which is the specific version number of the repository that needs to be fetched, generally the `Tags` label
3. The repository commit number that needs to be applied. Fill in the value corresponding to the commit here, which will apply all changes up to this commit in the repository. This field has higher priority than `version`. Please do not fill in any `commit` after the merge time of `version`

Combined with the compilation notes in the second section, we collected information about the `qBittorrent.git` and `libtorrent.git` repositories. To reduce build progress blocking caused by network issues, I chose to use mirror addresses here:

```yaml
sources:
  - kind: git
    url: https://githubfast.com/qbittorrent/qBittorrent.git
    version: release-4.6.7
    commit: 839bc696d066aca34ebd994ee1673c4b2d5afd7b

  - kind: git
    url: https://githubfast.com/arvidn/libtorrent.git
    version: v2.0.9
    commit: 4b4003d0fdc09a257a0841ad965b22533ed87a0d
```

![commit-1](images/4-commit-1.png)

![commit-2](images/4-commit-2.png)

As can be seen from the figures, the filled commit values all match the commits when the corresponding tags were generated.

### `build` Module

After completing the source file information settings, we start modifying the build rules.
Considering that we have obtained executable build rules in the previous lesson, we only need to modify the source code path according to the relevant specifications in the [Linyaps Application Build Project `linglong.yaml` Specification] `Git Pull Source Code Compilation Mode`.

After completing the modifications, we will get the following build rules:

```yaml
build: |
  mkdir -p ${PREFIX}/bin/ ${PREFIX}/share/
  ##Build 3rd libs
  mkdir /project/linglong/sources/libtorrent.git/build
  cd /project/linglong/sources/libtorrent.git/build
  cmake -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$PREFIX ..
  make -j$(nproc)
  make install

  ##Build main
  mkdir /project/linglong/sources/qBittorrent.git/build
  cd /project/linglong/sources/qBittorrent.git/build
  cmake -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$PREFIX ..
  make -j$(nproc)
  make install

  ##Extract common res
  cp -rf /project/template_app/* ${PREFIX}/share/
```

### Custom Patch Application

In this case, we also introduce a new scenario: after pulling open source project repository resources of a specific version number, we need to modify the source code to achieve patch effects for fixing vulnerabilities and bugs.

For this, we need to learn new knowledge: `git patch` related content. The git program has a built-in patch management system that needs to conveniently use this function to revise the source code. Generally, there are the following steps:

1. Pull the open source project repository resources to the local directory through `git clone`
2. Manually modify based on the pulled resources. At this time, you can compare the differences and changes with the initial state through the `git diff` function. This forms the content of the patch. The specific patch division can vary according to the maintainer
3. After generating the patch document, pull the same open source project repository again. At this time, apply this patch through `git apply` to achieve automatic revision based on change records, and also facilitate recording source code maintenance records

For example, after pulling the `qBittorrent--4.6.7` repository in this case, I need to add a merge related to security vulnerabilities:
https://github.com/qbittorrent/qBittorrent/pull/21364
![commit-3](images/4-commit-3.png)

It should be noted that this submission is based on the `qBittorrent--5.x` version, which means that if I want to fix this vulnerability on `qBittorrent--4.6.7`, I need to manually modify the source code.
Considering that the main version number of this application maintained in the short term will not be iterated to `5.x`, and with the possibility of other security vulnerabilities occurring and reducing the labor cost of repeatedly modifying the source code, I adopted the method of adding patches to assist me in modifying the source code.

The following shows my process of obtaining & applying patches:

1. First, I pulled the `qBittorrent--4.6.7` repository in `sources` to the local directory through `git clone`
2. Enter the pulled directory and check whether the current directory is a valid git directory through the git program:

```zsh
❯ git remote -v
origin  https://ghp.ci/https://github.com/qbittorrent/qBittorrent.git (fetch)
origin  https://ghp.ci/https://github.com/qbittorrent/qBittorrent.git (push)
```

As can be seen, after executing the command, information about the remote repository can be returned, indicating that this directory meets the requirements.

3. Then, manually modify the source code according to the file changes when the above security vulnerability was submitted. \* Here, the maintainer needs to judge which specific submissions need to be merged based on experience

4. After the modification is completed, return to the root directory of the source code and execute `git diff` to view the existing difference comparison:

5. After confirmation, save this difference locally to form a patch file

```bash
ziggy@linyaps23:/media/szbt/Data/ll-build/QT/qBittorrent-local$ git diff > ./
```

6. After obtaining the patch file, we will add the content of the applied patch to the existing `build` build rules:

```yaml
build: |
  mkdir -p ${PREFIX}/bin/ ${PREFIX}/share/
  ##Apply patch for qBittorrent
  cd /project/linglong/sources/qBittorrent.git
  git apply -v /project/patches/linyaps-qBittorrent-4.6.7-szbt2.patch

  ##Build 3rd libs
  mkdir /project/linglong/sources/libtorrent.git/build
  cd /project/linglong/sources/libtorrent.git/build
  cmake -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$PREFIX ..
  make -j$(nproc)
  make install

  ##Build main
  mkdir /project/linglong/sources/qBittorrent.git/build
  cd /project/linglong/sources/qBittorrent.git/build
  cmake -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$PREFIX ..
  make -j$(nproc)
  make install

  ##Extract common res
  cp -rf /project/template_app/* ${PREFIX}/share/
```

7. At this point, we can return to the build directory and start the build test. Execute:

```bash
ziggy@linyaps23:/media/szbt/Data/ll-build/QT/qBittorrent-local$ ll-builder build -v
```

This build ended quickly and successfully. We execute the following command to export the container as a Linyaps application installation package `binary.layer`:

```bash
ziggy@linyaps23:/media/szbt/Data/ll-build/QT/qBittorrent-local$ ll-builder export --layer
```

## Local Build Result Testing

After obtaining the Linyaps application installation package, I tried to experience it on different mainstream distributions that support the Linyaps environment to confirm whether the binary programs built through Linyaps containers have universality.

### deepin 23

![deepin 23](images/4-test-1.png)

### openKylin 2.0

![openKylin 2.0](images/4-test-2.png)

### Ubuntu 2404

![Ubuntu 2404](images/4-test-3.png)

### OpenEuler 2403

![OpenEuler 2403](images/4-test-4.png)

So far, it is sufficient to prove that `Qt5-based open source application--qBittorrent` can achieve one-stop pulling of project source code and compilation into executable binary files after adding custom patches and modifying build rules, and can also be used on other distributions!
