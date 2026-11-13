#!/usr/bin/env bash
#
# layer-icon-checker.sh 清理路径的回归测试。
#
# tools/layer-icon-checker.sh 通过 erofsfuse 把 .layer 文件挂到
# /tmp/linglong-layer-*，然后在成功路径末尾卸载并删除临时目录。无 Icon
# 字段、图标缺失、挂载失败或收到退出信号时函数会提前返回，挂载点和临时
# 目录就留在机器上，需要人工清理。
#
# 本用例用桩命令替换 hexdump/erofsfuse/umount/fusermount/find，不真的挂载
# 任何文件系统，所以既不需要 root 也不需要安装 erofsfuse：
#
#     bash tools/tests/layer-icon-checker.test.sh
#
# 退出码 0 表示全部用例通过，1 表示有用例失败。

set -u

# 显式收敛 PATH：从 Windows 的 PowerShell 里启动 Git Bash 时，PATH 里可能混进
# Windows 自带的 find.exe，桩命令和被测脚本都会因此拿到错误的 find。
PATH="/usr/bin:/bin"
export PATH

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
CHECKER="${SCRIPT_DIR}/../layer-icon-checker.sh"
LAYER_HEADER="<<< deepin linglong layer archive >>>"

REAL_FIND=$(command -v find)
REAL_SLEEP=$(command -v sleep)
REAL_KILL=$(command -v kill)

if [ ! -f "${CHECKER}" ]; then
	echo "cannot find ${CHECKER}" >&2
	exit 1
fi

TEST_ROOT=$(mktemp -d)
STUB_BIN="${TEST_ROOT}/bin"
LOG_DIR="${TEST_ROOT}/logs"
mkdir -p "${STUB_BIN}" "${LOG_DIR}"
export STUB_LOG="${LOG_DIR}"

# 记录脚本启动前就已经存在的挂载目录，收尾时只删本次新增的那些，
# 避免误删同机上另一次运行的挂载点。
mount_dirs() {
	(
		shopt -s nullglob
		printf '%s\n' /tmp/linglong-layer-*
	) | sort
}

INITIAL_MOUNTS="${TEST_ROOT}/initial-mounts.txt"
mount_dirs > "${INITIAL_MOUNTS}"

remove_new_mount_dirs() {
	local dir
	while IFS= read -r dir; do
		[ -n "${dir}" ] || continue
		if ! grep -qxF "${dir}" "${INITIAL_MOUNTS}"; then
			rm -rf -- "${dir}"
		fi
	done < <(mount_dirs)
}

trap 'remove_new_mount_dirs; rm -rf -- "${TEST_ROOT}"' EXIT

LAYER_FILE="${TEST_ROOT}/fake.layer"
printf 'fake layer payload\n' > "${LAYER_FILE}"

PASSED=0
FAILED=0

check() {
	if [ "$2" = "$3" ]; then
		PASSED=$((PASSED + 1))
		printf 'ok   - %s\n' "$1"
	else
		FAILED=$((FAILED + 1))
		printf 'FAIL - %s\n       expected [%s]\n       actual   [%s]\n' "$1" "$2" "$3"
	fi
}

# 统计某条桩命令被调用了多少次；calls.log 由桩自己追加。
count_calls() {
	grep -c "^$1 " "${LOG_DIR}/calls.log" 2>/dev/null || true
}

# ── 桩命令 ──
#
# 每个桩先把调用记录写进 ${STUB_LOG}/calls.log，再表现成“挂载成功/失败”
# 或“卸载成功/失败”。行为由 STUB_* 环境变量控制，用例通过 reset_case()
# 设定默认值。

cat > "${STUB_BIN}/hexdump" <<EOF
#!/usr/bin/env bash
printf 'hexdump %s\n' "\$*" >> "\${STUB_LOG}/calls.log"
size=""
while [ \$# -gt 0 ]; do
	case "\$1" in
		-s)
			size=\$2
			shift 2
			;;
		*)
			shift
			;;
	esac
done
# 带 -s 的那次调用读的是 info.json 大小，其余是 layer 文件头。
if [ -n "\${size}" ]; then
	printf '%s\n' "\${STUB_INFO_SIZE}"
else
	printf '%s\n' "\${STUB_HEXDUMP_HEADER}"
fi
EOF

cat > "${STUB_BIN}/erofsfuse" <<EOF
#!/usr/bin/env bash
printf 'erofsfuse %s\n' "\$*" >> "\${STUB_LOG}/calls.log"
# 最后一个参数是挂载目录，桩在这里造出 layer 解包后的目录结构。
dest=\${!#}
mkdir -p "\${dest}/entries/share/icons"
if [ -n "\${STUB_DESKTOP:-}" ]; then
	printf '%s\n' "\${STUB_DESKTOP}" > "\${dest}/app.desktop"
fi
if [ -n "\${STUB_ICON_REL:-}" ]; then
	: > "\${dest}/entries/share/icons/\${STUB_ICON_REL}"
fi
exit "\${STUB_EROFSFUSE_RC:-0}"
EOF

cat > "${STUB_BIN}/umount" <<EOF
#!/usr/bin/env bash
printf 'umount %s\n' "\$*" >> "\${STUB_LOG}/calls.log"
exit "\${STUB_UMOUNT_RC:-0}"
EOF

cat > "${STUB_BIN}/fusermount" <<EOF
#!/usr/bin/env bash
printf 'fusermount %s\n' "\$*" >> "\${STUB_LOG}/calls.log"
exit "\${STUB_FUSERMOUNT_RC:-1}"
EOF

cat > "${STUB_BIN}/fusermount3" <<EOF
#!/usr/bin/env bash
printf 'fusermount3 %s\n' "\$*" >> "\${STUB_LOG}/calls.log"
exit "\${STUB_FUSERMOUNT3_RC:-1}"
EOF

# find 桩只用来制造“正在检查图标”的时间窗，好让信号落在挂载建立之后；
# 它把真正的查找转交给系统 find。
cat > "${STUB_BIN}/find" <<EOF
#!/usr/bin/env bash
printf 'find %s\n' "\$*" >> "\${STUB_LOG}/calls.log"
if [ -n "\${STUB_FIND_SLEEP:-}" ] && [ ! -e "\${STUB_LOG}/find.sleep.done" ]; then
	: > "\${STUB_LOG}/find.sleep.done"
	"${REAL_SLEEP}" "\${STUB_FIND_SLEEP}"
fi
exec "${REAL_FIND}" "\$@"
EOF

chmod +x "${STUB_BIN}/"*

reset_case() {
	: > "${LOG_DIR}/calls.log"
	rm -f "${LOG_DIR}/find.sleep.done"
	STUB_HEXDUMP_HEADER="${LAYER_HEADER}"
	STUB_INFO_SIZE=64
	STUB_EROFSFUSE_RC=0
	STUB_UMOUNT_RC=0
	STUB_FUSERMOUNT_RC=1
	STUB_FUSERMOUNT3_RC=1
	STUB_DESKTOP=""
	STUB_ICON_REL=""
	STUB_FIND_SLEEP=""
	export STUB_HEXDUMP_HEADER STUB_INFO_SIZE STUB_EROFSFUSE_RC STUB_UMOUNT_RC \
		STUB_FUSERMOUNT_RC STUB_FUSERMOUNT3_RC STUB_DESKTOP STUB_ICON_REL \
		STUB_FIND_SLEEP
}

# 用桩 PATH 跑一次被测脚本，输出留在 checker.out 里。
run_checker() {
	PATH="${STUB_BIN}:/usr/bin:/bin" "${CHECKER}" "${LAYER_FILE}" \
		>"${LOG_DIR}/checker.out" 2>&1
}

# 断言本次运行没有留下新的挂载目录。
check_no_leak() {
	check "$1" "" "$(comm -13 "${TEST_ROOT}/before.txt" "${TEST_ROOT}/after.txt")"
}

case_no_icon_field() {
	reset_case
	STUB_DESKTOP="[Desktop Entry]"
	export STUB_DESKTOP
	mount_dirs > "${TEST_ROOT}/before.txt"
	run_checker
	local rc=$?
	mount_dirs > "${TEST_ROOT}/after.txt"

	check "no Icon field: exit status" 0 "${rc}"
	check "no Icon field: layer was mounted" 1 "$(count_calls erofsfuse)"
	check "no Icon field: mount released" 1 "$(count_calls umount)"
	check_no_leak "no Icon field: no leftover mount dir"
}

case_icon_found() {
	reset_case
	STUB_DESKTOP="[Desktop Entry]"$'\n'"Icon=myapp"
	STUB_ICON_REL="myapp.png"
	export STUB_DESKTOP STUB_ICON_REL
	mount_dirs > "${TEST_ROOT}/before.txt"
	run_checker
	local rc=$?
	mount_dirs > "${TEST_ROOT}/after.txt"

	check "icon found: exit status" 0 "${rc}"
	check "icon found: mount released" 1 "$(count_calls umount)"
	check_no_leak "icon found: no leftover mount dir"
}

case_icon_missing() {
	reset_case
	STUB_DESKTOP="[Desktop Entry]"$'\n'"Icon=missing"
	export STUB_DESKTOP
	mount_dirs > "${TEST_ROOT}/before.txt"
	run_checker
	local rc=$?
	mount_dirs > "${TEST_ROOT}/after.txt"

	check "icon missing: exit status" 255 "${rc}"
	check "icon missing: mount released" 1 "$(count_calls umount)"
	check_no_leak "icon missing: no leftover mount dir"
}

case_mount_failed() {
	reset_case
	STUB_DESKTOP="[Desktop Entry]"
	STUB_EROFSFUSE_RC=1
	export STUB_DESKTOP STUB_EROFSFUSE_RC
	mount_dirs > "${TEST_ROOT}/before.txt"
	run_checker
	local rc=$?
	mount_dirs > "${TEST_ROOT}/after.txt"

	check "mount failure: exit status" 255 "${rc}"
	check "mount failure: nothing to unmount" 0 "$(count_calls umount)"
	check_no_leak "mount failure: no leftover mount dir"
}

case_umount_falls_back() {
	reset_case
	STUB_DESKTOP="[Desktop Entry]"
	STUB_UMOUNT_RC=1
	STUB_FUSERMOUNT_RC=0
	export STUB_DESKTOP STUB_UMOUNT_RC STUB_FUSERMOUNT_RC
	mount_dirs > "${TEST_ROOT}/before.txt"
	run_checker
	local rc=$?
	mount_dirs > "${TEST_ROOT}/after.txt"

	check "umount fallback: exit status" 0 "${rc}"
	check "umount fallback: umount tried first" 1 "$(count_calls umount)"
	check "umount fallback: fusermount used" 1 "$(count_calls fusermount)"
	check_no_leak "umount fallback: no leftover mount dir"
}

case_unmount_failure_keeps_dir() {
	reset_case
	STUB_DESKTOP="[Desktop Entry]"
	STUB_UMOUNT_RC=1
	STUB_FUSERMOUNT_RC=1
	STUB_FUSERMOUNT3_RC=1
	export STUB_DESKTOP STUB_UMOUNT_RC STUB_FUSERMOUNT_RC STUB_FUSERMOUNT3_RC
	mount_dirs > "${TEST_ROOT}/before.txt"
	run_checker
	local rc=$?
	mount_dirs > "${TEST_ROOT}/after.txt"

	local leaked
	leaked=$(comm -13 "${TEST_ROOT}/before.txt" "${TEST_ROOT}/after.txt")

	check "unmount failure: exit status preserved" 0 "${rc}"
	check "unmount failure: unmount attempted" 1 "$(count_calls umount)"
	check "unmount failure: mount dir kept" 1 "$(printf '%s\n' "${leaked}" | grep -c 'linglong-layer-' || true)"
	check "unmount failure: kept dir still holds the layer data" 1 "$(grep -c 'Desktop Entry' "${leaked}/app.desktop" 2>/dev/null || true)"
	check "unmount failure: warns about the leftover" 1 "$(grep -c 'keep the mount point' "${LOG_DIR}/checker.out" || true)"

	if [ -n "${leaked}" ]; then
		rm -rf -- "${leaked}"
	fi
	mount_dirs > "${TEST_ROOT}/after.txt"
	check_no_leak "unmount failure: test cleaned the leftover"
}

case_not_a_layer() {
	reset_case
	STUB_HEXDUMP_HEADER="this is not a layer archive"
	export STUB_HEXDUMP_HEADER
	mount_dirs > "${TEST_ROOT}/before.txt"
	run_checker
	local rc=$?
	mount_dirs > "${TEST_ROOT}/after.txt"

	check "not a layer: exit status" 255 "${rc}"
	check "not a layer: never mounted" 0 "$(count_calls erofsfuse)"
	check_no_leak "not a layer: no leftover mount dir"
}

case_truncated_layer() {
	reset_case
	STUB_INFO_SIZE=""
	export STUB_INFO_SIZE
	mount_dirs > "${TEST_ROOT}/before.txt"
	run_checker
	local rc=$?
	mount_dirs > "${TEST_ROOT}/after.txt"

	check "truncated layer: exit status" 255 "${rc}"
	check "truncated layer: never mounted" 0 "$(count_calls erofsfuse)"
	check "truncated layer: no shell integer error" "" "$(grep 'integer expression expected' "${LOG_DIR}/checker.out" || true)"
	check_no_leak "truncated layer: no leftover mount dir"
}

# 运行中收到退出信号：清理同样要发生，退出码是 128+signo。
signal_case() {
	local signo=$1
	local expected=$2
	local name=$3

	reset_case
	STUB_DESKTOP="[Desktop Entry]"$'\n'"Icon=myapp"
	STUB_FIND_SLEEP=2
	export STUB_DESKTOP STUB_FIND_SLEEP
	mount_dirs > "${TEST_ROOT}/before.txt"

	# 打开 job control：关闭时 bash 会把后台任务的 SIGINT 设成忽略，
	# 脚本里的 trap INT 会静默失效，测不出真实的 Ctrl-C 行为。
	set -m
	PATH="${STUB_BIN}:/usr/bin:/bin" "${CHECKER}" "${LAYER_FILE}" \
		>"${LOG_DIR}/checker.out" 2>&1 &
	local pid=$!

	# 等 find 桩开始 sleep 再发信号，确保信号落在挂载建立之后。
	local i
	for ((i = 0; i < 400; i++)); do
		[ -e "${LOG_DIR}/find.sleep.done" ] && break
		"${REAL_SLEEP}" 0.05
	done
	"${REAL_KILL}" "-${signo}" "${pid}"
	wait "${pid}"
	local rc=$?
	set +m
	mount_dirs > "${TEST_ROOT}/after.txt"

	check "${name}: exit status" "${expected}" "${rc}"
	check "${name}: mount released" 1 "$(count_calls umount)"
	check_no_leak "${name}: no leftover mount dir"
}

main() {
	case_no_icon_field
	case_icon_found
	case_icon_missing
	case_mount_failed
	case_umount_falls_back
	case_unmount_failure_keeps_dir
	case_not_a_layer
	case_truncated_layer
	signal_case INT 130 SIGINT
	signal_case TERM 143 SIGTERM

	printf '\n%d passed, %d failed\n' "${PASSED}" "${FAILED}"
	[ "${FAILED}" -eq 0 ]
}

main "$@"