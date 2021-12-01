#set XDG_DATA_DIRS to include linglong installations

SYS_LINGLONG_INSTALLATIONS="/deepin/linglong/entries/share"
USR_LINGLONG_INSTALLATIONS="${XDG_DATA_HOME:-$HOME/.local/share}/linglong/entries/share"

case ":${XDG_DATA_DIRS}:" in
	*":${USR_LINGLONG_INSTALLATIONS}:"*)
		;;
	*)
		export XDG_DATA_DIRS
		XDG_DATA_DIRS="${USR_LINGLONG_INSTALLATIONS}:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
		;;
esac

case ":${XDG_DATA_DIRS}:" in
	*":${SYS_LINGLONG_INSTALLATIONS}:"*)
		;;
	*)
		export XDG_DATA_DIRS
		XDG_DATA_DIRS="${SYS_LINGLONG_INSTALLATIONS}:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
		;;
esac