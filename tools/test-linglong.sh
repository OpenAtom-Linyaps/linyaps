#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -Eeuo pipefail

SMOKE_REPO_NAME="${SMOKE_REPO_NAME:-smoketesting}"
SMOKE_REPO_URL="${SMOKE_REPO_URL:-https://repo-dev.cicd.getdeepin.org}"

DEMO_APP_ID="${DEMO_APP_ID:-org.deepin.demo}"
DEMO_VERSION="${DEMO_VERSION:-0.0.0.1}"
DEMO_ARCH="${DEMO_ARCH:-$(uname -m)}"
DEMO_CHANNEL="${DEMO_CHANNEL:-main}"
DEMO_PROJECT_DIR="${DEMO_PROJECT_DIR:-${DEMO_APP_ID}}"

CALENDAR_APP_ID="${CALENDAR_APP_ID:-org.dde.calendar}"
CALENDAR_VERSION="${CALENDAR_VERSION:-5.13.1.1}"
CALENDAR_MODULE_BASE_VERSION="${CALENDAR_MODULE_BASE_VERSION:-5.14.4.102}"
CALENDAR_MODULE_DOWNGRADE_VERSION="${CALENDAR_MODULE_DOWNGRADE_VERSION:-5.14.4.101}"

SEMVER_APP_ID="${SEMVER_APP_ID:-org.deepin.semver.demo}"
SEMVER_OLD_VERSION="${SEMVER_OLD_VERSION:-1.0.0.0}"

TESTSUITE_BASELINE_APP_ID="${TESTSUITE_BASELINE_APP_ID:-cn.org.linyaps.testsuite.baseline}"
TESTSUITE_BASELINE_TIMEOUT="${TESTSUITE_BASELINE_TIMEOUT:-300}"

LL_CLI="${LL_CLI:-ll-cli}"
LL_BUILDER="${LL_BUILDER:-ll-builder}"

CURRENT_DEFAULT_REPO=""
CURRENT_HIGHEST_PRIORITY_REPO=""
CURRENT_HIGHEST_PRIORITY=""
CURRENT_LL_CLI_DEFAULT_REPO=""
CURRENT_LL_CLI_HIGHEST_PRIORITY_REPO=""
CURRENT_LL_CLI_HIGHEST_PRIORITY=""
CURRENT_LL_BUILDER_DEFAULT_REPO=""
CURRENT_LL_BUILDER_HIGHEST_PRIORITY_REPO=""
CURRENT_LL_BUILDER_HIGHEST_PRIORITY=""
CURRENT_STEP_TITLE=""

COLOR_RESET=""
COLOR_RED=""
COLOR_GREEN=""
COLOR_BLUE=""

if [[ "${TRACE:-0}" == "1" ]]; then
    set -x
fi

init_colors()
{
    if [[ -n "${NO_COLOR:-}" || ( ! -t 1 && ! -t 2 ) ]]; then
        return 0
    fi

    COLOR_RESET=$'\033[0m'
    COLOR_RED=$'\033[31m'
    COLOR_GREEN=$'\033[92m'
    COLOR_BLUE=$'\033[34m'
}

print_step_result()
{
    local result="$1"
    local title="$2"
    local color="$3"
    local output="${4:-/dev/stdout}"

    printf '%s[%s]%s %s\n' "${color}" "${result}" "${COLOR_RESET}" "${title}" > "${output}"
}

on_error()
{
    local exit_code=$?
    local command="${BASH_COMMAND}"
    local i

    if [[ -n "${CURRENT_STEP_TITLE}" ]]; then
        print_step_result "FAIL" "${CURRENT_STEP_TITLE}" "${COLOR_RED}" /dev/stderr
        CURRENT_STEP_TITLE=""
    fi

    printf '\nERROR: command failed with exit code %s\n' "${exit_code}" >&2
    printf 'ERROR: command: %s\n' "${command}" >&2

    for ((i = 0; i < ${#FUNCNAME[@]} - 1; i++)); do
        printf 'ERROR: at %s:%s in %s()\n' \
            "${BASH_SOURCE[$((i + 1))]}" \
            "${BASH_LINENO[$i]}" \
            "${FUNCNAME[$((i + 1))]}" >&2
    done

    return "${exit_code}"
}

log()
{
    printf '\n%s==>%s %s\n' "${COLOR_BLUE}" "${COLOR_RESET}" "$*"
}

run_step()
{
    local title="$1"
    shift

    log "${title}"
    CURRENT_STEP_TITLE="${title}"
    "$@"
    print_step_result "PASS" "${title}" "${COLOR_GREEN}"
    CURRENT_STEP_TITLE=""
}

require_commands()
{
    local command

    for command in "$@"; do
        if ! command -v "${command}" >/dev/null 2>&1; then
            printf 'Missing required command: %s\n' "${command}" >&2
            exit 127
        fi
    done
}

sudo_ll_cli()
{
    sudo "${LL_CLI}" "$@"
}

remove_demo_project_dir()
{
    if [[ ! -e "${DEMO_PROJECT_DIR}" ]]; then
        return 0
    fi

    if rm -rf "${DEMO_PROJECT_DIR}" 2> /dev/null; then
        return 0
    fi

    chmod -R 755 "${DEMO_PROJECT_DIR}"
    rm -rf "${DEMO_PROJECT_DIR}"
}

reset_repositories()
{
    sudo_ll_cli repo remove "${SMOKE_REPO_NAME}" 2> /dev/null || true
    "${LL_BUILDER}" repo remove "${SMOKE_REPO_NAME}" 2> /dev/null || true
}

record_repo_state()
{
    local repo_command="$1"
    local repo_label="$2"
    local default_repo_var="$3"
    local highest_priority_repo_var="$4"
    local highest_priority_var="$5"
    local repo_show_output
    local highest_priority_info
    local -n default_repo="${default_repo_var}"
    local -n highest_priority_repo="${highest_priority_repo_var}"
    local -n highest_priority="${highest_priority_var}"

    if ! repo_show_output="$("${repo_command}" repo show)"; then
        printf 'Failed to run %s repo show.\n' "${repo_label}" >&2
        return 1
    fi
    default_repo="$(
        printf '%s\n' "${repo_show_output}" \
            | awk '$1 == "Default:" { print $2; exit }'
    )"
    highest_priority_info="$(
        printf '%s\n' "${repo_show_output}" \
            | awk '
                {
                    gsub(/\033\[[0-9;]*m/, "")
                }
                NF >= 4 && $NF ~ /^-?[0-9]+$/ {
                    print $(NF - 1), $NF
                    exit
                }
            '
    )"
    highest_priority_repo="${highest_priority_info% *}"
    highest_priority="${highest_priority_info##* }"

    if [[ -z "${default_repo}" || -z "${highest_priority_repo}" \
        || -z "${highest_priority}" ]]; then
        printf 'Failed to parse default repo or highest priority repo from %s repo show:\n%s\n' \
            "${repo_label}" "${repo_show_output}" >&2
        return 1
    fi

    printf 'Current %s default repo: %s\n' "${repo_label}" "${default_repo}"
    printf 'Current %s highest priority repo: %s (%s)\n' \
        "${repo_label}" "${highest_priority_repo}" "${highest_priority}"
}

record_current_repo_state()
{
    record_repo_state \
        "${LL_CLI}" \
        "ll-cli" \
        CURRENT_LL_CLI_DEFAULT_REPO \
        CURRENT_LL_CLI_HIGHEST_PRIORITY_REPO \
        CURRENT_LL_CLI_HIGHEST_PRIORITY
    record_repo_state \
        "${LL_BUILDER}" \
        "ll-builder" \
        CURRENT_LL_BUILDER_DEFAULT_REPO \
        CURRENT_LL_BUILDER_HIGHEST_PRIORITY_REPO \
        CURRENT_LL_BUILDER_HIGHEST_PRIORITY

    CURRENT_DEFAULT_REPO="${CURRENT_LL_CLI_DEFAULT_REPO}"
    CURRENT_HIGHEST_PRIORITY_REPO="${CURRENT_LL_CLI_HIGHEST_PRIORITY_REPO}"
    CURRENT_HIGHEST_PRIORITY="${CURRENT_LL_CLI_HIGHEST_PRIORITY}"
}

next_repo_priority()
{
    local current_priority="$1"

    if [[ ! "${current_priority}" =~ ^-?[0-9]+$ ]]; then
        printf 'Invalid repo priority: %s\n' "${current_priority}" >&2
        return 1
    fi

    printf '%d\n' "$((current_priority + 100))"
}

configure_smoke_repositories()
{
    local ll_cli_smoke_priority
    local ll_builder_smoke_priority

    ll_cli_smoke_priority="$(next_repo_priority "${CURRENT_LL_CLI_HIGHEST_PRIORITY}")"
    ll_builder_smoke_priority="$(next_repo_priority "${CURRENT_LL_BUILDER_HIGHEST_PRIORITY}")"

    sudo_ll_cli repo add "${SMOKE_REPO_NAME}" "${SMOKE_REPO_URL}"
    sudo_ll_cli repo set-priority "${SMOKE_REPO_NAME}" "${ll_cli_smoke_priority}"

    "${LL_BUILDER}" repo add "${SMOKE_REPO_NAME}" "${SMOKE_REPO_URL}"
    "${LL_BUILDER}" repo set-priority "${SMOKE_REPO_NAME}" "${ll_builder_smoke_priority}"
}

verify_repo_state_restored()
{
    local repo_command="$1"
    local repo_label="$2"
    local expected_default_repo="$3"
    local expected_highest_priority_repo="$4"
    local expected_highest_priority="$5"
    local actual_default_repo=""
    local actual_highest_priority_repo=""
    local actual_highest_priority=""
    local failed=0

    if [[ -z "${expected_default_repo}" || -z "${expected_highest_priority_repo}" \
        || -z "${expected_highest_priority}" ]]; then
        printf 'Skip %s repo state comparison: initial state was not recorded.\n' \
            "${repo_label}" >&2
        return 0
    fi

    record_repo_state \
        "${repo_command}" \
        "${repo_label}" \
        actual_default_repo \
        actual_highest_priority_repo \
        actual_highest_priority || return 1

    if [[ "${actual_default_repo}" != "${expected_default_repo}" ]]; then
        printf '%s default repo changed: before=%s after=%s\n' \
            "${repo_label}" "${expected_default_repo}" "${actual_default_repo}" >&2
        failed=1
    fi

    if [[ "${actual_highest_priority_repo}" != "${expected_highest_priority_repo}" ]]; then
        printf '%s highest priority repo changed: before=%s after=%s\n' \
            "${repo_label}" "${expected_highest_priority_repo}" "${actual_highest_priority_repo}" >&2
        failed=1
    fi

    if [[ "${actual_highest_priority}" != "${expected_highest_priority}" ]]; then
        printf '%s highest priority changed: before=%s after=%s\n' \
            "${repo_label}" "${expected_highest_priority}" "${actual_highest_priority}" >&2
        failed=1
    fi

    return "${failed}"
}

verify_all_repo_states_restored()
{
    local failed=0

    verify_repo_state_restored \
        "${LL_CLI}" \
        "ll-cli" \
        "${CURRENT_LL_CLI_DEFAULT_REPO}" \
        "${CURRENT_LL_CLI_HIGHEST_PRIORITY_REPO}" \
        "${CURRENT_LL_CLI_HIGHEST_PRIORITY}" || failed=1
    verify_repo_state_restored \
        "${LL_BUILDER}" \
        "ll-builder" \
        "${CURRENT_LL_BUILDER_DEFAULT_REPO}" \
        "${CURRENT_LL_BUILDER_HIGHEST_PRIORITY_REPO}" \
        "${CURRENT_LL_BUILDER_HIGHEST_PRIORITY}" || failed=1

    return "${failed}"
}

cleanup()
{
    local exit_code=$?
    local cleanup_exit_code=0

    popd >/dev/null 2>&1 || true
    "${LL_CLI}" kill -s 9 "${CALENDAR_APP_ID}" >/dev/null 2>&1 || true
    sudo_ll_cli uninstall "${DEMO_APP_ID}" >/dev/null 2>&1 || true
    sudo_ll_cli uninstall "${CALENDAR_APP_ID}" >/dev/null 2>&1 || true
    sudo_ll_cli uninstall "${SEMVER_APP_ID}" >/dev/null 2>&1 || true
    sudo_ll_cli uninstall "${TESTSUITE_BASELINE_APP_ID}" >/dev/null 2>&1 || true
    remove_demo_project_dir || cleanup_exit_code=1
    reset_repositories

    verify_all_repo_states_restored || cleanup_exit_code=1

    if [[ ${exit_code} -eq 0 && ${cleanup_exit_code} -ne 0 ]]; then
        exit_code="${cleanup_exit_code}"
    fi

    if [[ ${exit_code} -eq 0 ]]; then
        log "成功执行玲珑冒烟测试"
    else
        printf '\nSmoke testing failed with exit code %s\n' "${exit_code}" >&2
    fi

    exit "${exit_code}"
}

create_demo_project()
{
    remove_demo_project_dir
    "${LL_BUILDER}" create "${DEMO_APP_ID}"
}

test_demo_build_and_export()
{
    pushd "${DEMO_PROJECT_DIR}" >/dev/null

    "${LL_BUILDER}" build
    "${LL_BUILDER}" export --layer
    "${LL_BUILDER}" export
    "${LL_BUILDER}" run
}

test_demo_dbus_environment()
{
    local session_address="${DBUS_SESSION_BUS_ADDRESS:-}"
    local system_address="${DBUS_SYSTEM_BUS_ADDRESS:-unix:path=/var/run/dbus/system_bus_socket}"

    "${LL_BUILDER}" run -- bash -c "export" | grep -q "DBUS_SESSION_BUS_ADDRESS"
    "${LL_BUILDER}" run -- bash -c "export" | grep -q "DBUS_SYSTEM_BUS_ADDRESS"

    DBUS_SESSION_BUS_ADDRESS="${session_address},test=1" \
        "${LL_BUILDER}" run -- bash -c "export" | grep "DBUS_SESSION_BUS_ADDRESS" | grep -q "test=1"

    DBUS_SYSTEM_BUS_ADDRESS="${system_address},test=2" \
        "${LL_BUILDER}" run -- bash -c "export" | grep "DBUS_SYSTEM_BUS_ADDRESS" | grep -q "test=2"
}

test_demo_install_and_run()
{
    local layer_file="${DEMO_APP_ID}_${DEMO_VERSION}_${DEMO_ARCH}_binary.layer"
    local uab_file="${DEMO_APP_ID}_${DEMO_VERSION}_${DEMO_ARCH}_${DEMO_CHANNEL}.uab"

    sudo_ll_cli uninstall "${DEMO_APP_ID}" || true

    "./${uab_file}"
    sudo_ll_cli install "${uab_file}"
    sudo_ll_cli uninstall "${DEMO_APP_ID}" || true
    sudo_ll_cli install "${layer_file}"

    timeout 10 "${LL_CLI}" run "${DEMO_APP_ID}"

    popd >/dev/null
    remove_demo_project_dir
}

test_repository_queries()
{
    "${LL_CLI}" list >/dev/null
    "${LL_CLI}" search calendar >/dev/null
    "${LL_CLI}" search deepin >/dev/null
    "${LL_CLI}" search deepin --type=runtime >/dev/null
}

test_calendar_install_upgrade_run()
{
    sudo_ll_cli uninstall "${CALENDAR_APP_ID}" || true

    sudo_ll_cli install "${CALENDAR_APP_ID}"
    sudo_ll_cli uninstall "${CALENDAR_APP_ID}"

    sudo_ll_cli install "${CALENDAR_APP_ID}/${CALENDAR_VERSION}"
    sudo_ll_cli upgrade "${CALENDAR_APP_ID}"

    "${LL_CLI}" run "${CALENDAR_APP_ID}" &
    sleep 5
    "${LL_CLI}" kill -s 9 "${CALENDAR_APP_ID}"
    sleep 3

    sudo_ll_cli uninstall "${CALENDAR_APP_ID}"
}

list_calendar_modules()
{
    "${LL_CLI}" list | grep "${CALENDAR_APP_ID}" || true
}

assert_calendar_module_present()
{
    local module="$1"
    local modules

    modules="$(list_calendar_modules)"
    if ! grep -q "${module}" <<< "${modules}"; then
        printf 'Expected module is missing for %s: %s\n' "${CALENDAR_APP_ID}" "${module}" >&2
        printf 'Current modules:\n%s\n' "${modules}" >&2
        return 1
    fi
}

assert_calendar_module_absent()
{
    local module="$1"
    local modules

    modules="$(list_calendar_modules)"
    if grep -q "${module}" <<< "${modules}"; then
        printf 'Unexpected module remains for %s: %s\n' "${CALENDAR_APP_ID}" "${module}" >&2
        printf 'Current modules:\n%s\n' "${modules}" >&2
        return 1
    fi
}

assert_calendar_modules_after_downgrade()
{
    assert_calendar_module_present "binary"
    assert_calendar_module_present "develop"
    assert_calendar_module_present "lang-ja"
    assert_calendar_module_absent "unuse"
}

assert_calendar_modules_after_upgrade()
{
    assert_calendar_module_present "binary"
    assert_calendar_module_present "develop"
    assert_calendar_module_absent "lang-ja"
}

test_calendar_module_lifecycle()
{
    sudo_ll_cli uninstall "${CALENDAR_APP_ID}" || true

    sudo_ll_cli install "${CALENDAR_APP_ID}/${CALENDAR_MODULE_BASE_VERSION}"
    sudo_ll_cli install --module develop "${CALENDAR_APP_ID}"
    sudo_ll_cli install --module unuse "${CALENDAR_APP_ID}"
    sudo_ll_cli install --module lang-ja "${CALENDAR_APP_ID}"

    sudo_ll_cli install --force "${CALENDAR_APP_ID}/${CALENDAR_MODULE_DOWNGRADE_VERSION}"
    assert_calendar_modules_after_downgrade

    sudo_ll_cli upgrade "${CALENDAR_APP_ID}"
    assert_calendar_modules_after_upgrade
}

test_semver_upgrade_flow()
{
    "${LL_CLI}" search "${SEMVER_APP_ID}"

    sudo_ll_cli uninstall "${SEMVER_APP_ID}" || true
    sudo_ll_cli install "${SEMVER_APP_ID}"
    "${LL_CLI}" list | grep -q "${SEMVER_APP_ID}"

    sudo_ll_cli uninstall "${SEMVER_APP_ID}"
    sudo_ll_cli install "${SEMVER_APP_ID}/${SEMVER_OLD_VERSION}"
    sudo_ll_cli upgrade "${SEMVER_APP_ID}"
    "${LL_CLI}" list | grep -q "${SEMVER_APP_ID}"

    sudo_ll_cli uninstall "${SEMVER_APP_ID}"
    sudo_ll_cli install "${SEMVER_APP_ID}/${SEMVER_OLD_VERSION}" --force
    "${LL_CLI}" list | grep -q "${SEMVER_APP_ID}"
}

test_testsuite_baseline()
{
    local test_exit_code

    sudo_ll_cli uninstall "${TESTSUITE_BASELINE_APP_ID}" || true
    sudo_ll_cli install "${TESTSUITE_BASELINE_APP_ID}"

    set +e
    timeout "${TESTSUITE_BASELINE_TIMEOUT}" "${LL_CLI}" run "${TESTSUITE_BASELINE_APP_ID}"
    test_exit_code=$?
    set -e

    sudo_ll_cli uninstall "${TESTSUITE_BASELINE_APP_ID}"

    return "${test_exit_code}"
}

main()
{
    init_colors
    require_commands "${LL_CLI}" "${LL_BUILDER}" awk chmod grep sudo timeout uname
    trap on_error ERR
    trap cleanup EXIT

    log "开始玲珑冒烟测试"

    run_step "清理并重置仓库" reset_repositories
    run_step "记录 ll-cli 和 ll-builder 当前仓库状态" record_current_repo_state
    run_step "配置冒烟测试仓库" configure_smoke_repositories
    run_step "创建 demo 项目" create_demo_project
    run_step "构建并导出 demo 应用" test_demo_build_and_export
    run_step "验证 demo DBus 环境变量" test_demo_dbus_environment
    run_step "安装并运行 demo 应用" test_demo_install_and_run
    run_step "查询仓库与运行时信息" test_repository_queries
    run_step "安装、升级并运行日历应用" test_calendar_install_upgrade_run
    run_step "验证日历模块生命周期" test_calendar_module_lifecycle
    run_step "验证 versionV1 到 versionV2 升降级" test_semver_upgrade_flow
    run_step "安装并运行 baseline 测试套件" test_testsuite_baseline
}

main "$@"
