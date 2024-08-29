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

const char *argp_program_bug_address = "https://github.com/linuxdeepin/linglong/issues"; // NOLINT

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

    auto it = containers.begin(); // NOLINT
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

pid_t findParentPid(const std::filesystem::path &process) noexcept
{
    auto stat = process / "stat";
    std::ifstream stream{ stat };
    if (!stream.is_open()) {
        logErr() << "couldn't open stat file" << stat.string();
        return -1;
    }

    std::string content;
    if (!std::getline(stream, content, '\n')) {
        logErr() << "couldn't read from stat file";
        return -1;
    }
    stream.close();

    std::istringstream sstream{ content };
    std::string element;
    for (int i = 0; i < 4; std::getline(sstream, element, ' '), ++i) { }
    return std::stoi(element);
}

pid_t findInitProcessFallback(pid_t rootPid, const std::string &initBin) noexcept
{
    std::error_code ec;
    auto proc_iterator = std::filesystem::directory_iterator("/proc", ec);
    if (ec) {
        logErr() << "create directory iterator of /proc error:" << ec.message();
        return -1;
    }

    std::map<pid_t, std::filesystem::path> processes;
    for (const auto &entry : proc_iterator) {
        if (!entry.is_directory(ec)) {
            if (ec) {
                logErr() << "filesystem error:" << ec.message();
                return -1;
            }

            continue;
        }

        pid_t pid{ -1 };
        try {
            pid = std::stoi(entry.path().filename().string());
        } catch (std::invalid_argument &e) {
            // ignore these directory
            continue;
        }

        if (pid <= rootPid) {
            continue;
        }

        processes.insert({ pid, entry.path() });
    }

    auto currentPid = rootPid;
    pid_t initPid{ -1 };
    for (const auto &[pid, path] : processes) {
        auto parent = findParentPid(path);
        if (parent != currentPid) {
            continue;
        }

        auto bin = std::filesystem::read_symlink(path / "exe", ec);
        if (ec) {
            logErr() << "read symlink error:" << ec.message();
            return -1;
        }

        if (auto binStr = bin.filename().string(); binStr == initBin) {
            initPid = currentPid;
            currentPid = pid;
            continue;
        }

        break;
    }

    return initPid;
}

pid_t findInitProcess(pid_t rootPid) noexcept
{
    std::filesystem::path proc{ "/proc" };
    if (!std::filesystem::exists(proc)) {
        logFal() << "/proc doesn't exist.";
    }

    std::string initBin = "bash";
    auto pidStr = std::to_string(rootPid);
    auto children = proc / pidStr / "task" / pidStr / "children";
    if (!std::filesystem::exists(children)) {
        logDbg() << children.string() << "not exist, fallback to stat file";
        return findInitProcessFallback(rootPid, initBin);
    }
    
    std::error_code ec; // NOLINT
    while (true) {
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

            if (realExe.filename() == initBin) {
                childBoxFound = true;
                break;
            }
        }

        if (childBoxFound) {
            pidStr = std::to_string(chPid);
            children = proc / pidStr / "task" / pidStr / "children";
            continue;
        }

        break;
    }

    return std::stoi(pidStr);
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

    // https://github.com/OpenAtom-Linyaps/linyaps/pull/665/commits/74fd810e56acac3515c84b438741766d61956809
    // we make bash to be the init process(pid=1) in container.
    // Now, we need to find bash instead of ll-box
    auto initProcess = findInitProcess(boxPid);
    if (initProcess == -1) {
        logErr() << "couldn't find pid of init process which in container";
        return -1;
    }

    auto initProcessStr = std::to_string(initProcess);
    auto wdns = linglong::util::format("--wdns=%s", arg->cwd.c_str());

    std::vector<const char *> newArgv{
        "nsenter", "-t", initProcessStr.c_str(), "-U", "-m", "-p", wdns.c_str(), "--preserve-credentials"
    };

    for (int i = 0; i < argc; ++i) {
        newArgv.push_back(argv[i + 1]);
    }

    newArgv.push_back(nullptr);

    for (decltype(newArgv.size()) i = 0; i < newArgv.size(); ++i) {
        logDbg() << "newArgv[" << i << "]:" << newArgv[i];
    }

    return ::execvp("nsenter", const_cast<char **>(newArgv.data())); // NOLINT
}

int run(struct arg_run *arg, const std::string &containerID) noexcept
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

    linglong::Container container(bundleDir, containerID, runtime);
    return container.Start();
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
    auto *input = reinterpret_cast<struct arg_list *>(state->input); // NOLINT
    static std::vector<std::string> formatMap = { "json", "table" };

    if (key != 'f') {
        return ARGP_ERR_UNKNOWN;
    }

    std::string val{ arg };
    if (std::find(formatMap.cbegin(), formatMap.cend(), val) == formatMap.cend()) {
        argp_failure(state, -1, EINVAL, "invalid format %s", arg); // NOLINT
    }
    input->format = std::move(val);

    return 0;
}

int parse_run(int key, char *arg, struct argp_state *state)
{
    auto *input = reinterpret_cast<struct arg_run *>(state->input); // NOLINT

    switch (key) {
    case 'f': {
        input->config = arg;
    } break;
    case 'b': {
        input->bundle = arg;
    } break;
    case ARGP_KEY_NO_ARGS: {
        argp_usage(state); // NOLINT
    } break;
    default:
        return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

int parse_exec(int key, char *arg, struct argp_state *state)
{
    auto *input = reinterpret_cast<struct arg_exec *>(state->input); // NOLINT

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
        argp_usage(state); // NOLINT
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
        .global = reinterpret_cast<struct arg_global *>(state->input), // NOLINT
    };

    int argc = state->argc - state->next + 1;
    char **argv = &state->argv[state->next - 1]; // NOLINT
    char *argv0 = argv[0];                       // NOLINT

    std::string name = state->name;
    name += " list";
    argv[0] = name.data(); // NOLINT

    struct argp_option list_opt[] = // NOLINT
      {
          {
            .name = "format",
            .key = 'f',
            .arg = "FORMAT",
            .flags = 0,
            .doc = "select one of: table or json (default: \"table\")",
            .group = 0,
          },
          { nullptr } // NOLINT
      };

    struct argp list_argp = { .options = list_opt, // NOLINT
                              .parser = parse_list,
                              .doc = "OCI runtime" }; // NOLINT

    argp_parse(&list_argp, argc, argv, ARGP_IN_ORDER, &argc, &list_arg); // NOLINT
    argv[0] = argv0;                                                     // NOLINT
    state->next += argc - 1;

    list_arg.global->exitCode = list(&list_arg);
    return 0;
}

int cmd_run(struct argp_state *state)
{
    struct arg_run run_arg
    {
        .global = reinterpret_cast<struct arg_global *>(state->input), // NOLINT
    };

    int argc = state->argc - state->next + 1;
    char **argv = &state->argv[state->next - 1]; // NOLINT
    char *argv0 = argv[0];                       // NOLINT

    std::string name = state->name;
    name += " run";

    argv[0] = name.data(); // NOLINT

    struct argp_option run_opt[] = // NOLINT
      {
          {
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
          { nullptr } // NOLINT
      };

    struct argp run_argp = { .options = run_opt, // NOLINT
                             .parser = parse_run,
                             .args_doc = "CONTAINER",
                             .doc = "OCI runtime" }; // NOLINT

    argp_parse(&run_argp, argc, argv, ARGP_IN_ORDER, &argc, &run_arg); // NOLINT

    argv[0] = argv0;          // NOLINT
    if (!argv[state->next]) { // NOLINT
        logErr() << "container id must be set";
        return -1;
    }

    std::string container{
        argv[state->next] // NOLINT
    };
    state->next += argc;
    run_arg.global->exitCode = run(&run_arg, container);
    return 0;
}

int cmd_exec(struct argp_state *state)
{
    struct arg_exec exec_arg
    {
        .global = reinterpret_cast<struct arg_global *>(state->input), // NOLINT
    };

    int argc = state->argc - state->next + 1;
    char **argv = &state->argv[state->next - 1]; // NOLINT
    char *argv0 = argv[0];                       // NOLINT

    std::string name = state->name;
    name += " exec";

    argv[0] = name.data(); // NOLINT

    struct argp_option exec_opt[] = // NOLINT
      {
          {
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
          { nullptr } // NOLINT
      };

    struct argp exec_argp = { .options = exec_opt, // NOLINT
                              .parser = parse_exec,
                              .args_doc = "CONTAINER cmd",
                              .doc = "OCI runtime" }; // NOLINT

    argp_parse(&exec_argp, argc, argv, ARGP_IN_ORDER, &argc, &exec_arg); // NOLINT

    argv[0] = argv0; // NOLINT
    state->next += argc - 1;

    argc = state->argc - state->next;
    argv = &state->argv[state->next]; // NOLINT
    exec_arg.global->exitCode = exec(&exec_arg, argc, argv);

    // consume args
    state->next += argc - 1;
    return 0;
}

int cmd_kill(struct argp_state *state)
{
    int argc = state->argc - state->next + 1;
    char **argv = &state->argv[state->next - 1]; // NOLINT
    char *argv0 = argv[0];                       // NOLINT

    std::string name = state->name;
    name += " kill";

    struct argp kill_argp = { .options = nullptr,
                              .parser = nullptr,
                              .args_doc = "CONTAINER",
                              .doc = "OCI runtime" }; // NOLINT
    // no option currently, just for doc
    argp_parse(&kill_argp, argc, argv, ARGP_IN_ORDER, &argc, nullptr); // NOLINT
    argv[0] = argv0;
    state->next += argc - 1;

    if (state->argv[state->next] == nullptr) { // NOLINT
        logErr() << "container id must be set.";
        return EINVAL;
    }

    std::string container{ state->argv[state->next++] }; // NOLINT
    std::string signal;
    if (state->argv[state->next] != nullptr) { // NOLINT
        signal = state->argv[state->next++];   // NOLINT
    }

    while (state->argv[state->next] != nullptr) { // NOLINT
        ++(state->next);
    }

    auto *global = reinterpret_cast<struct arg_global *>(state->input); // NOLINT
    global->exitCode = kill(container, signal);
    return 0;
}

int parse_global(int key, char *arg, struct argp_state *state)
{
    auto *input = reinterpret_cast<struct arg_global *>(state->input); // NOLINT

    switch (key) {
    case OPTION_CGROUP_MANAGER: {
        if (::strcmp(arg, "disabled") == 0) {
            input->cgroupManager = arg;
            break;
        }

        argp_failure(state, -1, EINVAL, "invalid cgroup manager %s", arg); // NOLINT
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

        argp_error(state, "unknown command %s", arg); // NOLINT

        return -1;
    } break;
    case ARGP_KEY_NO_ARGS: {
        argp_usage(state); // NOLINT
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
    struct argp_option options[] = // NOLINT
      {
          {
            .name = "cgroup-manager",
            .key = OPTION_CGROUP_MANAGER,
            .arg = "MANAGER",
            .flags = 0,
            .doc = "cgroup manager",
            .group = 0,
          },
          { nullptr } // NOLINT
      };

    const auto *doc = "\nCOMMANDS:\n"
                      "\tlist        - list known containers\n"
                      "\trun         - run a container\n"
                      "\texec        - exec a command in a running container\n"
                      "\tkill        - send a signal to the container init process\n";

    struct argp global_argp = { .options = options, // NOLINT
                                .parser = parse_global,
                                .args_doc = "COMMAND [OPTION...]",
                                .doc = doc }; // NOLINT

    struct arg_global global;

    argp_parse(&global_argp, argc, argv, ARGP_IN_ORDER, nullptr, &global); // NOLINT

    return global.exitCode;
}
