#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

set -eu

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
checker="${script_dir}/layer-icon-checker.sh"
test_root=$(mktemp -d "${TMPDIR:-/tmp}/layer-icon-checker-test.XXXXXXXX")
fake_bin="${test_root}/bin"
log_file="${test_root}/calls.log"
layer_file="${test_root}/example.layer"

cleanup_test() {
	find "${test_root}" -depth -delete
}
trap cleanup_test EXIT

mkdir "${fake_bin}"
touch "${layer_file}"

cat >"${fake_bin}/fake-tool" <<'EOF'
#!/usr/bin/env bash
set -eu

case "${0##*/}" in
hexdump)
	case "$*" in
	*'1/40'*) printf '%s\n' '<<< deepin linglong layer archive >>>' ;;
	*) printf '%s\n' 1 ;;
	esac
	;;
erofsfuse)
	dest="${*: -1}"
	mkdir -p "${dest}/entries/applications"
	case "${LAYER_ICON_TEST_CASE}" in
	no-icon)
		printf '%s\n' '[Desktop Entry]' >"${dest}/entries/applications/app.desktop"
		;;
	fail)
		printf '%s\n' '[Desktop Entry]' 'Icon=missing' >"${dest}/entries/applications/app.desktop"
		;;
	success | signal)
		mkdir -p "${dest}/entries/share/icons"
		printf '%s\n' '[Desktop Entry]' 'Icon=present' >"${dest}/entries/applications/app.desktop"
		touch "${dest}/entries/share/icons/present.png"
		;;
	esac
	printf 'mount %s\n' "${dest}" >>"${LAYER_ICON_TEST_LOG}"
	if [ "${LAYER_ICON_TEST_CASE}" = signal ]; then
		kill -TERM "${PPID}"
	fi
	;;
umount)
	if [ "${1:-}" = -- ]; then
		shift
	fi
	printf 'umount %s\n' "$1" >>"${LAYER_ICON_TEST_LOG}"
	/usr/bin/find "$1" -mindepth 1 -delete
	;;
esac
EOF
chmod +x "${fake_bin}/fake-tool"
ln -s fake-tool "${fake_bin}/hexdump"
ln -s fake-tool "${fake_bin}/erofsfuse"
ln -s fake-tool "${fake_bin}/umount"

run_case() {
	case_name=$1
	expected_status=$2
	: >"${log_file}"

	set +e
	LAYER_ICON_TEST_CASE="${case_name}" LAYER_ICON_TEST_LOG="${log_file}" \
		PATH="${fake_bin}:${PATH}" bash "${checker}" "${layer_file}" >/dev/null 2>&1
	actual_status=$?
	set -e

	if [ "${actual_status}" -ne "${expected_status}" ]; then
		echo "${case_name}: expected status ${expected_status}, got ${actual_status}" >&2
		return 1
	fi
	mount_point=$(sed -n 's/^mount //p' "${log_file}")
	if ! grep -Fx "umount ${mount_point}" "${log_file}" >/dev/null; then
		echo "${case_name}: mount was not unmounted" >&2
		return 1
	fi
	if [ -e "${mount_point}" ]; then
		echo "${case_name}: temporary mount directory still exists" >&2
		return 1
	fi
}

run_case no-icon 0
run_case fail 255
run_case success 0
run_case signal 143

echo "layer-icon-checker cleanup tests passed"
