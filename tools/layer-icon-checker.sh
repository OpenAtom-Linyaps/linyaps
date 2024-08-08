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

prepareEnv() {
	if ! command -v hexdump >/dev/null 2>&1 || ! command -v erofsfuse >/dev/null 2>&1; then
		echo "This tool needs 'hexdump' and 'erofsfuse', try to install them with:"
		echo "'sudo apt install bsdextrautils erofsfuse'"
		exit 255
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
		exit 255
	fi

	infoSize=$(hexdump -n ${infoLength} -s ${layerHeaderLength} -e '1/4 "%u\n"' ${layerPath})
	if [ ! ${infoSize} -gt 0 ]; then
		echo "broken layer file, exit checking process ..."
		exit 255
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
	prepareEnv

	offset=$(getLayerOffset "${arg1}")

	# mount data from layer
	dest=$(mktemp --tmpdir=/tmp -d linglong-layer-XXXXXXXX)
	erofsfuse --offset=${offset} ${arg1} ${dest} >/dev/null 2>&1
	if [ ! $? -eq 0 ]; then
		echo "Extract layer failed!"
		return 255
	fi

	# check desktop file, quit normaly if no Icon set
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

	# clean data
	umount ${dest} && rm ${dest} -rf

	return 0
}

main "$@"
