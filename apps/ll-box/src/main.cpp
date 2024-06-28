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
#include <sys/wait.h>
#include <unistd.h>

const char *argp_program_bug_address = "https://github.com/linuxdeepin/linglong/issues";

namespace {
struct arg_global
{
    std::string cgroupManager;
    int exitCode{ -1 };
};

struct arg_list
{
    struct arg_global *global{ nullptr };
    std::string format{ "table" };
};

struct arg_run
{
    struct arg_global *global{ nullptr };
    std::string bundle{ std::filesystem::current_path() };
    std::string config{ "config.json" };
};

struct arg_exec
{
    struct arg_global *global{ nullptr };
    std::string uid{ "0" };
    std::string gid{ "0" };
    std::string cwd{ "/" };
};

enum globalOption { OPTION_CGROUP_MANAGER = 1000 };

enum execOption { OPTION_CWD = 1000 };

void containerJsonCleanUp()
{
    auto containers = linglong::readAllContainerJson();

    auto it = containers.begin();
    while (it != containers.end()) {
        auto boxPid = it->value("pid", -1);
        if (boxPid == -1) {
            ++it;
            continue;
        }

        if (kill(boxPid, 0) != 0) {
            auto jsonPath = std::filesystem::path("/run") / "user" / std::to_string(getuid())
              / "linglong" / "box" / (it->value("id", "unknown") + ".json");

            if (!std::filesystem::remove(jsonPath)) {
                logErr() << "remove" << jsonPath << "failed";
            }
            it = containers.erase(it);
        } else {
            ++it;
        }
    }
}

int list(struct arg_list *arg) noexcept
{
    auto containers = linglong::readAllContainerJson();
    if (arg->format == "json") {
        std::cout << containers.dump() << std::endl;
        return 0;
    }

    const auto *sep = "        ";
    const auto *halfSep = "    ";

    std::cout << "ID" << sep << "PID" << sep << "STATUS" << sep << "BUNDLE" << sep << "CREATED"
              << sep << "OWNER" << std::endl;
    for (const auto &container : containers) {
        std::cout << container.value("id", "unknown") << halfSep << container.value("pid", -1)
                  << halfSep << container.value("status", "unknown") << halfSep
                  << container.value("bundle", "unknown") << halfSep
                  << container.value("created", "unknown") << halfSep
                  << container.value("owner", "unknown") << std::endl;
    }

    return 0;
}

int exec(struct arg_exec *arg, int argc, char **argv) noexcept
{
    std::string containerID = argv[0];
    auto containers = linglong::readAllContainerJson();
    auto container = std::find_if(containers.cbegin(),
                                  containers.cend(),
                                  [&containerID](const nlohmann::json &container) {
                                      return container["id"] == containerID;
                                  });
    if (container == containers.cend()) {
        logErr() << "couldn't find container" << containerID;
        return -1;
    }

    auto boxPid = container->value("pid", -1);
    if (boxPid == -1) {
        logErr() << "couldn't get pid of container" << containerID;
        return -1;
    }

    auto boxPidStr = std::to_string(boxPid);
    auto findLastBox = [&boxPidStr]() {
        std::filesystem::path proc{ "/proc" };
        if (!std::filesystem::exists(proc)) {
            logFal() << "/proc doesn't exist.";
        }
        auto boxBin = std::filesystem::read_symlink("/proc/self/exe");
        std::error_code ec;
        while (true) {
            auto children = proc / boxPidStr / "task" / boxPidStr / "children";
            std::ifstream stream{ children };
            if (!stream.is_open()) {
                logErr() << "couldn't open" << children;
                return -1;
            }

            int chPid{ -1 };
            bool childBoxFound{ false };
            while (stream >> chPid) {
                auto exe = proc / std::to_string(chPid) / "exe";
                auto realExe = std::filesystem::read_symlink(exe, ec);
                if (ec) {
                    logErr() << "failed to read symlink" << exe << ":" << ec.message();
                    return -1;
                }

                if (realExe.filename() == boxBin.filename()) {
                    childBoxFound = true;
                    break;
                }
            }

            if (childBoxFound) {
                boxPidStr = std::to_string(chPid);
                continue;
            }

            break;
        }

        return 0;
    };

    if (findLastBox() == -1) {
        return -1;
    }

    auto wdns = linglong::util::format("--wdns=%s", arg->cwd.c_str());
    const char *nsenterArgv[] = { "nsenter", "-t",         boxPidStr.c_str(),        "-U",   "-m",
                                  "-p",      wdns.c_str(), "--preserve-credentials", nullptr };

    int nsenterArgc{ 0 };
    while (nsenterArgv[nsenterArgc] != nullptr) {
        ++nsenterArgc;
    }
    auto newArgc = nsenterArgc + argc + 1; // except argv[0] and add '&'
    const char **newArgv = (const char **)malloc(sizeof(char *) * newArgc);

    if (newArgv == nullptr) {
        logErr() << "malloc error";
        return errno;
    }
    for (int i = 0; i < nsenterArgc; ++i) {
        newArgv[i] = nsenterArgv[i];
    }
    for (int i = 0; i < argc; ++i) {
        newArgv[i + nsenterArgc] = argv[i + 1];
    }

    newArgv[newArgc - 2] = "&";
    newArgv[newArgc - 1] = nullptr;

    for (int i = 0; i < newArgc; ++i) {
        logDbg() << "newArgv[" << i << "]:" << newArgv[i];
    }
    return ::execvp("nsenter", const_cast<char **>(newArgv));
}

int run(struct arg_run *arg, const std::string &container) noexcept
try {
    if (arg->bundle.at(0) != '/') {
        arg->bundle = std::filesystem::current_path() / arg->bundle;
    }

    auto bundleDir = std::filesystem::path(arg->bundle);
    auto configFile = bundleDir / arg->config;
    auto configFileStream = std::ifstream(configFile);
    if (!configFileStream.is_open()) {
        logErr() << "failed to open config" << configFile;
        return -1;
    }

    auto json = nlohmann::json::parse(configFileStream);
    auto runtime = json.get<linglong::Runtime>();

    linglong::Container c(bundleDir, container, runtime);
    return c.Start();
} catch (const std::exception &e) {
    logErr() << "run failed:" << e.what();
    return -1;
}

int kill(const std::string &containerID, const std::string &signal) noexcept
{
    int sig = SIGTERM;
    if (!signal.empty()) {
        if (std::all_of(signal.cbegin(), signal.cend(), ::isdigit)) {
            sig = std::stoi(signal);
        }
        // TODO: parse signal string
    }

    auto containers = linglong::readAllContainerJson();
    for (auto container : containers) {
        if (container["id"] != containerID) {
            continue;
        }

        auto pid = container["pid"].get<pid_t>();
        return ::kill(pid, sig);
    }

    return -1;
}

int parse_list(int key, char *arg, struct argp_state *state)
{
    auto *input = (struct arg_list *)state->input;
    static std::vector<std::string> formatMap = { "json", "table" };

    switch (key) {
    case 'f': {
        std::string val{ arg };
        if (std::find(formatMap.cbegin(), formatMap.cend(), val) == formatMap.cend()) {
            argp_failure(state, -1, EINVAL, "invalid format %s", arg);
        }
        input->format = std::move(val);
    } break;
    default:
        return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

int parse_run(int key, char *arg, struct argp_state *state)
{
    auto *input = (struct arg_run *)state->input;

    switch (key) {
    case 'f': {
        input->config = arg;
    } break;
    case 'b': {
        input->bundle = arg;
    } break;
    case ARGP_KEY_NO_ARGS: {
        argp_usage(state);
    } break;
    default:
        return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

int parse_exec(int key, char *arg, struct argp_state *state)
{
    auto *input = (struct arg_exec *)state->input;

    switch (key) {
    case 'u': {
        std::string userSpec{ arg };
        auto semicolon = std::find(userSpec.cbegin(), userSpec.cend(), ':');

        std::string uid{ userSpec.cbegin(), semicolon };
        input->uid = uid;
        input->gid = input->uid;

        if (semicolon != userSpec.cend()) {
            std::string gid{ semicolon + 1, userSpec.cend() };
            input->gid = gid;
        }
    } break;
    case OPTION_CWD: {
        input->cwd = arg;
    } break;
    case ARGP_KEY_NO_ARGS: {
        argp_usage(state);
    } break;
    default:
        return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

int cmd_list(struct argp_state *state)
{
    struct arg_list list_arg
    {
        .global = (struct arg_global *)state->input
    };

    int argc = state->argc - state->next + 1;
    char **argv = &state->argv[state->next - 1];
    char *argv0 = argv[0];

    argv[0] = (char *)::malloc(strlen(state->name) + strlen(" list") + 1);
    if (argv[0] == nullptr) {
        argp_failure(state, -1, ENOMEM, nullptr);
    }
    sprintf(argv[0], "%s list", state->name);

    struct argp_option list_opt[] = { {
                                        .name = "format",
                                        .key = 'f',
                                        .arg = "FORMAT",
                                        .flags = 0,
                                        .doc = "select one of: table or json (default: \"table\")",
                                        .group = 0,
                                      },
                                      { nullptr } };

    struct argp list_argp = { .options = list_opt, .parser = parse_list, .doc = "OCI runtime" };

    argp_parse(&list_argp, argc, argv, ARGP_IN_ORDER, &argc, &list_arg);
    ::free(argv[0]);
    argv[0] = argv0;
    state->next += argc - 1;

    list_arg.global->exitCode = list(&list_arg);
    return 0;
}

int cmd_run(struct argp_state *state)
{
    struct arg_run run_arg
    {
        .global = (struct arg_global *)state->input
    };

    int argc = state->argc - state->next + 1;
    char **argv = &state->argv[state->next - 1];
    char *argv0 = argv[0];

    argv[0] = (char *)::malloc(strlen(state->name) + strlen(" run") + 1);
    if (argv[0] == nullptr) {
        argp_failure(state, -1, ENOMEM, nullptr);
    }
    sprintf(argv[0], "%s run", state->name);

    struct argp_option run_opt[] = { {
                                       .name = "bundle",
                                       .key = 'b',
                                       .arg = "DIR",
                                       .flags = 0,
                                       .doc = "container bundle (default \".\")",
                                       .group = 0,
                                     },
                                     {
                                       .name = "config",
                                       .key = 'f',
                                       .arg = "FILE",
                                       .flags = 0,
                                       .doc = "override the config file name",
                                       .group = 0,
                                     },
                                     { nullptr } };

    struct argp run_argp = { .options = run_opt,
                             .parser = parse_run,
                             .args_doc = "CONTAINER",
                             .doc = "OCI runtime" };

    argp_parse(&run_argp, argc, argv, ARGP_IN_ORDER, &argc, &run_arg);

    ::free(argv[0]);
    argv[0] = argv0;
    if (!argv[state->next]) {
        logErr() << "container id must be set";
        return -1;
    }

    std::string container{ argv[state->next] };
    state->next += argc;
    run_arg.global->exitCode = run(&run_arg, container);
    return 0;
}

int cmd_exec(struct argp_state *state)
{
    struct arg_exec exec_arg
    {
        .global = (struct arg_global *)state->input
    };

    int argc = state->argc - state->next + 1;
    char **argv = &state->argv[state->next - 1];
    char *argv0 = argv[0];

    argv[0] = (char *)::malloc(strlen(state->name) + strlen(" exec") + 1);
    if (argv[0] == nullptr) {
        argp_failure(state, -1, ENOMEM, nullptr);
    }
    sprintf(argv[0], "%s exec", state->name);

    struct argp_option exec_opt[] = { {
                                        .name = "user",
                                        .key = 'u',
                                        .arg = "USERSPEC",
                                        .flags = 0,
                                        .doc = "specify the user in the form UID[:GID]",
                                        .group = 0,
                                      },
                                      { .name = "cwd",
                                        .key = OPTION_CWD,
                                        .arg = "CWD",
                                        .flags = 0,
                                        .doc = "current working directory",
                                        .group = 0 },
                                      { nullptr } };

    struct argp exec_argp = { .options = exec_opt,
                              .parser = parse_exec,
                              .args_doc = "CONTAINER cmd",
                              .doc = "OCI runtime" };

    argp_parse(&exec_argp, argc, argv, ARGP_IN_ORDER, &argc, &exec_arg);

    ::free(argv[0]);
    argv[0] = argv0;
    state->next += argc - 1;

    argc = state->argc - state->next;
    argv = &state->argv[state->next];
    exec_arg.global->exitCode = exec(&exec_arg, argc, argv);

    // consume args
    state->next += argc - 1;
    return 0;
}

int cmd_kill(struct argp_state *state)
{
    int argc = state->argc - state->next + 1;
    char **argv = &state->argv[state->next - 1];
    char *argv0 = argv[0];

    argv[0] = (char *)::malloc(strlen(state->name) + strlen(" kill") + 1);
    if (argv[0] == nullptr) {
        argp_failure(state, -1, ENOMEM, nullptr);
    }
    sprintf(argv[0], "%s kill", state->name);

    struct argp kill_argp = { .options = nullptr,
                              .parser = nullptr,
                              .args_doc = "CONTAINER",
                              .doc = "OCI runtime" };
    // no option currently, just for doc
    argp_parse(&kill_argp, argc, argv, ARGP_IN_ORDER, &argc, nullptr);

    ::free(argv[0]);
    argv[0] = argv0;
    state->next += argc - 1;

    if (state->argv[state->next] == nullptr) {
        logErr() << "container id must be set.";
        return EINVAL;
    }

    std::string container{ state->argv[state->next++] };
    std::string signal;
    if (state->argv[state->next] != nullptr) {
        signal = state->argv[state->next++];
    }

    while (state->argv[state->next] != nullptr) {
        ++(state->next);
    }

    auto *global = (struct arg_global *)state->input;
    global->exitCode = kill(container, signal);
    return 0;
}

int parse_global(int key, char *arg, struct argp_state *state)
{
    auto *input = (struct arg_global *)state->input;

    switch (key) {
    case OPTION_CGROUP_MANAGER: {
        if (::strcmp(arg, "disabled") == 0) {
            input->cgroupManager = arg;
            break;
        }

        argp_failure(state, -1, EINVAL, "invalid cgroup manager %s", arg);
        return -1;
    } break;
    case ARGP_KEY_ARG: {
        if (::strcmp(arg, "list") == 0) {
            return cmd_list(state);
        }

        if (::strcmp(arg, "run") == 0) {
            return cmd_run(state);
        }

        if (::strcmp(arg, "exec") == 0) {
            return cmd_exec(state);
        }

        if (::strcmp(arg, "kill") == 0) {
            return cmd_kill(state);
        }

        argp_error(state, "unknown command %s", arg);

        return -1;
    } break;
    case ARGP_KEY_NO_ARGS: {
        argp_usage(state);
    } break;
    default:
        return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc == 1) {
        logErr() << "please specify a command";
        return -1;
    }
    // make sure that all containers status are valid 
    containerJsonCleanUp();
    struct argp_option options[] = { {
                                       .name = "cgroup-manager",
                                       .key = OPTION_CGROUP_MANAGER,
                                       .arg = "MANAGER",
                                       .flags = 0,
                                       .doc = "cgroup manager",
                                       .group = 0,
                                     },
                                     { nullptr } };

    auto doc = "\nCOMMANDS:\n"
               "\tlist        - list known containers\n"
               "\trun         - run a container\n"
               "\texec        - exec a command in a running container\n"
               "\tkill        - send a signal to the container init process\n";

    struct argp global_argp = { .options = options,
                                .parser = parse_global,
                                .args_doc = "COMMAND [OPTION...]",
                                .doc = doc };

    struct arg_global global;

    argp_parse(&global_argp, argc, argv, ARGP_IN_ORDER, nullptr, &global);

    return global.exitCode;
}
