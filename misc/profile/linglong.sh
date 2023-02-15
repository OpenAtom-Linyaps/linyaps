#set XDG_DATA_DIRS to include linglong installations

LINGLONG_ROOT="/var/lib/linglong"
VERSION=0

if [ -f /etc/os-release ]; then
	VERSION=$(sed -ne '/^VERSION_ID=.*$/P' /etc/os-release | awk -F = '{print $2}' | sed -e 's/"//g' | awk -F . '{print $1}')
	if [ -z "${VERSION}" ]; then
		VERSION=0
	fi
	if [ ${VERSION} -eq 20 ]; then
		LINGLONG_ROOT="/data/linglong"
	elif [ ${VERSION} -eq 23 ]; then
		LINGLONG_ROOT="/persistent/linglong"
	fi
fi

SYS_LINGLONG_INSTALLATIONS="${LINGLONG_ROOT}/entries/share"
USR_LINGLONG_INSTALLATIONS="${XDG_DATA_HOME:-$HOME/.local/share}/linglong/entries/share"

case ":${XDG_DATA_DIRS}:" in
*":${USR_LINGLONG_INSTALLATIONS}:"*) ;;

*)
	export XDG_DATA_DIRS
	XDG_DATA_DIRS="${USR_LINGLONG_INSTALLATIONS}:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
	export LINGLONG_ROOT
	;;
esac

case ":${XDG_DATA_DIRS}:" in
*":${SYS_LINGLONG_INSTALLATIONS}:"*) ;;

*)
	export XDG_DATA_DIRS
	XDG_DATA_DIRS="${SYS_LINGLONG_INSTALLATIONS}:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
	export LINGLONG_ROOT
	;;
esac
