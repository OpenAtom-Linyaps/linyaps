// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <fmt/format.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>

#include <array>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

constexpr std::array unblock_signals{ SIGABRT, SIGBUS,  SIGFPE,  SIGILL, SIGSEGV,
                                      SIGSYS,  SIGTRAP, SIGXCPU, SIGXFSZ };

constexpr auto containerLockPath = "/run/linglong/.lock";

namespace {

void print_sys_error(std::string_view msg, int error) noexcept
{
    fmt::println(stderr, "{}: {}", msg, ::strerror(error));
}

void print_sys_error(std::string_view msg) noexcept
{
    print_sys_error(msg, errno);
}

void print_info(std::string_view msg) noexcept
{
    static const auto is_debug = ::getenv("LINYAPS_INIT_VERBOSE_OUTPUT") != nullptr;
    if (is_debug) {
        fmt::println(stderr, msg);
    }
}

void print_child_status(int status, pid_t pid) noexcept
{
    if (WIFEXITED(status)) {
        print_info(fmt::format("child {} exited with status {}", pid, WEXITSTATUS(status)));
    } else if (WIFSIGNALED(status)) {
        print_info(fmt::format("child {} exited with signal {}", pid, WTERMSIG(status)));
    } else {
        print_info(fmt::format("child {} exited with unknown status {}", pid, status));
    }
}

class UniqueFd
{
public:
    explicit UniqueFd(int fd) noexcept
        : fd_(fd)
    {
    }

    UniqueFd() noexcept = default;

    UniqueFd(const UniqueFd &) = delete;
    UniqueFd &operator=(const UniqueFd &) = delete;

    UniqueFd(UniqueFd &&other) noexcept
        : fd_(other.fd_)
    {
        other.fd_ = -1;
    }

    UniqueFd &operator=(UniqueFd &&other) noexcept
    {
        if (this != &other) {
            reset();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    ~UniqueFd() noexcept { reset(); }

    void reset() noexcept
    {
        if (fd_ != -1) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    [[nodiscard]] int get() const noexcept { return fd_; }

    [[nodiscard]] explicit operator bool() const noexcept { return fd_ != -1; }

private:
    int fd_{ -1 };
};

class SignalMask
{
public:
    explicit SignalMask() noexcept
    {
        ::sigfillset(&blocked_);
        for (auto sig : unblock_signals) {
            ::sigdelset(&blocked_, sig);
        }

        if (::sigprocmask(SIG_SETMASK, &blocked_, &original_) == -1) {
            print_sys_error("failed to set signal mask");
            return;
        }

        valid_ = true;
    }

    ~SignalMask() noexcept
    {
        if (valid_) {
            ::sigprocmask(SIG_SETMASK, &original_, nullptr);
        }
    }

    SignalMask(const SignalMask &) = delete;
    SignalMask &operator=(const SignalMask &) = delete;
    SignalMask(SignalMask &&) = delete;
    SignalMask &operator=(SignalMask &&) = delete;

    [[nodiscard]] bool valid() const noexcept { return valid_; }

    [[nodiscard]] const sigset_t &blocked_set() const noexcept { return blocked_; }

    [[nodiscard]] bool restore() const noexcept
    {
        if (::sigprocmask(SIG_SETMASK, &original_, nullptr) == -1) {
            print_sys_error("failed to restore signal mask");
            return false;
        }

        return true;
    }

private:
    sigset_t blocked_{ };
    sigset_t original_{ };
    bool valid_{ false };
};

class Epoll
{
public:
    static Epoll create() noexcept
    {
        auto fd = ::epoll_create1(0);
        if (fd == -1) {
            print_sys_error("failed to create epoll");
        }
        return Epoll(UniqueFd(fd));
    }

    bool add(int fd, uint32_t events) noexcept
    {
        struct epoll_event ev{ .events = events, .data = { .fd = fd } };
        if (::epoll_ctl(fd_.get(), EPOLL_CTL_ADD, fd, &ev) == -1) {
            print_sys_error("failed to add event to epoll");
            return false;
        }
        return true;
    }

    int wait() noexcept
    {
        auto n = ::epoll_wait(fd_.get(), events_.data(), static_cast<int>(events_.size()), -1);
        if (n == -1) {
            if (errno == EINTR) {
                return 0;
            }
            print_sys_error("failed to wait for events");
            return -1;
        }
        n_ready_ = n;
        return n;
    }

    [[nodiscard]] const epoll_event *events() const noexcept { return events_.data(); }

    [[nodiscard]] int events_count() const noexcept { return n_ready_; }

    [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(fd_); }

    Epoll(Epoll &&) noexcept = default;
    Epoll &operator=(Epoll &&) noexcept = default;
    Epoll(const Epoll &) = delete;
    Epoll &operator=(const Epoll &) = delete;
    ~Epoll() noexcept = default;

private:
    explicit Epoll(UniqueFd fd) noexcept
        : fd_(std::move(fd))
    {
    }

    UniqueFd fd_;
    std::array<epoll_event, 4> events_{ };
    int n_ready_{ 0 };
};

class ChildProcess;

class SignalMonitor
{
public:
    static SignalMonitor create(const sigset_t &mask) noexcept
    {
        auto fd = ::signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
        if (fd == -1) {
            print_sys_error("failed to create signalfd");
        }
        return SignalMonitor(UniqueFd(fd));
    }

    bool dispatch(ChildProcess &child) noexcept;

    [[nodiscard]] int fd() const noexcept { return fd_.get(); }

    [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(fd_); }

    SignalMonitor(SignalMonitor &&) noexcept = default;
    SignalMonitor &operator=(SignalMonitor &&) noexcept = default;
    SignalMonitor(const SignalMonitor &) = delete;
    SignalMonitor &operator=(const SignalMonitor &) = delete;
    ~SignalMonitor() noexcept = default;

private:
    explicit SignalMonitor(UniqueFd fd) noexcept
        : fd_(std::move(fd))
    {
    }

    UniqueFd fd_;
};

class ChildProcess
{
public:
    static ChildProcess spawn(const std::vector<const char *> &args,
                              const SignalMask &mask) noexcept
    {
        auto pid = ::fork();
        if (pid == -1) {
            print_sys_error("Failed to fork");
            return ChildProcess{ };
        }

        if (pid == 0) {
            if (::setpgid(0, 0) == -1) {
                print_sys_error("failed to set process group");
                return ChildProcess{ };
            }

            if (::tcsetpgrp(0, ::getpid()) == -1 && errno != ENOTTY) {
                print_sys_error("failed to set terminal process group");
                return ChildProcess{ };
            }

            if (!mask.restore()) {
                return ChildProcess{ };
            }

            ::execvp(args[0], const_cast<char *const *>(args.data()));
            print_sys_error("failed to run process");
            ::_exit(EXIT_FAILURE);
        }

        print_info(fmt::format("run child {}", pid));
        return ChildProcess{ pid };
    }

    void forward_signal(int signo) const noexcept
    {
        if (pid_ < 0) {
            return;
        }

        if (::kill(pid_, signo) == -1) {
            print_sys_error(fmt::format("failed to forward signal {}", ::strsignal(signo)));
        }
    }

    void reap_pending() noexcept
    {
        while (true) {
            int status{ };
            auto ret = ::waitpid(-1, &status, WNOHANG);
            if (ret == 0 || (ret == -1 && errno == ECHILD)) {
                break;
            }

            if (ret == -1) {
                print_sys_error("failed to wait for child");
                break;
            }

            print_child_status(status, ret);

            if (ret == pid_) {
                pid_ = -1;
            }
        }
    }

    [[nodiscard]] static bool has_zombies() noexcept
    {
        while (true) {
            int status{ };
            auto ret = ::waitpid(-1, &status, WNOHANG);
            if (ret == -1) {
                if (errno == EINTR) {
                    continue;
                }

                if (errno == ECHILD) {
                    return false;
                }

                print_sys_error("waitpid failed");
                return true;
            }

            if (ret > 0) {
                print_child_status(status, ret);
            }

            return true;
        }
    }

    [[nodiscard]] bool is_alive() const noexcept { return pid_ > 0; }

    [[nodiscard]] bool has_exited() const noexcept { return pid_ == -1; }

    [[nodiscard]] explicit operator bool() const noexcept { return pid_ != 0; }

    ChildProcess(ChildProcess &&other) noexcept
        : pid_(other.pid_)
    {
        other.pid_ = 0;
    }

    ChildProcess &operator=(ChildProcess &&other) noexcept
    {
        if (this != &other) {
            pid_ = other.pid_;
            other.pid_ = 0;
        }

        return *this;
    }

    ChildProcess(const ChildProcess &) = delete;
    ChildProcess &operator=(const ChildProcess &) = delete;
    ~ChildProcess() noexcept = default;

private:
    ChildProcess() noexcept = default;

    explicit ChildProcess(pid_t pid) noexcept
        : pid_(pid)
    {
    }

    pid_t pid_{ 0 };
};

bool SignalMonitor::dispatch(ChildProcess &child) noexcept
{
    while (true) {
        signalfd_siginfo info{ };
        auto ret = ::read(fd_.get(), &info, sizeof(info));
        if (ret == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            print_sys_error("failed to read from signalfd");
            return false;
        }

        if (info.ssi_signo != SIGCHLD) {
            child.forward_signal(static_cast<int>(info.ssi_signo));
            continue;
        }

        child.reap_pending();
    }

    return true;
}

bool file_lock(int fd, bool blocked, short type = F_WRLCK) noexcept
{
    struct flock fl{ };
    fl.l_type = type;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    auto cmd = blocked ? F_SETLKW : F_SETLK;
    return ::fcntl(fd, cmd, &fl) != -1;
}

bool file_unlock(int fd) noexcept
{
    struct flock fl{ };
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    return ::fcntl(fd, F_SETLK, &fl) != -1;
}

bool overwrite_file(int fd, std::string_view content) noexcept
{
    if (::ftruncate(fd, 0) == -1) {
        print_sys_error("failed to truncate file");
        return false;
    }

    if (::lseek(fd, 0, SEEK_SET) == -1) {
        print_sys_error("failed to seek to beginning of file");
        return false;
    }

    while (true) {
        auto written = ::write(fd, content.data(), content.size());
        if (written == -1) {
            if (errno == EINTR) {
                continue;
            }

            print_sys_error("failed to write to file");
            return false;
        }

        break;
    }

    return true;
}

std::string read_file(int fd) noexcept
{
    if (fd < 0) {
        return { };
    }

    std::string content;
    std::array<char, 32> buffer{ };

    while (true) {
        auto bytes = ::read(fd, buffer.data(), buffer.size());
        if (bytes < 0) {
            if (errno == EINTR) {
                continue;
            }

            print_sys_error(fmt::format("failed to read from fd {}", fd));
            return { };
        }

        if (bytes == 0) {
            break;
        }

        content.append(buffer.data(), static_cast<std::size_t>(bytes));
    }

    return content;
}

class ContainerLock
{
public:
    enum class State : uint8_t {
        Error,
        Skipped,
        Active,
    };

    static ContainerLock acquire() noexcept
    {
        if (should_skip()) {
            print_info("skipping container lock");
            return ContainerLock{ State::Skipped };
        }

        auto lockFd = ::open(containerLockPath, O_RDWR | O_CLOEXEC);
        if (lockFd == -1) {
            print_sys_error(fmt::format("failed to open lock file {}", containerLockPath));
            return ContainerLock{ State::Error };
        }

        ContainerLock result{ State::Active, UniqueFd(lockFd) };
        if (!file_lock(result.fd_.get(), true)) {
            print_sys_error("failed to lock container lock during initializing");
            return ContainerLock{ State::Error };
        }

        auto content = read_file(result.fd_.get());
        if (content.empty()) {
            return ContainerLock{ State::Error };
        }

        if (content != "initializing") {
            print_info(
              fmt::format("container is not in expected initializing state, current state: {}",
                          content));
            return ContainerLock{ State::Error };
        }

        return result;
    }

    bool transition_to_running() noexcept
    {
        if (state_ != State::Active) {
            return state_ == State::Skipped;
        }

        if (!overwrite_file(fd_.get(), "running")) {
            print_info("failed to update lock state");
            return false;
        }

        if (!file_unlock(fd_.get())) {
            print_sys_error("failed to unlock lock file");
            return false;
        }

        return true;
    }

    bool transition_to_quitting() noexcept
    {
        if (state_ != State::Active) {
            return state_ == State::Skipped;
        }

        if (!file_lock(fd_.get(), false)) {
            if (errno != EAGAIN && errno != EACCES) {
                print_sys_error("failed to lock container lock during exiting");
            }
            return false;
        }

        if (!overwrite_file(fd_.get(), "quitting")) {
            print_info("failed to update lock state");
        }

        if (!file_unlock(fd_.get())) {
            print_sys_error("failed to unlock container lock");
        }

        return true;
    }

    [[nodiscard]] bool is_error() const noexcept { return state_ == State::Error; }

    ContainerLock(ContainerLock &&) noexcept = default;
    ContainerLock &operator=(ContainerLock &&) noexcept = default;
    ContainerLock(const ContainerLock &) = delete;
    ContainerLock &operator=(const ContainerLock &) = delete;
    ~ContainerLock() noexcept = default;

private:
    explicit ContainerLock(State state) noexcept
        : state_(state)
    {
    }

    ContainerLock(State state, UniqueFd fd) noexcept
        : fd_(std::move(fd))
        , state_(state)
    {
    }

    static bool should_skip() noexcept
    {
        static auto skip = [] {
            std::error_code ec;
            auto exist = std::filesystem::exists(containerLockPath, ec);
            if (ec) {
                print_sys_error(
                  "failed to detect lock exists or not, maybe container state has already broken");
                return true;
            }

            std::string env;
            if (auto *str = ::getenv("LINYAPS_INIT_SKIP_LOCK"); str != nullptr) {
                env.assign(str);
            } else {
                env = "NO";
            }

            return env == "YES" || !exist;
        }();

        return skip;
    }

    UniqueFd fd_;
    State state_{ State::Error };
};

std::vector<const char *> parse_args(int argc, char *argv[]) noexcept
{
    std::vector<const char *> args;
    args.reserve(static_cast<std::size_t>(argc));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    args.emplace_back(nullptr);
    return args;
}

int delegate_run(int argc, char **argv) noexcept
{
    print_info("delegate run");

    auto stdin_is_tty = ::isatty(STDIN_FILENO) == 1;
    auto args = parse_args(argc, argv);
    if (args.empty()) {
        print_info("no arguments provided for delegate run");
        return 0;
    }

    int tty_fd{ -1 };
    if (stdin_is_tty) {
        tty_fd = ::fcntl(STDIN_FILENO, F_DUPFD_CLOEXEC, 0);
        if (tty_fd < 0) {
            print_sys_error("failed dup tty");
            return -1;
        }
    }

    sigset_t new_mask;
    sigset_t old_mask;
    sigemptyset(&new_mask);
    sigaddset(&new_mask, SIGHUP);
    ::sigprocmask(SIG_BLOCK, &new_mask, &old_mask);

    int sync_pipe[2];
    if (::pipe2(sync_pipe, O_CLOEXEC) < 0) {
        print_sys_error("failed to create sync pipe");

        if (tty_fd >= 0) {
            ::close(tty_fd);
        }

        ::sigprocmask(SIG_SETMASK, &old_mask, nullptr);
        return -1;
    }

    auto child = ::fork();
    if (child == -1) {
        print_sys_error("failed to fork for delegate run");
        ::close(sync_pipe[0]);
        ::close(sync_pipe[1]);

        if (tty_fd >= 0) {
            ::close(tty_fd);
        }

        ::sigprocmask(SIG_SETMASK, &old_mask, nullptr);
        return -1;
    }

    auto parent = getpid();
    if (child == 0) {
        ::close(sync_pipe[1]);
        std::byte dummy{ };
        while (::read(sync_pipe[0], &dummy, sizeof(dummy)) > 0) { }
        ::close(sync_pipe[0]);

        while (::getppid() == parent) { }

        if (::setsid() == -1) {
            print_sys_error("setsid failed");
            _exit(EXIT_FAILURE);
        }

        ::sigprocmask(SIG_SETMASK, &old_mask, nullptr);
        ::signal(SIGHUP, SIG_DFL);
        ::signal(SIGTTIN, SIG_DFL);
        ::signal(SIGTTOU, SIG_DFL);
        ::signal(SIGTSTP, SIG_DFL);

        // using namespace std::chrono_literals;
        // std::this_thread::sleep_for(1ms);

        if (stdin_is_tty && tty_fd >= 0) {
            if (::ioctl(tty_fd, TIOCSCTTY, 0) == -1) {
                print_sys_error("TIOCSCTTY failed");
            }

            ::tcsetpgrp(tty_fd, ::getpid());

            ::dup2(tty_fd, STDIN_FILENO);
            ::dup2(tty_fd, STDOUT_FILENO);
            ::dup2(tty_fd, STDERR_FILENO);
            ::close(tty_fd);
        }

        ::execvp(args[0], const_cast<char *const *>(args.data()));
        print_sys_error("failed to exec for delegate run");
        ::_exit(EXIT_FAILURE);
    }

    ::close(sync_pipe[0]);

    if (ioctl(STDIN_FILENO, TIOCNOTTY, 0) < 0) {
        print_sys_error("failed detach terminal");
        return -1;
    }

    if (tty_fd >= 0) {
        ::close(tty_fd);
    }

    ::close(sync_pipe[1]);
    return 0;
}

int run_init(int argc, char **argv) noexcept
{
    const SignalMask sigmask;
    if (!sigmask.valid()) {
        return -1;
    }

    auto lock = ContainerLock::acquire();
    if (lock.is_error()) {
        return -1;
    }

    if (::prctl(PR_SET_CHILD_SUBREAPER, 1) == -1) {
        print_sys_error("failed to set child subreaper");
        return -1;
    }

    auto args = parse_args(argc, argv);
    if (args.empty()) {
        print_info("no arguments provided");
        return -1;
    }

    auto child = ChildProcess::spawn(args, sigmask);
    if (!child) {
        print_info("failed to run child process");
        return -1;
    }

    auto epoll = Epoll::create();
    if (!epoll) {
        return -1;
    }

    auto monitor = SignalMonitor::create(sigmask.blocked_set());
    if (!monitor) {
        return -1;
    }

    if (!epoll.add(monitor.fd(), EPOLLIN | EPOLLET)) {
        return -1;
    }

    if (!lock.transition_to_running()) {
        return -1;
    }

    while (true) {
        auto n = epoll.wait();
        if (n < 0) {
            return -1;
        }

        if (n == 0) {
            continue;
        }

        for (int i = 0; i < epoll.events_count(); ++i) {
            const auto &ev = epoll.events()[i];
            if (ev.data.fd != monitor.fd()) {
                continue;
            }

            if (!monitor.dispatch(child)) {
                return -1;
            }

            if (child.has_exited() && !ChildProcess::has_zombies()) {
                if (lock.transition_to_quitting()) {
                    return 0;
                }
            }
        }
    }
}

} // namespace

int main(int argc, char **argv) // NOLINT
{
    if (::getpid() != 1) {
        return delegate_run(argc, argv);
    }

    return run_init(argc, argv);
}
