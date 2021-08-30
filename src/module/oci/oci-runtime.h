/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <sys/mount.h>
#include "module/util/json.h"
#include "module/util/common.h"

namespace linglong {

#undef linux

struct Root {
    std::string path;
    bool readonly;
};

inline void from_json(const nlohmann::json &j, Root &o)
{
    o.path = j.at("path").get<std::string>();
    o.readonly = j.value("readonly", false);
}

inline void to_json(nlohmann::json &j, const Root &o)
{
    j["path"] = o.path;
    j["readonly"] = o.readonly;
}

struct Process {
    std::vector<std::string> args;
    std::vector<std::string> env;
    std::string cwd;
};

inline void from_json(const nlohmann::json &j, Process &o)
{
    o.args = j.at("args").get<std::vector<std::string>>();
    o.env = j.at("env").get<std::vector<std::string>>();
    o.cwd = j.at("cwd").get<std::string>();
}

inline void to_json(nlohmann::json &j, const Process &o)
{
    j["args"] = o.args;
    j["env"] = o.env;
    j["cwd"] = o.cwd;
}

struct Mount {
    enum Type {
        Unknow,
        Bind,
        Proc,
        Sysfs,
        Devpts,
        Mqueue,
        Tmpfs,
        Cgroup,
    };
    std::string destination;
    std::string type;
    std::string source;
    util::str_vec options;

    Type fsType;
    uint32_t flags = 0u;
};

inline void from_json(const nlohmann::json &j, Mount &o)
{
    static std::map<std::string, Mount::Type> fsTypes = {
        {"bind", Mount::Bind},
        {"proc", Mount::Proc},
        {"devpts", Mount::Devpts},
        {"mqueue", Mount::Mqueue},
        {"tmpfs", Mount::Tmpfs},
        {"cgroup", Mount::Cgroup},
        {"sysfs", Mount::Sysfs},
    };

    static std::map<std::string, uint32_t> optionFlags = {
        {"nosuid", MS_NOSUID},
        {"strictatime", MS_STRICTATIME},
        {"noexec", MS_NOEXEC},
        {"nodev", MS_NODEV},
        {"relatime", MS_RELATIME},
        {"ro", MS_RDONLY},
    };

    o.destination = j.at("destination").get<std::string>();
    o.type = j.at("type").get<std::string>();
    o.fsType = fsTypes.find(o.type)->second;
    o.source = j.at("source").get<std::string>();

    o.options = j.value("options", util::str_vec());
    if (o.options.size() > 0) {
        for (auto const &opt : o.options) {
            if (optionFlags.find(opt) != optionFlags.end()) {
                o.flags |= optionFlags.find(opt)->second;
            }
        }
    }
}

inline void to_json(nlohmann::json &j, const Mount &o)
{
    j["destination"] = o.destination;
    j["source"] = o.source;
    j["type"] = o.type;
    j["options"] = o.options;
}

struct Namespace {
    int type;
};

static std::map<std::string, int> namespaceType = {
    {"pid", CLONE_NEWPID},
    {"uts", CLONE_NEWUTS},
    {"mount", CLONE_NEWNS},
    {"cgroup", CLONE_NEWCGROUP},
    {"network", CLONE_NEWNET},
    {"ipc", CLONE_NEWIPC},
    {"user", CLONE_NEWUSER},
};

inline void from_json(const nlohmann::json &j, Namespace &o)
{
    o.type = namespaceType.find(j.at("type"))->second;
}

inline void to_json(nlohmann::json &j, const Namespace &o)
{
    auto matchPair = std::find_if(std::begin(namespaceType), std::end(namespaceType), [&](const std::pair<std::string, int> &pair) {
        return pair.second == o.type;
    });
    j["type"] = matchPair->first;
}

struct Linux {
    std::vector<Namespace> namespaces;
};

inline void from_json(const nlohmann::json &j, Linux &o)
{
    o.namespaces = j.at("namespaces").get<std::vector<Namespace>>();
}

inline void to_json(nlohmann::json &j, const Linux &o)
{
    j["namespaces"] = o.namespaces;
}

/*
    "hooks": {
        "prestart": [
            {
                "path": "/usr/bin/fix-mounts",
                "args": ["fix-mounts", "arg1", "arg2"],
                "env":  [ "key1=value1"]
            },
            {
                "path": "/usr/bin/setup-network"
            }
        ],
        "poststart": [
            {
                "path": "/usr/bin/notify-start",
                "timeout": 5
            }
        ],
        "poststop": [
            {
                "path": "/usr/sbin/cleanup.sh",
                "args": ["cleanup.sh", "-f"]
            }
        ]
    }
 */

struct Hook {
    std::string path;
    util::str_vec args;
    std::vector<std::string> env;
};

inline void from_json(const nlohmann::json &j, Hook &o)
{
    o.path = j.at("path").get<std::string>();
    o.args = j.at("args").get<util::str_vec>();
    o.env = j.at("env").get<std::vector<std::string>>();
}

inline void to_json(nlohmann::json &j, const Hook &o)
{
    j["path"] = o.path;
    j["args"] = o.args;
    j["env"] = o.env;
}

struct Hooks {
    std::vector<Hook> prestart;
    std::vector<Hook> poststart;
    std::vector<Hook> poststop;
};

inline void from_json(const nlohmann::json &j, Hooks &o)
{
    o.prestart = j.at("prestart").get<std::vector<Hook>>();
    o.poststart = j.at("poststart").get<std::vector<Hook>>();
    o.poststop = j.at("poststop").get<std::vector<Hook>>();
}

inline void to_json(nlohmann::json &j, const Hooks &o)
{
    j["poststop"] = o.poststop;
    j["poststart"] = o.poststart;
    j["prestart"] = o.prestart;
}

struct Runtime {
    std::string version;
    Root root;
    Process process;
    std::string hostname;
    std::vector<Mount> mounts;
    Linux linux;
    Hooks hooks;
};

inline void from_json(const nlohmann::json &j, Runtime &o)
{
    o.version = j.at("ociVersion").get<std::string>();
    o.hostname = j.at("hostname").get<std::string>();
    o.process = j.at("process");
    o.mounts = j.at("mounts").get<std::vector<Mount>>();
    o.linux = j.at("linux");
    o.root = j.at("root");
    //    o.hooks = j.value("hooks", Hooks());
}

inline void to_json(nlohmann::json &j, const Runtime &o)
{
    j["ociVersion"] = o.version;
    j["hostname"] = o.hostname;
    j["process"] = o.process;
    j["mounts"] = o.mounts;
    j["linux"] = o.linux;
    j["root"] = o.root;
    //    j["hooks"] = o.hooks;
}

inline static Runtime fromFile(const std::string &filepath)
{
    return util::json::fromFile(filepath).get<Runtime>();
}

inline static Runtime fromString(const std::string &content)
{
    return util::json::fromByteArray(content).get<Runtime>();
}

} // namespace linglong
