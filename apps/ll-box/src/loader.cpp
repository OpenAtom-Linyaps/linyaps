/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "container/container_option.h"
#include "util/logger.h"
#include "util/oci_runtime.h"

#include <list>
#include <regex>

#include <sys/stat.h>
#include <wordexp.h>

//    "lowerParent":
//    "/run/user/<<user_uid>>/linglong/375f5681145f4f4f9ffeb3a67aebd422/.overlayfs/lower_parent",
//     "upper": "/run/user/<<user_uid>>/linglong/375f5681145f4f4f9ffeb3a67aebd422/.overlayfs/upper",
//     "workdir":
//     "/run/user/<<user_uid>>/linglong/375f5681145f4f4f9ffeb3a67aebd422/.overlayfs/workdir",
const std::string kLoadTemplate = R"KLT00(
{
    "annotations": {
        "containerRootPath": "",
        "native": {
            "mounts": [
                {
                    "destination": "/usr",
                    "options": [
                        "ro",
                        "rbind"
                    ],
                    "source": "/usr",
                    "type": "bind"
                },
                {
                    "destination": "/etc",
                    "options": [
                        "ro",
                        "rbind"
                    ],
                    "source": "/etc",
                    "type": "bind"
                },
                {
                    "destination": "/usr/share/locale/",
                    "options": [
                        "ro",
                        "rbind"
                    ],
                    "source": "/usr/share/locale/",
                    "type": "bind"
                }
            ]
    }
    },
	"hooks": null,
	"hostname": "linglong",
	"linux": {
		"gidMappings": [{
			"containerID": 0,
			"hostID": <<user_uid>>,
			"size": 1
		}],
		"uidMappings": [{
			"containerID": 0,
			"hostID": <<user_uid>>,
			"size": 1
		}],
		"namespaces": [
            {
                "type": "pid"
            },
            {
                "type": "mount"
            },
            {
                "type": "uts"
            },
            {
                "type": "user"
            }
		]
	},
	"mounts": [
        {
            "destination": "/sys",
            "options": [
                "nosuid",
                "noexec",
                "nodev"
            ],
            "source": "sysfs",
            "type": "sysfs"
        },
        {
            "destination": "/proc",
            "options": [],
            "source": "proc",
            "type": "proc"
        },
        {
            "destination": "/dev",
            "options": [
                "nosuid",
                "strictatime",
                "mode=0755",
                "size=65536k"
            ],
            "source": "tmpfs",
            "type": "tmpfs"
        },
        {
            "destination": "/dev/pts",
            "options": [
                "nosuid",
                "noexec",
                "newinstance",
                "ptmxmode=0666",
                "mode=0620"
            ],
            "source": "devpts",
            "type": "devpts"
        },
        {
            "destination": "/dev/shm",
            "options": [
                "nosuid",
                "noexec",
                "nodev",
                "mode=1777"
            ],
            "source": "shm",
            "type": "tmpfs"
        },
        {
            "destination": "/dev/mqueue",
            "options": [
                "nosuid",
                "noexec",
                "nodev"
            ],
            "source": "mqueue",
            "type": "mqueue"
        },
        {
            "destination": "/sys/fs/cgroup",
            "options": [
                "nosuid",
                "noexec",
                "nodev",
                "relatime",
                "ro"
            ],
            "source": "cgroup",
            "type": "cgroup"
        },
        {
            "destination": "/dev/dri",
            "options": [
                "rbind"
            ],
            "source": "/dev/dri",
            "type": "bind"
        },
        {
            "destination": "/dev/snd",
            "options": [
                "rbind"
            ],
            "source": "/dev/snd",
            "type": "bind"
        },
        {
            "destination": "/run/user/<<user_uid>>",
            "options": [
                "nodev",
                "nosuid",
                "mode=700"
            ],
            "source": "tmpfs",
            "type": "tmpfs"
        },
        {
            "destination": "/run/user/<<user_uid>>/pulse",
            "options": [
                "rbind"
            ],
            "source": "/run/user/<<user_uid>>/pulse",
            "type": "bind"
        },
        {
            "destination": "/run/udev",
            "options": [
                "rbind"
            ],
            "source": "/run/udev",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>",
            "options": [
                "rbind"
            ],
            "source": "<<home_dir>>/.linglong/<<appid>>/home",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/.linglong/<<appid>>",
            "options": [
                "rbind"
            ],
            "source": "<<home_dir>>/.linglong/<<appid>>",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/.local/share",
            "options": [
                "rbind"
            ],
            "source": "<<home_dir>>/.linglong/<<appid>>/share",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/.config",
            "options": [
                "rbind"
            ],
            "source": "<<home_dir>>/.linglong/<<appid>>/config",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/.cache",
            "options": [
                "rbind"
            ],
            "source": "<<home_dir>>/.linglong/<<appid>>/cache",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/.deepinwine",
            "options": [
                "rbind"
            ],
            "source": "<<home_dir>>/.deepinwine",
            "type": "bind"
        },
        {
            "destination": "/run/user/<<user_uid>>/dconf",
            "options": [
                "rbind"
            ],
            "source": "/run/user/<<user_uid>>/dconf",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/.linglong/<<appid>>/config/systemd/user",
            "options": [
                "rbind"
            ],
            "source": "<<home_dir>>/.config/systemd/user",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/.config/user-dirs.dirs",
            "options": [
                "rbind"
            ],
            "source": "<<home_dir>>/.config/user-dirs.dirs",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/.linglong/<<appid>>/config/user-dirs.dirs",
            "options": [
                "rbind"
            ],
            "source": "<<home_dir>>/.config/user-dirs.dirs",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/.linglong/<<appid>>/share/fonts",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "<<home_dir>>/.local/share/fonts",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/.linglong/<<appid>>/config/fontconfig",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "<<home_dir>>/.config/fontconfig",
            "type": "bind"
        },
        {
            "destination": "/run/host/appearance/user-fonts",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "<<home_dir>>/.local/share/fonts",
            "type": "bind"
        },
        {
            "destination": "/run/host/appearance/user-fonts-cache",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "<<home_dir>>/.cache/fontconfig",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/.cache/deepin/dde-api",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "<<home_dir>>/.cache/deepin/dde-api",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/.linglong/<<appid>>/cache/deepin/dde-api",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "<<home_dir>>/.cache/deepin/dde-api",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/.linglong/<<appid>>/config/dconf",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "<<home_dir>>/.config/dconf",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/.Xauthority",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "<<home_dir>>/.Xauthority",
            "type": "bind"
        },
        {
            "destination": "/tmp",
            "options": [
                "rbind"
            ],
            "source": "/tmp/linglong/573992118ad045e1a42bb03af87f7984",
            "type": "bind"
        },
        {
            "destination": "/run/host/network/etc/resolv.conf",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "/etc/resolv.conf",
            "type": "bind"
        },
        {
            "destination": "/run/resolvconf",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "/run/resolvconf",
            "type": "bind"
        },
        {
            "destination": "/run/host/appearance/fonts",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "/usr/share/fonts",
            "type": "bind"
        },
        {
            "destination": "/usr/lib/locale/",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "/usr/lib/locale/",
            "type": "bind"
        },
        {
            "destination": "/usr/share/themes",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "/usr/share/themes",
            "type": "bind"
        },
        {
            "destination": "/usr/share/icons",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "/usr/share/icons",
            "type": "bind"
        },
        {
            "destination": "/usr/share/zoneinfo",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "/usr/share/zoneinfo",
            "type": "bind"
        },
        {
            "destination": "/run/host/etc/localtime",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "/etc/localtime",
            "type": "bind"
        },
        {
            "destination": "/run/host/etc/machine-id",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "/etc/machine-id",
            "type": "bind"
        },
        {
            "destination": "/etc/machine-id",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "/etc/machine-id",
            "type": "bind"
        },
        {
            "destination": "/var",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "/var",
            "type": "bind"
        },
        {
            "destination": "/run/host/appearance/fonts-cache",
            "options": [
                "ro",
                "rbind"
            ],
            "source": "/var/cache/fontconfig",
            "type": "bind"
        },
        {
            "destination": "/tmp/.X11-unix",
            "options": [
                "rbind"
            ],
            "source": "/tmp/.X11-unix",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/Desktop",
            "options": [
                "rw",
                "rbind"
            ],
            "source": "<<home_dir>>/Desktop",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/Documents",
            "options": [
                "rw",
                "rbind"
            ],
            "source": "<<home_dir>>/Documents",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/Downloads",
            "options": [
                "rw",
                "rbind"
            ],
            "source": "<<home_dir>>/Downloads",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/Music",
            "options": [
                "rw",
                "rbind"
            ],
            "source": "<<home_dir>>/Music",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/Pictures",
            "options": [
                "rw",
                "rbind"
            ],
            "source": "<<home_dir>>/Pictures",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/Videos",
            "options": [
                "rw",
                "rbind"
            ],
            "source": "<<home_dir>>/Videos",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/.Public",
            "options": [
                "rw",
                "rbind"
            ],
            "source": "<<home_dir>>/.Public",
            "type": "bind"
        },
        {
            "destination": "<<home_dir>>/.Templates",
            "options": [
                "rw",
                "rbind"
            ],
            "source": "<<home_dir>>/.Templates",
            "type": "bind"
        },
        {
            "destination": "/run/app/env",
            "options": [
                "rbind"
            ],
            "source": "/run/user/<<user_uid>>/linglong/375f5681145f4f4f9ffeb3a67aebd422/env",
            "type": "bind"
        },
        {
            "destination": "/run/user/<<user_uid>>//bus",
            "options": [],
            "source": "/run/user/<<user_uid>>//bus",
            "type": "bind"
        },
        {
            "destination": "/run/dbus/system_bus_socket",
            "options": [],
            "source": "/run/dbus/system_bus_socket",
            "type": "bind"
        }
    ],
	"ociVersion": "1.0.1",
	"process": {
		"args": [
			"/bin/bash"
		],
        "cwd":"/",
		"env": [
			"PATH=/runtime/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
			"TERM=xterm",
			"_=/usr/bin/env",
			"PS1=️\\033[48;5;214;38;5;26m${debian_chroot:+($debian_chroot)}\\h ⚛ \\w\\033[0m",
			"QT_PLUGIN_PATH=/usr/lib/plugins",
			"QT_QPA_PLATFORM_PLUGIN_PATH=/usr/lib/plugins/platforms",
			"DISPLAY=<<x_display>>",
			"LANG=zh_CN.UTF-8",
			"LANGUAGE=zh_CN",
			"XDG_SESSION_DESKTOP=deepin",
			"D_DISABLE_RT_SCREEN_SCALE=",
			"XMODIFIERS=@im=<<im_mothod>>",
			"DESKTOP_SESSION=deepin",
			"DEEPIN_WINE_SCALE=2.00",
			"XDG_CURRENT_DESKTOP=Deepin",
			"XIM=<<im_mothod>>",
			"XDG_SESSION_TYPE=tty",
            "XDG_RUNTIME_DIR=/run/user/<<user_uid>>",
            "DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/<<user_uid>>/bus",
			"CLUTTER_IM_MODULE=<<im_mothod>>",
			"QT4_IM_MODULE=<<im_mothod>>",
            "QT_IM_MODULE=<<im_mothod>>",
			"GTK_IM_MODULE=<<im_mothod>>"
		]
	},
	"root": {
		"path": "/run/user/<<user_uid>>/linglong/375f5681145f4f4f9ffeb3a67aebd422/root",
		"readonly": false
	}
}
)KLT00";

using namespace linglong;

#include <dirent.h>

// start container from tmp mount path
linglong::Runtime loadBundle(int argc, char **argv)
{
    if (argc < 4) {
        throw std::runtime_error("invalid load args count");
    }

    auto getenv = [](const char *var) -> const char *const {
        auto env = ::getenv(var);
        return env ? env : "";
    };

    // fix json template string with home dir & im mothod & user uid & appid
    std::string getHomeDir = getenv("HOME");
    if (getHomeDir == "") {
        // set default home dir with deepin user
        getHomeDir = "/home/deepin";
    }
    std::string getCurrentImMethod = getenv("QT_IM_MODULE");
    if (getCurrentImMethod == "") {
        getCurrentImMethod = "fcitx";
    }

    std::string getCurrentUid = std::to_string(getuid());
    if (getCurrentUid == "") {
        getCurrentUid = "1000";
    }

    std::string xDisplay = getenv("DISPLAY");

    std::unordered_map<std::string, std::string> patternReplacements = {
        { "<<home_dir>>", getHomeDir },
        { "<<im_mothod>>", getCurrentImMethod },
        { "<<user_uid>>", getCurrentUid },
        { "<<x_display>>", xDisplay },
        { "<<appid>>", argv[1] }
    };

    std::string replacedTemplate = kLoadTemplate;
    for (const auto &[key, value] : patternReplacements) {
        replacedTemplate =
          std::regex_replace(replacedTemplate, std::regex(key), patternReplacements[key]);
    }

    auto r = linglong::fromString(replacedTemplate);
    std::string id = argv[1];
    std::string bundleRoot = argv[2];
    std::string exec = argv[3];

    // fix appid with user home dir
    std::list<std::string> userHomeDirectorys = { "home/Documents",       "home/Downloads",
                                                  "home/Music",           "home/Pictures",
                                                  "home/Videos",          "home/Public",
                                                  "home/Templates",       "home/Desktop",
                                                  "cache/deepin/dde-api", "config/systemd/user" };

    if (!util::fs::exists(getHomeDir + "/.linglong/" + id)) {
        for (const auto &dir : userHomeDirectorys) {
            util::fs::create_directories(
              util::fs::path(getHomeDir + "/.linglong/" + id + "/" + dir).parent_path(),
              0755);
        }
    }

    {
        Mount m;
        m.source = "tmpfs";
        m.type = "tmpfs";
        m.fsType = Mount::Tmpfs;
        m.data = { "nodev", "nosuid" };
        m.destination = "/opt";
        r.mounts->push_back(m);
    }

    if (util::fs::exists(bundleRoot + "/opt")) {
        std::string source = bundleRoot + "/opt";
        util::str_vec namelist;
        DIR *dir;
        struct dirent *ent;

        if ((dir = opendir(source.c_str())) != nullptr) {
            /* print all the files and directories within directory */
            while ((ent = readdir(dir)) != nullptr) {
                if ("." == std::string(ent->d_name) || ".." == std::string(ent->d_name)) {
                    continue;
                }
                if (util::fs::is_dir(source + "/" + ent->d_name)) {
                    namelist.push_back(ent->d_name);
                }
            }
            closedir(dir);
        }

        Mount m;
        m.type = "bind";
        m.fsType = Mount::Bind;

        for (auto const &name : namelist) {
            m.source = source.append("/") + name;
            m.destination = "/opt/" + name;
            logInf() << m.source << "to" << m.destination << name;
            r.mounts->push_back(m);
        }
    }

    // app files
    if (util::fs::exists(bundleRoot + "/files")) {
        Mount m;
        m.type = "bind";
        m.fsType = Mount::Bind;
        m.source = bundleRoot;
        m.destination = util::format("/opt/apps/%s", id.c_str());
        r.mounts->push_back(m);
    }

    // process runtime
    if (util::fs::exists(bundleRoot + "/lib/extra_files.txt")) {
        std::ifstream extraFiles(bundleRoot + "/lib/extra_files.txt");
        std::string path;
        while (std::getline(extraFiles, path)) {
            Mount m;
            m.type = "rbind";
            m.fsType = Mount::Bind;
            m.source = bundleRoot + "/lib" + path;
            m.destination = path;
            r.mounts->push_back(m);
        }
    }

    // process env
    r.process.env.push_back(util::format("XAUTHORITY=%s", getenv("XAUTHORITY")));
    r.process.env.push_back(util::format("XDG_RUNTIME_DIR=%s", getenv("XDG_RUNTIME_DIR")));
    r.process.env.push_back(
      util::format("DBUS_SESSION_BUS_ADDRESS=%s", getenv("DBUS_SESSION_BUS_ADDRESS")));
    r.process.env.push_back(util::format("HOME=%s", getenv("HOME")));

    // extra env
    r.process.env.push_back(util::format("PATH=%s", getenv("PATH")));
    r.process.env.push_back(util::format("LD_LIBRARY_PATH=%s", getenv("LD_LIBRARY_PATH")));
    r.process.env.push_back(util::format("PYTHONPATH=%s", getenv("PYTHONPATH")));
    r.process.env.push_back(util::format("PYTHONHOME=%s", getenv("PYTHONHOME")));
    r.process.env.push_back(util::format("XDG_DATA_DIRS=%s", getenv("XDG_DATA_DIRS")));
    r.process.env.push_back(util::format("PERLLIB=%s", getenv("PERLLIB")));
    r.process.env.push_back(
      util::format("GSETTINGS_SCHEMA_DIR=%s", getenv("GSETTINGS_SCHEMA_DIR")));

    util::str_vec ldLibraryPath;

    if (getenv("LD_LIBRARY_PATH")) {
        ldLibraryPath.push_back(getenv("LD_LIBRARY_PATH"));
    }
    ldLibraryPath.push_back("/opt/runtime/lib");
    ldLibraryPath.push_back("/opt/runtime/lib/i386-linux-gnu");
    ldLibraryPath.push_back("/opt/runtime/lib/x86_64-linux-gnu");
    r.process.env.push_back(
      util::format("LD_LIBRARY_PATH=%s", util::str_vec_join(ldLibraryPath, ':').c_str()));

    r.process.cwd = getenv("HOME");
    // r.process.args = util::str_vec {exec,"sh"};
    auto split = [](std::string words) -> util::str_vec {
        wordexp_t p;
        auto ret = wordexp(words.c_str(), &p, WRDE_SHOWERR);
        if (ret != 0) {
            std::cerr << "wordexp" << strerror(errno);
            wordfree(&p);
            return {};
        }
        util::str_vec res;
        for (int i = 0; i < (int)p.we_wordc; i++) {
            res.push_back(p.we_wordv[i]);
        }
        wordfree(&p);
        return res;
    };
    r.process.args = split(exec);

    /// run/user/1000/linglong/375f5681145f4f4f9ffeb3a67aebd422/root

    char dirTemplate[255] = {
        0,
    };
    sprintf(dirTemplate, "/run/user/%d/linglong/XXXXXX", getuid());
    util::fs::create_directories(util::fs::path(dirTemplate).parent_path(), 0755);

    char *dirName = mkdtemp(dirTemplate);
    if (dirName == nullptr) {
        throw std::runtime_error("mkdtemp failed");
    }

    auto rootPath = util::fs::path(dirName) / "root";
    util::fs::create_directories(rootPath, 0755);

    r.root.path = rootPath.string();
    r.annotations->container_root_path = dirName;

    // FIXME: workaround id map
    r.linux.uidMappings[0].hostID = getuid();
    r.linux.gidMappings[0].hostID = getgid();

    return r;
}
