#!/bin/sh
#
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -eu

OBS_BASE_URL=${LINYAPS_OBS_BASE_URL:-https://ci.deepin.com/repo/obs/linglong:/CI:/release}
OS_RELEASE_FILE=${LINYAPS_OS_RELEASE_FILE:-/etc/os-release}
DRY_RUN=${LINYAPS_DRY_RUN:-0}
INSTALL_MODE=default

# The OBS repositories use different binary package names for the web store
# installer: DEB repositories publish linglong-installer, while RPM
# repositories publish linyaps-web-store-installer. There is no
# linglong-web-store-installer binary package in the supported repositories.
CLI_PACKAGE=linglong-bin
BUILDER_PACKAGE=linglong-builder
DEB_INSTALLER_PACKAGE=linglong-installer
RPM_INSTALLER_PACKAGE=linyaps-web-store-installer

log()
{
    printf '%s\n' "$*"
}

die()
{
    printf 'Error: %s\n' "$*" >&2
    exit 1
}

usage()
{
    cat <<'EOF'
Usage: install.sh [full]

Install Linyaps for end users. Use "full" to also install the
linglong-builder package for application development.
EOF
}

print_command()
{
    printf '+'
    for argument do
        printf ' %s' "$argument"
    done
    printf '\n'
}

run()
{
    if [ "$DRY_RUN" = "1" ]; then
        print_command "$@"
        return 0
    fi

    "$@"
}

run_as_root()
{
    if [ "$(id -u)" -eq 0 ]; then
        run "$@"
        return
    fi

    command -v sudo >/dev/null 2>&1 ||
        die "root privileges are required, but sudo is not installed"
    run sudo "$@"
}

require_command()
{
    [ "$DRY_RUN" = "1" ] && return 0

    command -v "$1" >/dev/null 2>&1 ||
        die "required command not found: $1"
}

write_root_file()
{
    destination=$1
    content=$2

    if [ "$DRY_RUN" = "1" ]; then
        log "+ write $destination"
        return
    fi

    printf '%s\n' "$content" | run_as_root tee "$destination" >/dev/null
}

install_with_apt()
{
    repository=$1
    require_command apt-get

    run_as_root mkdir -p /etc/apt/sources.list.d
    write_root_file /etc/apt/sources.list.d/linglong.list \
        "deb [trusted=yes] $OBS_BASE_URL/$repository/ ./"

    log "Refreshing package metadata..."
    run_as_root env DEBIAN_FRONTEND=noninteractive apt-get update
    log "Installing or updating Linyaps..."
    set -- "$CLI_PACKAGE" "$DEB_INSTALLER_PACKAGE"
    if [ "$INSTALL_MODE" = "full" ]; then
        set -- "$@" "$BUILDER_PACKAGE"
    fi
    run_as_root env DEBIAN_FRONTEND=noninteractive \
        apt-get install -y "$@"
}

install_with_dnf()
{
    repository=$1
    require_command dnf

    repo_url=$OBS_BASE_URL/$repository
    repo_config="[linglong_CI_release]
name=linglong:CI:release ($repository)
baseurl=$repo_url/
enabled=1
gpgcheck=0
repo_gpgcheck=0"

    run_as_root mkdir -p /etc/yum.repos.d
    write_root_file \
        '/etc/yum.repos.d/linglong%3ACI%3Arelease.repo' \
        "$repo_config"

    log "Refreshing package metadata..."
    run_as_root dnf -y makecache --refresh
    log "Installing or updating Linyaps..."
    set -- "$CLI_PACKAGE" "$RPM_INSTALLER_PACKAGE"
    if [ "$INSTALL_MODE" = "full" ]; then
        set -- "$@" "$BUILDER_PACKAGE"
    fi
    run_as_root dnf -y install "$@"
}

install_with_pacman()
{
    require_command pacman
    # This distribution's linyaps package includes ll-builder.
    log "Installing or updating Linyaps from the distribution repository..."
    run_as_root pacman -Syu --needed --noconfirm linyaps
}

install_on_aosc()
{
    require_command oma
    # AOSC OS ships ll-cli and ll-builder together in the linyaps package.
    log "Refreshing package metadata..."
    run_as_root oma refresh
    log "Installing or updating Linyaps from the AOSC OS repository..."
    run_as_root oma install -y linyaps
}

update_existing_installation()
{
    log "This release has no current Linyaps repository mapping."
    log "Updating the existing package from the configured repositories..."

    case "$OS_ID" in
        arch | manjaro | parabola)
            install_with_pacman
            ;;
        aosc | aosc-os)
            install_on_aosc
            ;;
        fedora | openeuler | anolis)
            require_command dnf
            run_as_root dnf -y makecache --refresh
            set -- "$CLI_PACKAGE"
            if [ "$INSTALL_MODE" = "full" ]; then
                set -- "$@" "$BUILDER_PACKAGE"
            fi
            run_as_root dnf -y install "$@"
            ;;
        *)
            if command -v apt-get >/dev/null 2>&1; then
                run_as_root env DEBIAN_FRONTEND=noninteractive apt-get update
                set -- "$CLI_PACKAGE"
                if [ "$INSTALL_MODE" = "full" ]; then
                    set -- "$@" "$BUILDER_PACKAGE"
                fi
                run_as_root env DEBIAN_FRONTEND=noninteractive \
                    apt-get install -y "$@"
            elif command -v dnf >/dev/null 2>&1; then
                run_as_root dnf -y makecache --refresh
                set -- "$CLI_PACKAGE"
                if [ "$INSTALL_MODE" = "full" ]; then
                    set -- "$@" "$BUILDER_PACKAGE"
                fi
                run_as_root dnf -y install "$@"
            else
                die "cannot determine how to update the existing installation"
            fi
            ;;
    esac
}

show_nixos_instructions()
{
    cat >&2 <<'EOF'
NixOS manages Linyaps declaratively, so this script will not modify your
NixOS configuration. Add the following option to configuration.nix and rebuild:

  services.linyaps.enable = true;

  sudo nixos-rebuild switch --upgrade

On a flake-based system, run your usual nixos-rebuild command with --upgrade.
EOF
    exit 2
}

select_obs_repository()
{
    case "$OS_ID:$OS_VERSION" in
        deepin:23*) repository=Deepin_23 ;;
        deepin:25*) repository=Deepin_25 ;;
        debian:12*) repository=Debian_12 ;;
        debian:13*) repository=Debian_13 ;;
        ubuntu:24.04*) repository=xUbuntu_24.04 ;;
        ubuntu:25.04*) repository=Ubuntu_25.04 ;;
        ubuntu:25.10*) repository=Ubuntu_25.10 ;;
        fedora:42*) repository=Fedora_42 ;;
        fedora:43*) repository=Fedora_43 ;;
        openeuler:24.03*) repository=openEuler_24.03 ;;
        openeuler:25.03*) repository=openEuler_25.03 ;;
        anolis:23.3*) repository=AnolisOS_23.3 ;;
        anolis:23.4*) repository=AnolisOS_23.4 ;;
        openkylin:2.0*) repository=openkylin_2.0 ;;
        uos:*) repository=uos_1070 ;;
        *) return 1 ;;
    esac

    printf '%s\n' "$repository"
}

case "${1-}" in
    "")
        ;;
    full)
        INSTALL_MODE=full
        ;;
    -h | --help)
        usage
        exit 0
        ;;
    *)
        usage >&2
        die "unsupported installation mode: $1"
        ;;
esac

[ "$#" -le 1 ] || {
    usage >&2
    die "too many arguments"
}

[ -r "$OS_RELEASE_FILE" ] ||
    die "cannot read operating system information from $OS_RELEASE_FILE"

# OS release files contain shell-compatible variable assignments by definition.
# Clear the values first so environment variables cannot override missing fields.
ID=
VERSION_ID=
PRETTY_NAME=
# shellcheck disable=SC1090
. "$OS_RELEASE_FILE"

OS_ID=$(printf '%s' "$ID" | tr '[:upper:]' '[:lower:]')
OS_VERSION=$VERSION_ID
OS_NAME=${PRETTY_NAME:-$OS_ID $OS_VERSION}

ALREADY_INSTALLED=0
if command -v ll-cli >/dev/null 2>&1; then
    ALREADY_INSTALLED=1
    log "Linyaps is already installed; checking for an update."
    ll-cli --version 2>/dev/null || true
else
    log "Installing Linyaps on $OS_NAME."
fi

case "$OS_ID" in
    arch | manjaro | parabola)
        install_with_pacman
        ;;
    aosc | aosc-os)
        install_on_aosc
        ;;
    nixos)
        show_nixos_instructions
        ;;
    *)
        if ! repository=$(select_obs_repository); then
            if [ "$ALREADY_INSTALLED" = "1" ]; then
                update_existing_installation
                repository=
            else
                die "unsupported distribution or version: $OS_NAME"
            fi
        fi
        case "$repository" in
            Fedora_* | openEuler_* | AnolisOS_*)
                install_with_dnf "$repository"
                ;;
            ?*)
                install_with_apt "$repository"
                ;;
        esac
        ;;
esac

if [ "$DRY_RUN" = "1" ]; then
    log "Dry run completed."
    exit 0
fi

command -v ll-cli >/dev/null 2>&1 ||
    die "the package manager completed, but ll-cli was not found"

if [ "$INSTALL_MODE" = "full" ]; then
    command -v ll-builder >/dev/null 2>&1 ||
        die "the package manager completed, but ll-builder was not found"
fi

log "Linyaps is ready."
ll-cli --version
if [ "$INSTALL_MODE" = "full" ]; then
    ll-builder --version
fi
