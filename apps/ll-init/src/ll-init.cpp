// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <sys/epoll.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>

#include <array>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <vector>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

// no need to block these signals
constexpr std::array unblock_signals{ SIGABRT, SIGBUS,  SIGFPE,  SIGILL, SIGSEGV,
                                      SIGSYS,  SIGTRAP, SIGXCPU, SIGXFSZ };

// now, we just inherit the control terminal from the outside and put all the processes in the
// same process group. maybe we need refactor this in the future.

namespace {

struct WaitPidResult
{
    pid_t pid;
    int status;
};

void print_sys_error(std::string_view msg) noexcept
{
    std::cerr << msg << ": " << ::strerror(errno) << std::endl;
}

void print_sys_error(std::string_view msg, const std::error_code &ec) noexcept
{
    std::cerr << msg << ": " << ec.message() << std::endl;
}

void print_info(std::string_view msg) noexcept
{
    static const auto is_debug = ::getenv("LINYAPS_INIT_VERBOSE_OUTPUT") != nullptr;
    if (is_debug) {
        std::cerr << msg << std::endl;
    }
}

class sigConf
{
public:
    sigConf() noexcept = default;
    sigConf(const sigConf &) = delete;
    sigConf(sigConf &&) = delete;
    sigConf &operator=(const sigConf &) = delete;
    sigConf &operator=(sigConf &&) = delete;
    ~sigConf() noexcept = default;

    bool block_signals() noexcept
    {
        ::sigfillset(&cur_set);
        for (auto signal : unblock_signals) {
            ::sigdelset(&cur_set, signal);
        }

        // ignore the rest of the signals
        auto ret = ::sigprocmask(SIG_SETMASK, &cur_set, &old_set);
        if (ret == -1) {
            print_sys_error("Failed to set signal mask");
            return false;
        }

        return true;
    }

    [[nodiscard]] bool restore_signals() const noexcept
    {
        auto ret = ::sigprocmask(SIG_SETMASK, &old_set, nullptr);
        if (ret == -1) {
            print_sys_error("Failed to restore signal mask");
            return false;
        }

        return true;
    }

    [[nodiscard]] const sigset_t &current_sigset() const noexcept { return cur_set; }

private:
    sigset_t cur_set{};
    sigset_t old_set{};
};

class file_descriptor_wrapper
{
public:
    explicit file_descriptor_wrapper(int fd) noexcept
        : fd(fd)
    {
    }

    file_descriptor_wrapper() noexcept = default;

    file_descriptor_wrapper(const file_descriptor_wrapper &) = delete;
    file_descriptor_wrapper &operator=(const file_descriptor_wrapper &) = delete;

    file_descriptor_wrapper(file_descriptor_wrapper &&other) noexcept
        : fd(other.fd)
    {
        other.fd = -1;
    }

    file_descriptor_wrapper &operator=(file_descriptor_wrapper &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        close();
        fd = other.fd;
        other.fd = -1;

        return *this;
    }

    ~file_descriptor_wrapper() noexcept { close(); }

    void close() noexcept
    {
        if (fd != -1) {
            ::close(fd);
            fd = -1;
        }
    }

    explicit operator bool() const noexcept { return fd != -1; }

    operator int() const noexcept { return fd; } // NOLINT

private:
    int fd{ -1 };
};

file_descriptor_wrapper create_signalfd(const sigset_t &sigset) noexcept
{
    auto fd = ::signalfd(-1, &sigset, SFD_NONBLOCK);
    if (fd == -1) {
        print_sys_error("Failed to create signalfd");
    }

    return file_descriptor_wrapper(fd);
}

file_descriptor_wrapper create_timerfd() noexcept
{
    auto fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (fd == -1) {
        print_sys_error("Failed to create timerfd");
    }

    return file_descriptor_wrapper(fd);
}

file_descriptor_wrapper create_epoll() noexcept
{
    auto fd = ::epoll_create1(0);
    if (fd == -1) {
        print_sys_error("Failed to create epoll");
    }

    return file_descriptor_wrapper(fd);
}

template <std::size_t N>
constexpr auto make_array(const char (&str)[N]) noexcept // NOLINT
{
    static_assert(N > 0, "N must be greater than 0");
    std::array<char, N - 1> arr{};
    for (std::size_t i = 0; i < N - 1; ++i) {
        arr[i] = str[i];
    }

    return arr;
}

std::pair<struct sockaddr_un, socklen_t> get_socket_address() noexcept
{
    constexpr auto fs_addr{ make_array("/run/linglong/init/socket") };

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;

    std::copy(fs_addr.cbegin(), fs_addr.cend(), &addr.sun_path[0]);
    addr.sun_path[fs_addr.size()] = 0;
    return std::make_pair(addr, offsetof(sockaddr_un, sun_path) + fs_addr.size());
}

file_descriptor_wrapper create_fs_uds() noexcept
{
    auto fd = ::socket(AF_UNIX, SOCK_NONBLOCK | SOCK_SEQPACKET, 0);
    file_descriptor_wrapper socket_fd{ fd };
    if (fd == -1) {
        print_sys_error("Failed to create unix domain socket");
        return socket_fd;
    }

    auto [addr, len] = get_socket_address();
    if (len == 0) {
        print_info("Failed to get socket address");
        return socket_fd;
    }

    // just in case the socket file exists
    ::unlink(addr.sun_path);

    auto ret = ::bind(socket_fd, reinterpret_cast<struct sockaddr *>(&addr), len);
    if (ret == -1) {
        print_sys_error("Failed to bind unix domain socket");
        return socket_fd;
    }

    ret = ::listen(socket_fd, 1);
    if (ret == -1) {
        print_sys_error("Failed to listen on unix domain socket");
        return socket_fd;
    }

    return socket_fd;
}

std::vector<const char *> parse_args(int argc, char *argv[]) noexcept
{
    std::vector<const char *> args;
    int idx{ 1 };

    while (idx < argc) {
        args.emplace_back(argv[idx++]);
    }
    args.emplace_back(nullptr);

    return args;
}

void print_child_status(int status, const std::string &pid) noexcept
{
    if (WIFEXITED(status)) {
        print_info("child " + pid + " exited with status " + std::to_string(WEXITSTATUS(status)));
    } else if (WIFSIGNALED(status)) {
        print_info("child " + pid + " exited with signal " + std::to_string(WTERMSIG(status)));
    } else {
        print_info("child " + pid + "  exited with unknown status");
    }
}

pid_t run(std::vector<const char *> args, const sigConf &conf) noexcept
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

        if (!conf.restore_signals()) {
            return -1;
        }

        ::execvp(args[0], const_cast<char *const *>(args.data()));
        print_sys_error("Failed to run process");
        ::_exit(EXIT_FAILURE);
    }

    return pid;
}

bool handle_sigevent(const file_descriptor_wrapper &sigfd,
                     pid_t child,
                     struct WaitPidResult &waitChild) noexcept
{
    while (true) {
        signalfd_siginfo info{};
        auto ret = ::read(sigfd, &info, sizeof(info));
        if (ret == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            print_sys_error("Failed to read from signalfd");
            return false;
        }

        if (info.ssi_signo != SIGCHLD) {
            auto ret = ::kill(child, info.ssi_signo);
            if (ret == -1) {
                auto msg = std::string("Failed to forward signal ") + ::strsignal(info.ssi_signo);
                print_sys_error(msg);
            }

            continue;
        }

        while (true) {
            int status{};
            auto ret = ::waitpid(-1, &status, WNOHANG);
            if (ret == 0 || (ret == -1 && errno == ECHILD)) {
                break;
            }

            if (ret == -1) {
                print_sys_error("Failed to wait for child");
                return false;
            }

            print_child_status(status, std::to_string(ret));

            if (ret == child) {
                waitChild.pid = child;
                waitChild.status = status;
            }
        }
    }

    return true;
}

bool shouldWait() noexcept
{
    std::error_code ec;
    auto proc_it = std::filesystem::directory_iterator{
        "/proc",
        std::filesystem::directory_options::skip_permission_denied,
        ec
    };

    if (ec) {
        print_sys_error("Failed to open /proc", ec);
        return false;
    }

    for (const auto &entry : proc_it) {
        if (!entry.is_directory(ec)) {
            continue;
        }

        if (ec) {
            print_sys_error("Failed to stat " + entry.path().string(), ec);
            return false;
        }

        pid_t pid{ -1 };
        try {
            pid = std::stoi(entry.path().filename());
        } catch (...) {
            continue;
        }

        if (pid == 1) { // ignore init process
            continue;
        }

        // if we get here, it means some process is still alive
        return true;
    }

    return false;
}

int handle_timerfdevent(const file_descriptor_wrapper &timerfd) noexcept
{
    // we don't care how many times we read from the timerfd
    // just traversal the /proc directory once
    while (true) {
        uint64_t expir{};
        auto ret = ::read(timerfd, &expir, sizeof(expir));
        if (ret == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            print_sys_error("Failed to read from timerfd");
            return -1;
        }
    }

    return shouldWait() ? 1 : 0;
}

file_descriptor_wrapper start_timer(const file_descriptor_wrapper &epfd) noexcept
{
    auto timerfd = create_timerfd();
    if (!timerfd) {
        return timerfd;
    }

    struct itimerspec timer_spec{};
    auto *interval = ::getenv("LINYAPS_INIT_WAIT_INTERVAL");
    constexpr auto default_interval{ 3 };
    if (interval != nullptr) {
        try {
            auto interval_int = std::stoi(interval);
            timer_spec.it_interval.tv_sec = interval_int;
            timer_spec.it_interval.tv_nsec = 0;
        } catch (...) {
            print_info("Invalid interval, using default 3 seconds");
            timer_spec.it_interval.tv_sec = default_interval;
            timer_spec.it_interval.tv_nsec = 0;
        }
    } else {
        timer_spec.it_interval.tv_sec = default_interval;
        timer_spec.it_interval.tv_nsec = 0;
    }
    timer_spec.it_value.tv_sec = default_interval;
    timer_spec.it_value.tv_nsec = 0;

    auto ret = ::timerfd_settime(timerfd, 0, &timer_spec, nullptr);
    if (ret == -1) {
        print_sys_error("Failed to set timerfd");
        return {};
    }

    struct epoll_event timer_ev{ .events = EPOLLIN | EPOLLET, .data = { .fd = timerfd } }; // NOLINT
    ret = ::epoll_ctl(epfd, EPOLL_CTL_ADD, timerfd, &timer_ev);
    if (ret == -1) {
        print_sys_error("Failed to add timerfd to epoll");
        return {};
    }

    return timerfd;
}

unsigned long get_arg_max() noexcept
{
    auto arg_max = sysconf(_SC_ARG_MAX);
    if (arg_max == -1) {
        return static_cast<unsigned long>(256 * 1024);
    }

    return arg_max - 4096;
}

int delegate_run(const std::vector<std::string> &args, const sigConf &conf) noexcept
{
    auto child = fork();
    if (child == -1) {
        print_sys_error("Failed to fork child");
        return -1;
    }

    if (child == 0) {
        std::vector<char *> c_args;
        c_args.reserve(args.size());
        for (const auto &arg : args) {
            c_args.emplace_back(const_cast<char *>(arg.c_str()));
        }
        c_args.emplace_back(nullptr);

        if (!conf.restore_signals()) {
            ::_exit(EXIT_FAILURE);
        }

        ::execvp(c_args[0], c_args.data());
        print_sys_error("Failed to exec");
        ::_exit(EXIT_FAILURE);
    }

    return 0;
}

bool handle_client(const file_descriptor_wrapper &unix_socket, const sigConf &conf) noexcept
{
    static const unsigned long arg_max = get_arg_max();

    const file_descriptor_wrapper client{ ::accept(unix_socket, nullptr, nullptr) };
    if (!client) {
        print_sys_error("Failed to accept client");
        return false;
    }

    const struct timeval tv{ 3, 0 };
    if (::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
        print_sys_error("Failed to set recv timeout");
        return false;
    }

    std::uint64_t arg_len{ 0 };
    auto ret = ::recv(client, &arg_len, sizeof(arg_len), 0);
    if (ret == -1) {
        print_sys_error("Failed to read from client");
        return false;
    }

    if (arg_len > arg_max) {
        print_info("Command too long");
        return false;
    }

    std::string command_buffer;
    command_buffer.reserve(arg_len);
    std::array<char, 4096> buffer{};
    while (true) {
        auto readBytes = ::recv(client, buffer.data(), buffer.size(), 0);
        if (readBytes < 0) {
            print_sys_error("Failed to read from client");
            return false;
        }

        command_buffer.append(buffer.data(), readBytes);

        if (command_buffer.size() >= arg_len) {
            break;
        }
    }

    if (command_buffer.empty()) {
        print_info("Empty command");
        return false;
    }

    if (command_buffer.back() != 0) {
        command_buffer.push_back(0);
    }

    std::vector<std::string> commands;
    auto start = command_buffer.cbegin();
    while (start != command_buffer.end()) {
        auto end{ start };
        while (end != command_buffer.end() && *end != 0) {
            ++end;
        }

        commands.emplace_back(start, end);
        start = end + 1;
    }

    if (commands.empty()) {
        print_info("Command may be invalid");
        return false;
    }

    ret = delegate_run(commands, conf);
    if (ret == -1) {
        print_sys_error("Failed to delegate command");
    }

    if (::send(client, &ret, sizeof(ret), 0) == -1) {
        print_sys_error("Failed to send result to client");
    }

    return true;
}

bool register_event(const file_descriptor_wrapper &epfd,
                    const file_descriptor_wrapper &fd,
                    epoll_event ev) noexcept
{
    auto ret = ::epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    if (ret == -1) {
        print_sys_error("Failed to add event to epoll");
        return false;
    }

    return true;
}

} // namespace

int main(int argc, char **argv) // NOLINT
{
    sigConf conf;
    if (!conf.block_signals()) {
        return -1;
    }

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

    auto *singleModeEnv = ::getenv("LINYAPS_INIT_SINGLE_MODE");
    const bool singleMode = singleModeEnv != nullptr && std::string_view{ singleModeEnv } == "1";

    auto child = run(args, conf);
    if (child == -1) {
        print_info("Failed to run child process");
        return -1;
    }

    auto epfd = create_epoll();
    if (!epfd) {
        return -1;
    }

    auto sigfd = create_signalfd(conf.current_sigset());
    if (!sigfd) {
        return -1;
    }

    const struct epoll_event ev{ .events = EPOLLIN | EPOLLET, .data = { .fd = sigfd } };
    if (!register_event(epfd, sigfd, ev)) {
        return -1;
    }

    file_descriptor_wrapper unix_socket;
    if (!singleMode) {
        unix_socket = create_fs_uds();
        if (!unix_socket) {
            return -1;
        }

        const struct epoll_event ev{ .events = EPOLLIN | EPOLLET,
                                     .data = { .fd = unix_socket } }; // NOLINT
        if (!register_event(epfd, unix_socket, ev)) {
            return -1;
        }
    }

    file_descriptor_wrapper timerfd;
    bool done{ false };
    std::array<struct epoll_event, 10> events{};
    WaitPidResult waitChild{ .pid = child, .status = 0 };
    int childExitCode = 0;
    while (true) {
        ret = ::epoll_wait(epfd, events.data(), events.size(), -1);
        if (ret == -1) {
            print_sys_error("Failed to wait for events");
            return -1;
        }

        for (auto i = 0; i < ret; ++i) {
            const auto event = events.at(i);
            if (event.data.fd == sigfd) {
                if (!handle_sigevent(sigfd, waitChild.pid, waitChild)) {
                    return -1;
                }

                if (waitChild.pid == child) {
                    // Init process will propagate received signals to all child processes (using
                    // pid -1) after initial child exits
                    if (WIFEXITED(waitChild.status)) {
                        waitChild.pid = -1;
                        childExitCode = WEXITSTATUS(waitChild.status);
                    } else if (WIFSIGNALED(waitChild.status)) {
                        waitChild.pid = -1;
                        childExitCode = 128 + WTERMSIG(waitChild.status);
                    }

                    if (!shouldWait()) {
                        done = true;
                    }
                    timerfd = start_timer(epfd);
                    if (!timerfd) {
                        return -1;
                    }
                }

                continue;
            }

            if (event.data.fd == timerfd) {
                ret = handle_timerfdevent(timerfd);
                if (ret == -1) {
                    return -1;
                }

                if (ret == 0) {
                    done = true;
                }

                continue;
            }

            if (unix_socket && event.data.fd == unix_socket) {
                if (handle_client(unix_socket, conf)) {
                    done = false;
                }
            }
        }

        if (done) {
            unix_socket.close();
            break;
        }
    }

    return childExitCode;
}
