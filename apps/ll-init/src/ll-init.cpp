// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <sys/prctl.h>
#include <sys/signalfd.h>

#include <array>
#include <csignal>
#include <cstring>
#include <iostream>
#include <variant>
#include <vector>

#include <sys/wait.h>

// no need to block these signals
constexpr std::array<int, 13> unblock_signals{ SIGABRT, SIGBUS,  SIGFPE,  SIGILL,  SIGSEGV,
                                               SIGSYS,  SIGTRAP, SIGXCPU, SIGXFSZ, SIGTTIN,
                                               SIGTTOU, SIGINT,  SIGTERM };

struct sig_conf
{
    sigset_t cur_set{};
    sigset_t old_set{};
    struct sigaction ttin_action{};
    struct sigaction ttou_action{};
    struct sigaction int_action{};
    struct sigaction term_action{};
};

sig_atomic_t received_signal{ 0 };

namespace {

void print_sys_error(std::string_view msg) noexcept
{
    std::cerr << msg << ": " << ::strerror(errno) << std::endl;
}

void print_info(std::string_view msg) noexcept
{
    std::cerr << msg << std::endl;
}

// ATTENTION: there is a potential risk that the signal will be overridden if multiple signals are
// received at the same time.
void sig_forward(int signo) noexcept
{
    received_signal = signo;
}

using sig_result = std::variant<int, sig_conf>;

sig_result handle_signals() noexcept
{
    sig_conf conf;
    ::sigfillset(&conf.cur_set);
    for (auto signal : unblock_signals) {
        ::sigdelset(&conf.cur_set, signal);
    }

    // ignore the rest of the signals
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

    // we only forward SIGINT, SIGTERM to the child process
    struct sigaction forward_action{};
    forward_action.sa_handler = sig_forward;
    forward_action.sa_flags = 0;

    if (::sigaction(SIGINT, &forward_action, &conf.int_action) == -1) {
        print_sys_error("Failed to forward SIGINT");
        return -1;
    }

    if (::sigaction(SIGTERM, &forward_action, &conf.term_action) == -1) {
        print_sys_error("Failed to forward SIGTERM");
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

        ret = ::sigaction(SIGINT, &conf.int_action, nullptr);
        if (ret == -1) {
            print_sys_error("Failed to restore SIGINT action");
            return -1;
        }

        ret = ::sigaction(SIGTERM, &conf.term_action, nullptr);
        if (ret == -1) {
            print_sys_error("Failed to restore SIGTERM action");
            return -1;
        }

        args.emplace_back(nullptr);

        ::execv(args[0], const_cast<char *const *>(args.data()));
        print_sys_error("Failed to exec");
        ::_exit(EXIT_FAILURE);
    }

    return pid;
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
        print_info("Failed to run child process");
        return -1;
    }

    int child_status{};
    while (true) {
        ret = waitpid(-1, &child_status, 0);
        if (ret != -1) {
            if (ret == child) {
                // if child process already exited, we will send the signal to all processes in this
                // pid namespace
                child = -1;
            }

            continue;
        }

        if (errno == EINTR) {
            ret = ::kill(child, received_signal);
            if (ret == -1) {
                auto msg = std::string("Failed to forward signal ") + ::strsignal(received_signal);
                print_sys_error(msg);
            }
        }

        if (errno == ECHILD) {
            break;
        }
    }

    if (WIFEXITED(child_status)) {
        print_info("Last child exited with status " + std::to_string(WEXITSTATUS(child_status)));
    } else if (WIFSIGNALED(child_status)) {
        print_info("Last child exited with signal " + std::to_string(WTERMSIG(child_status)));
    } else {
        print_info("Last child exited with unknown status");
    }

    return 0;
}
