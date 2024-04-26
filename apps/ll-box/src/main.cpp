/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "container/container.h"
#include "container/helper.h"
#include "util/logger.h"
#include "util/message_reader.h"
#include "util/oci_runtime.h"

#include <argp.h>

#include <csignal>
#include <filesystem>
#include <iostream>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

constexpr auto cgroup_manager_option = 1111;
constexpr auto config_option = 3333;

std::optional<std::string> command;
std::optional<std::string> container;
std::optional<std::string> signal;

std::string bundle = std::filesystem::current_path();
std::string config = "config.json";

std::optional<std::vector<std::string>> commands;

int parse_opt(int key, char *arg, struct argp_state *state)
{
    switch (key) {
    case cgroup_manager_option:

        if (command) {
            argp_failure(state, 1, 0, "global option must be specified before the command");
            return -1;
        }

        if (strcmp(arg, "disabled") == 0) {
            return 0;
        }

        argp_failure(state, 1, 0, "invalid cgroup manager %s", arg);
        return -1;

    case 'b':

        bundle = arg;
        return 0;

    case config_option:

        config = arg;
        return 0;

    case 'f':
        return 0;

    case ARGP_KEY_ARG:

        if (!command) {
            command = arg;
            return 0;
        }

        if (command == "run") {
            if (!container) {
                container = arg;
                return 0;
            }

            argp_failure(state, 1, 0, "command %s takes only one argument", command->c_str());
            return -1;
        }

        if (command == "exec") {
            if (!container) {
                container = arg;
                return 0;
            }

            if (!commands) {
                commands = std::vector<std::string>{ arg };
                return 0;
            }

            commands->push_back(arg);
            return 0;
        }

        if (command == "kill") {
            if (!container) {
                container = arg;
                return 0;
            }

            if (!signal) {
                signal = arg;
                return 0;
            }

            argp_failure(state, 1, 0, "command %s takes only two argument", command->c_str());
            return -1;
        }

        return -1;

    case ARGP_KEY_END:

        if (!command) {
            argp_failure(state, 1, 0, "command is required");
            return -1;
        }

        if (command != "list" && !container) {
            argp_failure(state, 1, 0, "container is required");
            return -1;
        }

        if (command == "exec" && !commands) {
            argp_failure(state, 1, 0, "commands is required");
            return -1;
        }

        if (command == "kill" && !signal) {
            argp_failure(state, 1, 0, "signal is required");
            return -1;
        }

        return 0;
    default:
        return ARGP_ERR_UNKNOWN;
    }
}

int list() noexcept
{
    auto containers = linglong::readAllContainerJson();
    std::cout << containers.dump() << std::endl;
    return 0;
}

int exec() noexcept
{
    logErr() << "Not implemented yet";
    return -1;
}

int run() noexcept
try {
    auto configFile = std::ifstream(bundle + "/" + config);
    if (!configFile.is_open()) {
        logErr() << "failed to open config" << bundle + "/" + config;
        return -1;
    }

    auto json = nlohmann::json::parse(configFile);
    auto runtime = json.get<linglong::Runtime>();

    linglong::Container c(bundle, *container, runtime);
    return c.Start();
} catch (const std::exception &e) {
    logErr() << "run failed:" << e.what();
    return -1;
}

int kill() noexcept
{
    auto containers = linglong::readAllContainerJson();
    auto id = container.value();
    for (auto container : containers) {
        if (container["id"] != id) {
            continue;
        }

        auto pid = container["pid"].get<pid_t>();

        // FIXME: parse signal
        return ::kill(pid, SIGTERM);
    }

    return -1;
}

} // namespace

int main(int argc, char **argv)
{

    argp_option options[] = {
        {
          .name = nullptr,
          .key = 0,
          .arg = nullptr,
          .flags = 0,
          .doc = "Global options",
          .group = 1,
        },
        {
          .name = "cgroup-manager",
          .key = cgroup_manager_option,
          .arg = "MODE",
          .flags = 0,
          .doc = "allowed values: disabled",
          .group = 1,
        },
        {
          .name = nullptr,
          .key = 0,
          .arg = nullptr,
          .flags = 0,
          .doc = "Run options",
          .group = 2,
        },
        {
          .name = "bundle",
          .key = 'b',
          .arg = "DIR",
          .flags = 0,
          .doc = "Path to the root of the bundle dir (default \".\")",
          .group = 2,
        },
        {
          .name = "config",
          .key = config_option,
          .arg = "FILE",
          .flags = 0,
          .doc = "Override the configuration file to use. The default value is config.json",
          .group = 2,
        },
        {
          .name = nullptr,
          .key = 0,
          .arg = nullptr,
          .flags = 0,
          .doc = "List options",
          .group = 3,
        },
        {
          .name = "format",
          .key = 'f',
          .arg = "FORMAT",
          .flags = 0,
          .doc = "Output format (default \"json\")",
          .group = 3,
        },
        { nullptr },
    };

    argp argp = {
        .options = options,
        .parser = parse_opt,
        .args_doc = R"(list -f json
run <CONTAINER>
exec <CONTAINER> <CMD>
kill <CONTAINER> <SIGNAL>)",
    };

    if (argp_parse(&argp, argc, argv, ARGP_NO_EXIT, nullptr, nullptr) != 0) {
        return -1;
    }

    if (command == "list") {
        return list();
    }

    if (command == "run") {
        return run();
    }

    if (command == "exec") {
        return exec();
    }

    if (command == "kill") {
        return kill();
    }

    return -1;
}
