#!/bin/sh
#
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -eu

OBS_BASE_URL=${LINYAPS_OBS_BASE_URL:-https://ppa.linyaps.org.cn/release/}
LATEST_OBS_BASE_URL=https://ppa.linyaps.org.cn/latest/
OS_RELEASE_FILE=${LINYAPS_OS_RELEASE_FILE:-/etc/os-release}
OS_VERSION_FILE=${LINYAPS_OS_VERSION_FILE:-/etc/os-version}
REPOSITORY_OVERRIDE=${LINYAPS_REPO_DIR:-}
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

log() {
    printf '%s\n' "$*"
}

die() {
    printf 'Error: %s\n' "$*" >&2
    exit 1
}

usage() {
    cat << 'EOF'
Usage: install.sh [full] [latest]

Install Linyaps for end users. Use "full" to also install the
linglong-builder package for application development. Use "latest" to
install packages from the latest repository instead of the release repository.

Environment:
  LINYAPS_REPO_DIR          use this repository name instead of the one
                            detected from the distribution
  LINYAPS_OS_RELEASE_FILE   read the distribution ID from this file
                            (default: /etc/os-release)
  LINYAPS_OS_VERSION_FILE   read the UOS minor version from this file
                            (default: /etc/os-version)
  LINYAPS_DRY_RUN=1         print the commands instead of running them
EOF
}

print_command() {
    printf '+'
    for argument; do
        printf ' %s' "$argument"
    done
    printf '\n'
}

run() {
    if [ "$DRY_RUN" = "1" ]; then
        print_command "$@"
        return 0
    fi

    "$@"
}

run_as_root() {
    if [ "$(id -u)" -eq 0 ]; then
        run "$@"
        return
    fi

    command -v sudo > /dev/null 2>&1 ||
        die "root privileges are required, but sudo is not installed"
    run sudo "$@"
}

require_command() {
    [ "$DRY_RUN" = "1" ] && return 0

    command -v "$1" > /dev/null 2>&1 ||
        die "required command not found: $1"
}

write_root_file() {
    destination=$1
    content=$2

    if [ "$DRY_RUN" = "1" ]; then
        log "+ write $destination"
        return
    fi

    printf '%s\n' "$content" | run_as_root tee "$destination" > /dev/null
}

install_with_apt() {
    repository=$1
    require_command apt-get

    run_as_root mkdir -p /etc/apt/sources.list.d
    write_root_file /etc/apt/sources.list.d/linglong.list \
        "deb [trusted=yes] ${OBS_BASE_URL%/}/$repository/ ./"

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

install_with_dnf() {
    repository=$1
    require_command dnf

    repo_url=${OBS_BASE_URL%/}/$repository
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

install_with_pacman() {
    require_command pacman
    # This distribution's linyaps package includes ll-builder.
    log "Installing or updating Linyaps from the distribution repository..."
    run_as_root pacman -Syu --needed --noconfirm linyaps
}

install_on_aosc() {
    require_command oma
    # AOSC OS ships ll-cli and ll-builder together in the linyaps package.
    log "Refreshing package metadata..."
    run_as_root oma refresh
    log "Installing or updating Linyaps from the AOSC OS repository..."
    run_as_root oma install -y linyaps
}

update_existing_installation() {
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
            if command -v apt-get > /dev/null 2>&1; then
                run_as_root env DEBIAN_FRONTEND=noninteractive apt-get update
                set -- "$CLI_PACKAGE"
                if [ "$INSTALL_MODE" = "full" ]; then
                    set -- "$@" "$BUILDER_PACKAGE"
                fi
                run_as_root env DEBIAN_FRONTEND=noninteractive \
                    apt-get install -y "$@"
            elif command -v dnf > /dev/null 2>&1; then
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

show_nixos_instructions() {
    cat >&2 << 'EOF'
NixOS manages Linyaps declaratively, so this script will not modify your
NixOS configuration. Add the following option to configuration.nix and rebuild:

  services.linyaps.enable = true;

  sudo nixos-rebuild switch --upgrade

On a flake-based system, run your usual nixos-rebuild command with --upgrade.
EOF
    exit 2
}

fetch_url() {
    if command -v curl > /dev/null 2>&1; then
        curl -fsSL --max-time 15 "$1"
    elif command -v wget > /dev/null 2>&1; then
        wget -qO- --timeout=15 "$1"
    else
        printf '%s\n' \
            "Cannot probe the remote repository: neither curl nor wget is installed." >&2
        return 1
    fi
}

list_obs_repositories() {
    repository_index=$(fetch_url "${OBS_BASE_URL%/}/") || return 1

    printf '%s\n' "$repository_index" | LC_ALL=C awk '
        match($0, /href="[^"]+\/"/) {
            print substr($0, RSTART + 6, RLENGTH - 8)
        }
    '
}

probe_obs_repository() {
    # Repository names follow the naming used by the packagers, which is not
    # always the ID from os-release: openSUSE Leap reports ID=opensuse-leap
    # while its repository is openSUSE_16.0. Try the full ID first, then the ID
    # without its variant suffix ("-leap", "-tumbleweed", ...).
    repository_suffix=${ID}_${VERSION_ID}
    repository_suffix_without_variant=${ID%%-*}_${VERSION_ID}

    repository=$(list_obs_repositories | LC_ALL=C awk -v suffix="$repository_suffix" \
        -v variant_suffix="$repository_suffix_without_variant" '
        function ends_with(text, wanted) {
            return length(text) >= length(wanted) &&
                substr(text, length(text) - length(wanted) + 1) == wanted
        }
        BEGIN {
            suffix = tolower(suffix)
            variant_suffix = tolower(variant_suffix)
        }
        {
            normalized = tolower($0)
            if (ends_with(normalized, suffix) ||
                (variant_suffix != suffix && ends_with(normalized, variant_suffix))) {
                print $0
                exit
            }
        }
    ') || return 1
    [ -n "$repository" ] || return 1

    printf '%s\n' "$repository"
}

probe_repository_type() {
    repository_url=${OBS_BASE_URL%/}/$1/
    repository_index=$(fetch_url "$repository_url") || return 1

    printf '%s\n' "$repository_index" | LC_ALL=C awk '
        match($0, /href="[^"]+"/) {
            candidate = substr($0, RSTART + 6, RLENGTH - 7)
            if (candidate == "Release") {
                has_release = 1
            } else if (tolower(candidate) ~ /\.repo$/) {
                has_repo = 1
            }
        }
        END {
            if (has_release && !has_repo) {
                print "apt"
                exit 0
            }
            if (has_repo && !has_release) {
                print "dnf"
                exit 0
            }
            exit 1
        }
    '
}

install_from_obs_repository() {
    repository=$1
    if ! repository_type=$(probe_repository_type "$repository"); then
        die "cannot determine package manager for repository: $repository"
    fi

    case "$repository_type" in
        apt) install_with_apt "$repository" ;;
        dnf) install_with_dnf "$repository" ;;
    esac
}

select_uos_repository() {
    if [ -r "$OS_VERSION_FILE" ]; then
        uos_minor_version=$(LC_ALL=C awk -F= '
            $1 ~ /^[[:space:]]*MinorVersion[[:space:]]*$/ {
                value = $2
                sub(/^[[:space:]]*/, "", value)
                sub(/[[:space:]]*$/, "", value)
                print value
                exit
            }
        ' "$OS_VERSION_FILE")
        case "$uos_minor_version" in
            "" | *[!0-9]*) ;;
            *)
                printf 'uos_%s\n' "$uos_minor_version"
                return 0
                ;;
        esac
    fi

    # Minimal images (and containers built from them) may not ship
    # /etc/os-version. A single uos repository is unambiguous in that case.
    uos_repositories=$(list_obs_repositories | LC_ALL=C awk 'tolower($0) ~ /^uos_/') ||
        return 1
    uos_repository_count=$(printf '%s\n' "$uos_repositories" | grep -c . || true)
    [ "$uos_repository_count" -eq 1 ] || return 1

    printf '%s\n' "$uos_repositories"
}

select_obs_repository() {
    if [ -n "$REPOSITORY_OVERRIDE" ]; then
        printf '%s\n' "${REPOSITORY_OVERRIDE%/}"
        return 0
    fi

    case "$OS_ID" in
        uos) select_uos_repository || probe_obs_repository ;;
        *) probe_obs_repository ;;
    esac
}

for argument; do
    case "$argument" in
        full)
            INSTALL_MODE=full
            ;;
        latest)
            OBS_BASE_URL=$LATEST_OBS_BASE_URL
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            usage >&2
            die "unsupported argument: $argument"
            ;;
    esac
done

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
if command -v ll-cli > /dev/null 2>&1; then
    ALREADY_INSTALLED=1
    log "Linyaps is already installed; checking for an update."
    ll-cli --version 2> /dev/null || true
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
                die "unsupported distribution or version: $OS_NAME
No repository matching ID=$ID VERSION_ID=$VERSION_ID was found in ${OBS_BASE_URL%/}/.
Set LINYAPS_REPO_DIR=<repository> to select one of the repositories listed there."
            fi
        fi
        [ -z "$repository" ] || install_from_obs_repository "$repository"
        ;;
esac

if [ "$DRY_RUN" = "1" ]; then
    log "Dry run completed."
    exit 0
fi

command -v ll-cli > /dev/null 2>&1 ||
    die "the package manager completed, but ll-cli was not found"

if [ "$INSTALL_MODE" = "full" ]; then
    command -v ll-builder > /dev/null 2>&1 ||
        die "the package manager completed, but ll-builder was not found"
fi

log "Linyaps is ready."
ll-cli --version
if [ "$INSTALL_MODE" = "full" ]; then
    ll-builder --version
fi
