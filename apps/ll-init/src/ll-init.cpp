// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <sys/prctl.h>
#include <sys/signalfd.h>

#include <array>
#include <csignal>
#include <cstring>
#include <iostream>
#include <optional>
#include <variant>
#include <vector>

#include <sys/wait.h>

// no need to block these signals
constexpr std::array<int, 11> unblock_signals{ SIGABRT, SIGBUS,  SIGFPE,  SIGILL,  SIGSEGV, SIGSYS,
                                               SIGTRAP, SIGXCPU, SIGXFSZ, SIGTTIN, SIGTTOU };

struct sig_conf
{
    sigset_t cur_set{};
    sigset_t old_set{};
    struct sigaction ttin_action{};
    struct sigaction ttou_action{};
};

namespace {

void print_sys_error(std::string_view msg) noexcept
{
    std::cerr << msg << ": " << ::strerror(errno) << std::endl;
}

void print_info(std::string_view msg) noexcept
{
    std::cout << msg << std::endl;
}

using sig_result = std::variant<int, sig_conf>;

sig_result handle_signals() noexcept
{
    sig_conf conf;
    ::sigfillset(&conf.cur_set);
    for (auto signal : unblock_signals) {
        ::sigdelset(&conf.cur_set, signal);
    }

    auto ret = ::sigprocmask(SIG_SETMASK, &conf.cur_set, &conf.old_set);
    if (ret == -1) {
        print_sys_error("Failed to set signal mask");
        return -1;
    }

    // background process will be suspended by SIGTTIN and SIGTTOU if it try to read/write from/to
    // terminal, so we need to ignore these signals.
    struct sigaction ignore{};
    ignore.sa_handler = SIG_IGN;
    ignore.sa_flags = 0;

    if (::sigaction(SIGTTIN, &ignore, &conf.ttin_action) == -1) {
        print_sys_error("Failed to ignore SIGTTIN");
        return -1;
    }

    if (::sigaction(SIGTTOU, &ignore, &conf.ttou_action) == -1) {
        print_sys_error("Failed to ignore SIGTTOU");
        return -1;
    }

    return conf;
}

std::vector<const char *> parse_args(int argc, char *argv[]) noexcept
{
    std::vector<const char *> args;
    int idx{ 1 };
    while (idx < argc) {
        args.emplace_back(argv[idx++]);
    }

    return args;
}

pid_t run(std::vector<const char *> args, const sig_conf &conf) noexcept
{
    auto pid = ::fork();
    if (pid == -1) {
        print_sys_error("Failed to fork");
        return -1;
    }

    if (pid == 0) {
        auto ret = ::setpgid(0, 0);
        if (ret == -1) {
            print_sys_error("Failed to set process group");
            return -1;
        }

        ret = ::tcsetpgrp(0, ::getpid());
        if (ret == -1 && errno != ENOTTY) {
            print_sys_error("Failed to set terminal process group");
            return -1;
        }

        ret = ::sigprocmask(SIG_SETMASK, &conf.old_set, nullptr);
        if (ret == -1) {
            print_sys_error("Failed to restore signal mask");
            return -1;
        }

        ret = ::sigaction(SIGTTIN, &conf.ttin_action, nullptr);
        if (ret == -1) {
            print_sys_error("Failed to restore SIGTTIN action");
            return -1;
        }

        ret = ::sigaction(SIGTTOU, &conf.ttou_action, nullptr);
        if (ret == -1) {
            print_sys_error("Failed to restore SIGTTOU action");
            return -1;
        }

        args.emplace_back(nullptr);

        ::execv(args[0], const_cast<char *const *>(args.data()));
        print_sys_error("Failed to exec");
        return -1;
    }

    return pid;
}

std::optional<int> handle_exited_child(pid_t child) noexcept
{
    int status{};
    while (true) {
        auto ret = ::waitpid(-1, &status, WNOHANG);
        if (ret == 0) {
            break;
        }

        if (ret == child) {
            return status;
        }
    }

    return std::nullopt;
}
} // namespace

int main(int argc, char *argv[])
{
    auto sig_ret = handle_signals();
    if (std::holds_alternative<int>(sig_ret)) {
        return std::get<int>(sig_ret);
    }

    auto conf = std::get<sig_conf>(sig_ret);

    auto ret = ::prctl(PR_SET_CHILD_SUBREAPER, 1);
    if (ret == -1) {
        print_sys_error("Failed to set child subreaper");
        return -1;
    }

    auto args = parse_args(argc, argv);
    if (args.empty()) {
        print_info("No arguments provided");
        return -1;
    }

    auto child = run(args, conf);
    if (child == -1) {
        return -1;
    }

    auto sigfd = ::signalfd(-1, &conf.cur_set, 0);
    if (sigfd == -1) {
        print_sys_error("Failed to create signalfd");
        return -1;
    }

    int child_status{};
    signalfd_siginfo info{};
    while (true) {
        std::memset(&info, 0, sizeof(info));
        auto ret = ::read(sigfd, &info, sizeof(info));
        if (ret == -1) {
            print_sys_error("Failed to read from signalfd");
            return -1;
        }

        if (info.ssi_signo != SIGCHLD) {
            ret = ::kill(child, info.ssi_signo);
            if (ret == -1) {
                auto msg = std::string("Failed to forward signal ") + ::strsignal(info.ssi_signo);
                print_sys_error(msg);
            }
        }

        auto child_exited = handle_exited_child(child);
        if (child_exited) {
            child_status = *child_exited;
            break;
        }
    }

    ::close(sigfd);

    if (WIFEXITED(child_status)) {
        print_info("Child exited with status " + std::to_string(WEXITSTATUS(child_status)));
    } else if (WIFSIGNALED(child_status)) {
        print_info("Child exited with signal " + std::to_string(WTERMSIG(child_status)));
    } else {
        print_info("Child exited with unknown status");
    }

    return 0;
}
