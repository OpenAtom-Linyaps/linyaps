/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "container/container.h"
#include "container/helper.h"
#include "util/logger.h"
#include "util/oci_runtime.h"
#include "util/platform.h"

#include <argp.h>

#include <csignal>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <variant>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

const char *argp_program_bug_address = // NOLINT
  "https://github.com/OpenAtom-Linyaps/linyaps/issues";

namespace {

struct box_args;

struct arg_list
{
    struct box_args *global{ nullptr };
    std::string format{ "table" };
};

struct arg_run
{
    struct box_args *global{ nullptr };
    std::string container;
    std::string bundle{ std::filesystem::current_path() };
    std::string config{ "config.json" };
};

struct arg_exec
{
    struct box_args *global{ nullptr };
    std::string container;
    std::string uid{ "0" };
    std::string gid{ "0" };
    std::string cwd{ "/" };
    std::vector<std::string> cmd;
};

struct arg_kill
{
    struct box_args *global{ nullptr };
    std::string container;
    std::optional<std::string> signal;
};

struct box_args
{
    std::variant<arg_list, arg_run, arg_exec, arg_kill> subCommand;
    std::string cgroupManager;
    std::string root;
};

enum globalOption { OPTION_CGROUP_MANAGER = 1000, OPTION_ROOT };

enum killOption { OPTION_KILL_CONTAINER = 1000, OPTION_KILL_SIGNAl };

enum execOption { OPTION_CWD = 1000 };

void containerJsonCleanUp(const std::filesystem::path &root)
{
    auto containers = linglong::readAllContainerJson(root);

    auto it = containers.begin(); // NOLINT
    while (it != containers.end()) {
        auto boxPid = it->value("pid", -1);
        if (boxPid == -1) {
            ++it;
            continue;
        }

        if (kill(boxPid, 0) != 0) {
            auto jsonPath = root / (it->value("id", "unknown") + ".json");
            if (!std::filesystem::remove(jsonPath)) {
                logErr() << "remove" << jsonPath << "failed";
            }
            it = containers.erase(it);
        } else {
            ++it;
        }
    }
}

int list(const arg_list &arg) noexcept
{
    auto containers = linglong::readAllContainerJson(arg.global->root);
    if (arg.format == "json") {
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

pid_t findLastBoxFallback(pid_t rootPid) noexcept
{
    std::error_code ec;
    auto proc_iterator = std::filesystem::directory_iterator("/proc", ec);
    if (ec) {
        logErr() << "create directory iterator of /proc error:" << ec.message();
        return -1;
    }

    auto boxBinPath = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec) {
        logErr() << "read symlink error:" << ec.message();
        return -1;
    }
    auto boxBinName = boxBinPath.filename().string();

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
    pid_t lastBox{ -1 };
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

        if (auto binStr = bin.filename().string(); binStr == boxBinName) {
            lastBox = currentPid;
            currentPid = pid;
            continue;
        }

        break;
    }

    return lastBox;
}

pid_t findLastBox(pid_t rootPid) noexcept
{
    std::filesystem::path proc{ "/proc" };
    if (!std::filesystem::exists(proc)) {
        logFal() << "/proc doesn't exist.";
    }

    auto pidStr = std::to_string(rootPid);
    auto children = proc / pidStr / "task" / pidStr / "children";
    if (!std::filesystem::exists(children)) {
        logDbg() << children.string() << "not exist, fallback to stat file";
        return findLastBoxFallback(rootPid);
    }

    auto boxBin = std::filesystem::read_symlink("/proc/self/exe");
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

            if (realExe.filename() == boxBin.filename()) {
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

int exec(const arg_exec &arg) noexcept
{
    const auto &containerID = arg.container;
    auto containers = linglong::readAllContainerJson(arg.global->root);
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

    auto lastBox = findLastBox(boxPid);
    if (lastBox == -1) {
        logErr() << "couldn't find pid of last ll-box";
        return -1;
    }

    auto boxPidStr = std::to_string(lastBox);
    auto wdns = linglong::util::format("--wdns=%s", arg.cwd.c_str());

    std::vector<const char *> newArgv{ "nsenter", "-t",         boxPidStr.c_str(),
                                       "-U",      "-m",         "-p",
                                       "-u",      wdns.c_str(), "--preserve-credentials" };

    for (const auto &str : arg.cmd) {
        newArgv.push_back(str.c_str());
    }

    newArgv.push_back(nullptr);

    for (decltype(newArgv.size()) i = 0; i < newArgv.size(); ++i) {
        logDbg() << "newArgv[" << i << "]:" << newArgv[i];
    }

    return ::execvp("nsenter", const_cast<char **>(newArgv.data())); // NOLINT
}

int run(const arg_run &arg) noexcept
try {
    auto bundleDir = std::filesystem::path(arg.bundle);
    auto configFile = bundleDir / arg.config;
    auto configFileStream = std::ifstream(configFile);
    if (!configFileStream.is_open()) {
        logErr() << "failed to open config" << configFile;
        return -1;
    }

    auto json = nlohmann::json::parse(configFileStream);
    auto runtime = json.get<linglong::Runtime>();

    linglong::Container container(bundleDir, arg.container, arg.global->root, runtime);
    return container.Start();
} catch (const std::exception &e) {
    logErr() << "run failed:" << e.what();
    return -1;
}

int kill(const arg_kill &arg) noexcept
{
    auto signal = arg.signal.value_or("SIGTERM");
    int sig{ -1 };
    while (true) {
        if (std::all_of(signal.cbegin(), signal.cend(), ::isdigit)) {
            sig = std::stoi(signal);
            break;
        }

        if (signal.rfind("SIG", 0) == std::string::npos) {
            signal.insert(0, "SIG");
        }

        sig = linglong::util::strToSig(signal);
        if (sig == -1) {
            logErr() << "invalid signal" << signal;
            return -1;
        }

        break;
    }

    auto containers = linglong::readAllContainerJson(arg.global->root);
    for (auto container : containers) {
        if (container["id"] != arg.container) {
            continue;
        }

        auto pid = container["pid"].get<pid_t>();
        logDbg() << "kill" << pid << "with" << signal << "(" << sig << ")";
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
        if (arg[0] != '/') {
            input->bundle = std::filesystem::current_path() / arg;
        } else {
            input->bundle = arg;
        }
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

        input->uid = std::string{ userSpec.cbegin(), semicolon };
        if (!std::all_of(input->uid.cbegin(), input->uid.cend(), ::isdigit)) {
            argp_error(state, "invalid uid %s", input->uid.c_str());
        }

        input->gid = input->uid;
        if (semicolon != userSpec.cend()) {
            input->gid = std::string{ semicolon + 1, userSpec.cend() };
            if (!std::all_of(input->gid.cbegin(), input->gid.cend(), ::isdigit)) {
                argp_error(state, "invalid gid %s", input->gid.c_str());
            }
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

arg_list subCommand_list(struct argp_state *state)
{
    struct arg_list list_arg{
        .global = reinterpret_cast<struct box_args *>(state->input), // NOLINT
    };

    int argc = state->argc - state->next + 1;
    char **argv = &state->argv[state->next - 1]; // NOLINT

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
    state->next += argc - 1;

    return list_arg;
}

arg_run subCommand_run(struct argp_state *state)
{
    struct arg_run run_arg{
        .global = reinterpret_cast<struct box_args *>(state->input), // NOLINT
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

    argv[0] = argv0; // NOLINT
    state->next += argc - 1;

    if (state->argv[state->next] == nullptr) {                       // NOLINT
        argp_failure(state, -1, EINVAL, "container id must be set"); // NOLINT
    }
    run_arg.container = state->argv[state->next++]; // NOLINT

    return run_arg;
}

arg_exec subCommand_exec(struct argp_state *state)
{
    struct arg_exec exec_arg{
        .global = reinterpret_cast<struct box_args *>(state->input), // NOLINT
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
                              .doc = "OCI runtime" };                               // NOLINT
    if (argp_parse(&exec_argp, argc, argv, ARGP_IN_ORDER, &argc, &exec_arg) != 0) { // NOLINT
        argp_failure(state, -1, EINVAL, "parse exec args failed");
    }

    argv[0] = argv0; // NOLINT
    state->next += argc - 1;

    if (state->argv[state->next] == nullptr) { // NOLINT
        argp_failure(state, -1, EINVAL, "container id must be set.");
    }
    exec_arg.container = state->argv[state->next++];

    while (state->next < state->argc) {                        // NOLINT
        exec_arg.cmd.emplace_back(state->argv[state->next++]); // NOLINT
    }

    return exec_arg;
}

arg_kill subCommand_kill(struct argp_state *state)
{
    struct arg_kill kill_arg{
        .global = reinterpret_cast<struct box_args *>(state->input), // NOLINT
    };

    int argc = state->argc - state->next + 1;
    char **argv = &state->argv[state->next - 1]; // NOLINT
    char *argv0 = argv[0];                       // NOLINT

    std::string name = state->name;
    name += " kill";
    argv[0] = name.data(); // NOLINT

    struct argp kill_argp = { .options = nullptr,
                              .parser = nullptr,
                              .args_doc = "CONTAINER [SIGNAL]",
                              .doc = "OCI runtime" }; // NOLINT
    // no option currently, just for doc
    if (argp_parse(&kill_argp, argc, argv, ARGP_IN_ORDER, &argc, &kill_arg) != 0) { // NOLINT
        argp_failure(state, -1, EINVAL, "parse kill args failed");
    }

    argv[0] = argv0;
    state->next += argc - 1;

    if (state->argv[state->next] == nullptr) { // NOLINT
        argp_failure(state, -1, EINVAL, "container id must be set");
    }
    kill_arg.container = state->argv[state->next++];

    if (state->argv[state->next] != nullptr) { // NOLINT
        kill_arg.signal = state->argv[state->next++];
    }

    return kill_arg;
}

int parse_global(int key, char *arg, struct argp_state *state)
{
    auto *input = reinterpret_cast<struct box_args *>(state->input); // NOLINT

    switch (key) {
    case OPTION_CGROUP_MANAGER: {
        if (::strcmp(arg, "disabled") == 0) {
            input->cgroupManager = arg;
            break;
        }

        argp_failure(state, -1, EINVAL, "invalid cgroup manager %s", arg); // NOLINT
        return -1;
    } break;
    case OPTION_ROOT: {
        if (arg != nullptr) {
            input->root = arg;
        }

        break;
    }
    case ARGP_KEY_ARG: {
        if (::strcmp(arg, "list") == 0) {
            input->subCommand = subCommand_list(state);
        } else if (::strcmp(arg, "run") == 0) {
            input->subCommand = subCommand_run(state);
        } else if (::strcmp(arg, "exec") == 0) {
            input->subCommand = subCommand_exec(state);
        } else if (::strcmp(arg, "kill") == 0) {
            input->subCommand = subCommand_kill(state);
        } else {
            argp_error(state, "unknown command %s", arg); // NOLINT
            return -1;
        }
    } break;
    case ARGP_KEY_NO_ARGS: {
        argp_usage(state); // NOLINT
    } break;
    default:
        return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

template<typename... Ts>
struct Overload : public Ts...
{
    using Ts::operator()...;
};

template<class... Ts>
Overload(Ts...) -> Overload<Ts...>;

} // namespace

int main(int argc, char **argv)
{
    // detecting some kernel features at runtime to reporting errors friendly
    std::error_code ec;
    std::filesystem::path feature{ "/proc/sys/kernel/unprivileged_userns_clone" };

    auto check = [](const std::filesystem::path &setting, int expected) {
        // We assume that the fact that a file does not exist or that an error occurs during the
        // detection process does not mean that the feature is disabled.
        std::error_code ec;
        if (!std::filesystem::exists(setting, ec)) {
            return true;
        }

        std::ifstream stream{ setting };
        if (!stream.is_open()) {
            return true;
        }

        std::string content;
        std::getline(stream, content);

        try {
            return std::stoi(content) == expected;
        } catch (std::exception &e) {
            logWan() << "ignore exception" << e.what() << "and continue"; // NOLINT
            return true;
        }
    };

    // for debian:
    // https://salsa.debian.org/kernel-team/linux/-/blob/debian/latest/debian/patches/debian/add-sysctl-to-disallow-unprivileged-CLONE_NEWUSER-by-default.patch
    if (!check("/proc/sys/kernel/unprivileged_userns_clone", 1)) {
        logErr() << "unprivileged_userns_clone is not enabled";
        return EPERM;
    }

    // for ubuntu:
    // https://gitlab.com/apparmor/apparmor/-/wikis/unprivileged_userns_restriction#disabling-unprivileged-user-namespaces
    if (!check("/proc/sys/kernel/apparmor_restrict_unprivileged_unconfined", 0)) {
        logErr() << "apparmor_restrict_unprivileged_unconfined is not disabled";
        return EPERM;
    }
    if (!check("/proc/sys/kernel/apparmor_restrict_unprivileged_userns", 0)) {
        logErr() << "apparmor_restrict_unprivileged_userns is not disabled";
        return EPERM;
    }

    if (argc == 1) {
        logErr() << "please specify a command";
        return -1;
    }

    std::string defaultRuntimeDir;
    auto *XDGRuntimeDir = ::getenv("XDG_RUNTIME_DIR");
    if (XDGRuntimeDir == nullptr) {
        defaultRuntimeDir = "/run";
    } else {
        defaultRuntimeDir = XDGRuntimeDir;
    }

    auto defaultRootDir = std::filesystem::path{ defaultRuntimeDir } / "ll-box";
    auto rootDoc = std::string{ "root directory for storage of container state  (this should be "
                                "located in tmpfs) (default: " }
      + defaultRootDir.string() + ")";

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
          {
            .name = "root",
            .key = OPTION_ROOT,
            .arg = "DIR",
            .flags = 0,
            .doc = rootDoc.c_str(),
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

    struct box_args global{ .root = defaultRootDir };

    if (argp_parse(&global_argp, argc, argv, ARGP_IN_ORDER, nullptr, &global) != 0) {
        logErr() << "failed to parse arguments";
        return -1;
    }

    // make sure that all containers status are valid
    containerJsonCleanUp(global.root);

    return std::visit(Overload{ [](const arg_list &args) {
                                   return list(args);
                               },
                                [](const arg_run &args) {
                                    return run(args);
                                },
                                [](const arg_exec &args) {
                                    return exec(args);
                                },
                                [](const arg_kill &arg) {
                                    return kill(arg);
                                } },
                      global.subCommand);
}
