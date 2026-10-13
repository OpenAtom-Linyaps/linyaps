#!/usr/bin/env bash

# 该工具用于检查.layer文件中，图标是否有效
# 退出状态
#   0：
#       图标检查通过，在layer文件内的entries/share/icons路径下存在Icon指定的图标文件；
#       该layer文件中的desktop里未定义Icon字段；
#   255：
#       环境不满足，没有hexdump/erofsfuse等工具；
#       非layer类型文件；
#       layer文件损坏，无法获取到info.json大小；
#       layer文件挂载失败；
#       输入参数非普通文件或者该文件不存在；
#       图标检查未通过，在entries/share/icons下不存在Icon指定的图标文件；
#
# TIPS: 该工具的检查函数iconCheck()为弱检查，未完全按xdg desktop specification标准匹配图标
#       能准确处理icon文件不存在的情况
#       对于图标文件是否在有效路径，以及图标格式是否合法等情况无法处理

# 清理约定：
#   正常结束、出错返回、收到 HUP/INT/QUIT/TERM 信号都必须释放挂载。
#   旧实现只在成功路径末尾做 `umount && rm`，其余路径提前 return/exit 时，
#   /tmp/linglong-layer-* 和 erofsfuse 挂载点会留在原地，需要人工清理。
#   现在清理统一收敛到下面这个 EXIT trap：mktemp 之后登记 layerMountDir，
#   挂载成功后置 layerMounted=1，任何一条退出路径都执行同一份清理逻辑；
#   卸载失败时保留挂载目录，避免删掉仍在使用的挂载点。
#   被信号中断时退出码是 128+signo（SIGINT 为 130，SIGTERM 为 143），清理照常执行。
#   回归测试：tools/tests/layer-icon-checker.test.sh

# 当前挂载的临时目录。cleanup() 只看这两个变量，所有退出路径都通过它们
# 汇报状态，而不是各自写一份清理代码。
layerMountDir=""
layerMounted=0

# 卸载 erofsfuse 挂载点。
#
# erofsfuse 挂出来的是 FUSE 文件系统：root 下直接 `umount` 可以卸载，普通
# 用户调用 `umount` 常常因缺少 CAP_SYS_ADMIN 失败，需要改用 fusermount。
# 不同发行版装的是 fuse2 的 fusermount 或 fuse3 的 fusermount3，因此逐个
# 尝试；返回 0 表示挂载点确实已经卸载。
releaseMount() {
	local mountDir=$1

	if umount "${mountDir}" >/dev/null 2>&1; then
		return 0
	fi
	if command -v fusermount >/dev/null 2>&1 && fusermount -u "${mountDir}" >/dev/null 2>&1; then
		return 0
	fi
	if command -v fusermount3 >/dev/null 2>&1 && fusermount3 -u "${mountDir}" >/dev/null 2>&1; then
		return 0
	fi
	return 1
}

cleanup() {
	# 先记下原始退出码：下面每条命令都会改写 $?。
	local status=$?

	# 摘掉所有 trap，避免清理过程中再收到信号而递归进入 cleanup。
	trap - EXIT HUP INT QUIT TERM

	if [ "${layerMounted}" -eq 1 ]; then
		if releaseMount "${layerMountDir}"; then
			layerMounted=0
		else
			# 卸载失败时保留目录：文件系统还挂在这里，删掉目录只会让挂载点
			# 失去路径（内核里依然占用资源，且再也无法按路径卸载），留给
			# 使用者手动处理。
			echo "Failed to unmount ${layerMountDir}, keep the mount point for manual cleanup." >&2
			exit "${status}"
		fi
	fi

	if [ -n "${layerMountDir}" ]; then
		rm -rf -- "${layerMountDir}"
	fi

	exit "${status}"
}

# 信号只把退出码换算成 128+signo，清理统一交给 EXIT trap。
onSignal() {
	exit $((128 + $1))
}

# 必须在产生任何副作用（mktemp、erofsfuse）之前调用。
setupCleanup() {
	trap 'onSignal 1' HUP
	trap 'onSignal 2' INT
	trap 'onSignal 3' QUIT
	trap 'onSignal 15' TERM
	trap cleanup EXIT
}

prepareEnv() {
	if ! command -v hexdump >/dev/null 2>&1 || ! command -v erofsfuse >/dev/null 2>&1; then
		echo "This tool needs 'hexdump' and 'erofsfuse', try to install them with:"
		echo "'sudo apt install bsdextrautils erofsfuse'"
		return 255
	fi
}

getLayerOffset() {
	layerPath=$1
	layerHeader="<<< deepin linglong layer archive >>>"

	declare -i layerHeaderLength=40
	declare -i infoLength=4
	declare -i infoSize offset

	fileHeader=$(hexdump -n ${layerHeaderLength} -e '1/40 "%s\n"' ${layerPath})
	if [ "${fileHeader}" != "${layerHeader}" ]; then
		echo "${layerPath} is not a layer file, exit checking process ..."
		return 255
	fi

	infoSize=$(hexdump -n ${infoLength} -s ${layerHeaderLength} -e '1/4 "%u\n"' ${layerPath})
	# hexdump 在文件被截断时什么都不输出，此时 ${infoSize} 是空串；原来的
	# `[ ! ${infoSize} -gt 0 ]` 会报 "integer expression expected" 且判定为假，
	# 于是带着空 offset 继续去挂载。这里显式先判断空串。
	if [ -z "${infoSize}" ] || [ "${infoSize}" -le 0 ]; then
		echo "broken layer file, exit checking process ..."
		return 255
	fi

	offset=${layerHeaderLength}+${infoLength}+${infoSize}
	echo ${offset}
}

getDesktopIcons() {
	path=$1
	declare desktopIcons

	desktopList=$(find $path -name "*.desktop")
	for desktop in ${desktopList}; do
		desktopIcons+=$(sed -n 's/^Icon=\(.*\)/\1/p' ${desktop})
		desktopIcons+="\n"
	done
	echo -e "${desktopIcons}"
}

formatPrintIconPath() {
	IFS=$'\n'
	for text in $2; do
		echo "	$1 --> $text"
	done
}

formatPrint() {
	IFS=$'\n'
	for text in $1; do
		echo "	$text"
	done
}

iconCheck() {
	path=$1
	iconPath=${path}/entries/share/icons
	icons=$2

	echo "LayerDataPath: ${path}"
	echo "IconName:"
	formatPrint "${icons}"
	echo "IconPath:"

	for icon in ${icons}; do
		result=$(find ${iconPath} -name "${icon}*")
		if [ "${result}" == "" ]; then
			formatPrintIconPath "${icon}" "not found"
			return 255
		else
			formatPrintIconPath "${icon}" "${result}"
		fi
	done
}

main() {
	# 先装清理钩子：下面每一步都可能提前返回或收到信号，钩子必须早于
	# 任何副作用（mktemp、erofsfuse）就位。
	setupCleanup

	RED='\033[0;31m'
	YELLOW='\033[1;33m'
	GREEN='\033[0;32m'
	NC='\033[0m' # No Color

	declare arg1="$1"
	if [[ -z ${arg1} ]]; then
		echo "Usage:"
		echo "	./icon-check.sh path"
		return 0
	fi

	if [ ! -f ${arg1} ]; then
		echo "${arg1} is not a regular file or not exists."
		return 255
	fi

	# env check
	if ! prepareEnv; then
		return 255
	fi

	offset=$(getLayerOffset "${arg1}")
	# getLayerOffset 运行在命令替换的子 shell 里，函数内部的 return/exit 只结束
	# 那个子 shell，状态必须由调用方检查；否则非 layer 文件会带着空 offset 继续
	# 走到 erofsfuse，最后只留下一句 Extract layer failed。
	if [ $? -ne 0 ]; then
		return 255
	fi

	# mount data from layer
	dest=$(mktemp --tmpdir=/tmp -d linglong-layer-XXXXXXXX)
	if [ -z "${dest}" ]; then
		echo "Create temporary mount point failed!"
		return 255
	fi
	# 登记临时目录：从这一刻起，任何退出路径都会删掉它。
	layerMountDir="${dest}"
	erofsfuse --offset=${offset} ${arg1} ${dest} >/dev/null 2>&1
	if [ ! $? -eq 0 ]; then
		echo "Extract layer failed!"
		return 255
	fi
	# 只有挂载成功才置位：失败时 cleanup() 只删空目录，不会去 umount。
	layerMounted=1

	# check desktop file, quit normally if no Icon set
	icons=$(getDesktopIcons ${dest})
	if [ "${icons}" == "" ]; then
		echo "No icon set in desktop file, nothing need to check ..."
		return 0
	fi

	# check whether icon exists
	iconCheck "${dest}" "${icons}"
	if [ ! $? -eq 0 ]; then
		echo "---------------------------------------------------"
		echo -e "Icon check ${RED}failed${NC} ..."
		return 255
	fi
	echo "--------------------------------------------------"
	echo -e "Icon check ${YELLOW}passed${NC} ..."

	# 成功路径也不再单独清理，同样交给 EXIT trap，保证成功、失败、信号
	# 三条路径共用同一份清理逻辑。
	return 0
}

main "$@"
