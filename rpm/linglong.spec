Name:           linglong
Version:        1.4.3
Release:        1
Summary:        Linglong Package FrameWork
License:        LGPLv3
URL:            https://github.com/linuxdeepin/%{name}
Source0:        %{url}/archive/%{version}/linglong-%{version}.tar.gz

BuildRequires:  cmake gcc-c++
BuildRequires:  qt5-qtbase-devel qt5-qtwebsockets-devel qt5-qtbase-private-devel
BuildRequires:  glib2-devel nlohmann-json-devel ostree-devel yaml-cpp-devel
BuildRequires:  systemd-devel gtest-devel libseccomp-devel elfutils-libelf-devel
Requires:       linglong-bin = %{version}-%{release}

%description
This package is a linglong package framework.

%package        -n linglong-bin
Summary:        Linglong package manager
Requires:       linglong-box = %{version}-%{release}
%description    -n linglong-bin
Linglong package management command line tool.

%package        -n linglong-builder
Summary:        Linglong build tools
Requires:       linglong-box = %{version}-%{release} linglong-bin = %{version}-%{release}
%description    -n linglong-builder
This package is a tool that makes it easy to build applications and dependencies.

%package        -n linglong-box
Summary:        Linglong sandbox
%description    -n linglong-box
Linglong sandbox with OCI standard.

%prep
%autosetup -p1 -n linglong-%{version}

%define _debugsource_template %{nil}

%build
export PATH=%{_qt5_bindir}:$PATH
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX:PATH=%{_prefix} \
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
%{_bindir}/linglong-repair-tool
%{_prefix}/lib/%{name}/container/*
%{_prefix}/lib/sysusers.d/*.conf
%{_prefix}/lib/systemd/system/*.service
%{_prefix}/lib/systemd/system-preset/*.preset
%{_prefix}/lib/systemd/user/*
%{_prefix}/lib/systemd/system-environment-generators/*
%{_prefix}/lib/systemd/user-environment-generators/*
%{_libexecdir}/%{name}/ll-package-manager
%{_libexecdir}/%{name}/00-id-mapping
%{_libexecdir}/%{name}/05-initialize
%{_libexecdir}/%{name}/20-devices
%{_libexecdir}/%{name}/30-user-home
%{_libexecdir}/%{name}/40-host-ipc
%{_libexecdir}/%{name}/90-legacy
%{_libexecdir}/%{name}/create-linglong-dirs
%{_libexecdir}/%{name}/upgrade-all
%{_datadir}/bash-completion/completions/ll-cli
%{_datadir}/dbus-1/system-services/*.service
%{_datadir}/dbus-1/system.d/*.conf
%{_datadir}/%{name}/config.yaml
%{_datadir}/mime/packages/*
%{_datadir}/%{name}/api/api.json
%{_datadir}/applications/*.desktop

%files -n linglong-builder
%license LICENSE
%{_bindir}/ll-builder
%{_libexecdir}/%{name}/fetch-dsc-repo
%{_libexecdir}/%{name}/fetch-git-repo
%{_libexecdir}/%{name}/app-conf-generator
%{_libexecdir}/%{name}/builder/helper/*.sh
%{_datadir}/bash-completion/completions/ll-builder
%{_datadir}/%{name}/builder/templates/*.yaml

%files -n linglong-box
%license LICENSE
%{_bindir}/ll-box

%changelog
* Thu Apr 25 2024 chenhuixing <chenhuixing@deepin.org> - 1.4.3-1
- Init project
