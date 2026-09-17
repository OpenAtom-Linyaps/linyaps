%global debug_package %{nil}
Name:           linglong
Version:        1.13.8
Release:        2
Summary:        Linglong Package FrameWork
License:        LGPLv3
URL:            https://github.com/linuxdeepin/%{name}
Source0:        %{url}/archive/%{version}/linglong-%{version}.tar

BuildRequires:  cmake gcc-c++
BuildRequires:  qt5-qtbase-devel qt5-qtbase-private-devel
BuildRequires:  glib2-devel nlohmann-json-devel ostree-devel yaml-cpp-devel
BuildRequires:  systemd-devel systemd-rpm-macros gtest-devel elfutils-libelf-devel
BuildRequires:  glibc-static libstdc++-static
BuildRequires:  libcurl-devel openssl-devel
BuildRequires:  gtest-devel gmock-devel erofs-utils
BuildRequires:  libcap-devel gettext-devel
BuildRequires:  libuuid-devel
Requires:       linglong-bin = %{version}-%{release}

%description
This package is a linglong package framework.

%package        -n linglong-bin
Summary:        Linglong package manager
Requires:       linglong-box
Requires:       polkit erofs-utils
Recommends:     erofsfuse
%{?systemd_requires}
%description    -n linglong-bin
Linglong package management command line tool.

%package        -n linglong-builder
Summary:        Linglong build tools
Requires:       linglong-box linglong-bin = %{version}-%{release}
Requires:       erofs-utils fuse-overlayfs shadow-utils
Recommends:     git
%description    -n linglong-builder
This package is a tool that makes it easy to build applications and dependencies.

%prep
%autosetup -p1 -n linglong-%{version}

%define _debugsource_template %{nil}

%build
export PATH=%{_qt5_bindir}:$PATH
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX:PATH=%{_prefix} \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -DINCLUDE_INSTALL_DIR:PATH=%{_includedir} \
      -DLIB_INSTALL_DIR:PATH=%{_libdir} \
      -DSYSCONF_INSTALL_DIR:PATH=%{_sysconfdir} \
      -DSHARE_INSTALL_PREFIX:PATH=%{_datadir} \
      -DBUILD_SHARED_LIBS=OFF \
      -DCPM_LOCAL_PACKAGES_ONLY=ON ..
%make_build

%install
cd build
%make_install INSTALL_ROOT=%{buildroot}

%post -n linglong-bin
%systemd_post org.deepin.linglong.PackageManager.service
systemctl start org.deepin.linglong.PackageManager.service 2>/dev/null || :

%preun -n linglong-bin
%systemd_preun org.deepin.linglong.PackageManager.service

%postun -n linglong-bin
%systemd_postun_with_restart org.deepin.linglong.PackageManager.service

%files
%doc README.md
%license LICENSE
%exclude %{_libdir}/cmake/linglong-*/*.cmake

%files -n linglong-bin
%doc README.md
%license LICENSE
%{_sysconfdir}/profile.d/*
%{_sysconfdir}/X11/Xsession.d/*
%{_bindir}/ll-cli
%{_bindir}/llpkg
%{_prefix}/lib/%{name}/container/*
%{_prefix}/lib/%{name}/generate-xdg-data-dirs.sh
%{_prefix}/lib/sysusers.d/*.conf
%{_prefix}/lib/tmpfiles.d/*.conf
%{_prefix}/lib/systemd/system/*.service
%{_prefix}/lib/systemd/system-preset/*.preset
%{_prefix}/lib/systemd/system-environment-generators/*
%{_prefix}/lib/systemd/user-generators/*
%{_libexecdir}/%{name}/ll-package-manager
%{_libexecdir}/%{name}/ld-cache-generator
%{_libexecdir}/%{name}/font-cache-generator
%{_libexecdir}/%{name}/ll-init
%{_libexecdir}/%{name}/ll-driver-detect
%{_datadir}/bash-completion/completions/ll-cli
%{_datadir}/zsh/vendor-completions/_ll-cli
%{_datadir}/dbus-1/system-services/*.service
%{_datadir}/dbus-1/system.d/*.conf
%{_datadir}/polkit-1/actions/org.deepin.linglong.PackageManager1.policy
%{_datadir}/%{name}/config.yaml
%{_datadir}/%{name}/export-dirs.json
%{_datadir}/mime/packages/*
%{_datadir}/locale/*
%{_datadir}/applications/*
%{_datadir}/icons/*
%{_datadir}/fish/vendor_completions.d/ll-cli.fish

%files -n linglong-builder
%license LICENSE
%{_bindir}/ll-builder
%{_libexecdir}/%{name}/fetch-dsc-source
%{_libexecdir}/%{name}/fetch-git-source
%{_libexecdir}/%{name}/fetch-file-source
%{_libexecdir}/%{name}/fetch-archive-source
%{_libexecdir}/%{name}/app-conf-generator
%{_libexecdir}/%{name}/builder/helper/*.sh
%{_datadir}/bash-completion/completions/ll-builder
%{_datadir}/zsh/vendor-completions/_ll-builder
%{_datadir}/fish/vendor_completions.d/ll-builder.fish
%{_datadir}/%{name}/builder/templates/*.yaml
%{_datadir}/%{name}/builder/uab/*

%changelog
* Wed Sep 17 2026 dengbo <dengbo@deepin.org> - 1.13.8-2
- fix(rpm): ensure package-manager service starts immediately after installation

* Thu Jul 16 2026 dengbo <dengbo@deepin.org> - 1.13.8-1
- fix: avoid missing task state if task is removed quickly
- feat: embed prerelease and build metadata in version string
- fix: fall back to copy when hard link fails during UAB export
- feat: add PipeWire socket mount support for sandbox
- refactor: make runtime config loading accept custom config dirs
- fix: don't mount /etc/passwd and /etc/group in build mode
- fix: ensure XDG_RUNTIME_DIR is set for namespaced child process
- fix: add rslave propagation to bind mounts
- feat: add disable_xdp option to system config
- fix: /persistent can not write
- feat: add /persistent mount
* Thu Jul 02 2026 dengbo <dengbo@deepin.org> - 1.13.4-1
- fix(cli): use HOME env instead of hardcoded "/home/" path prefix check
- fix: fix fd marshaling in QtDBus
- fix: ll-package-manager process start abnormal
* Tue Jun 30 2026 dengbo <dengbo@deepin.org> - 1.13.3-1
- feat: avoid duplicate ll-package-manager startup by checking existing connection
- fix: retry cache removal after modify directory permissions
* Thu Jun 25 2026 dengbo <dengbo@deepin.org> - 1.13.2-1
- fix: parse install hook lines with proper quote handling
* Wed Jun 24 2026 dengbo <dengbo@deepin.org> - 1.13.1-1
- fix: start package manager before repo loading in peer mode
- fix: make tests pass without /etc/localtime
* Thu Jun 18 2026 dengbo <dengbo@deepin.org> - 1.13.0-1
- feat: add RISC-V 64 architecture support
- feat: add instance-specific config
- feat: support run command in runtime or base environment
- feat: custom user/group content exposed in container
- feat: auto-detect NVIDIA GPU via CDI
- feat: support system-wide runtime configuration
- feat: add --disable-xdp flag to disable xdp integration
- feat(oci): enable xdg-desktop-portal for GTK/Qt apps by default
- feat: add device passthru mode
- feat: support CDI device
- feat: add RunContextConfig
- feat: add shell completion files for ll-builder
- feat(dbus): implement standard-compliant address parsing
- refactor: use overlayfs
- refactor: redesign prune logic
- refactor: redesign overlayfs API and add coverage
- refactor: add polkit authorization to PM
- refactor: remove monolithic clearReference
- refactor: unify repo command handling in cli and builder
- refactor: fix compiler warnings across runtime and utils
- fix: build run use current uid/gid
- fix: resolve extension definitions from config
- fix: auto-disable XDP for non-conforming apps
- fix: correct xdp document fuse mount option
- fix: improve XOrg display handling
- fix: enter container error
- fix: improve desktop file path resolution
- fix: follow symlinks when iterating entries directory
- fix: register QDBusObjectPath meta type explicitly
- fix: remove incorrect use of std::move
- fix: exclude systemd files from merge output
* Wed May 20 2026 dengbo <dengbo@deepin.org> - 1.12.4-1
- fix: include config.json in debian package install
- feat: add system config for device mode
- feat: update polkit requirement for linglong-bin
* Fri Apr 24 2026 dengbo <dengbo@deepin.org> - 1.12.3-1
- refactor: redesign prune logic
- feat(dbus): implement standard-compliant address parsing
- fix: follow symlinks when iterating entries directory
- fix: enter container error
- fix: register QDBusObjectPath meta type explicitly
- refactor: split repo load/create
- repo: add fallback when ostree_commit_get_object_sizes is unavailable
* Sat Mar 21 2026 dengbo <dengbo@deepin.org> - 1.12.2-1
- feat: add RISC-V 64 architecture support
- fix: improve XOrg display handling
- fix: improve desktop file path resolution in content command
- fix: exclude systemd files from merge output
- feat: add shell completion files for ll-builder
- fix: remove incorrect use of std::move
