#include "ocppi/cli/Process.hpp"

#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

int runProcess(const std::string &binaryPath,
               const std::vector<std::string> &args, std::string &output)
{
        constexpr auto bufferSize = 1024;
        auto cArgs = std::make_unique<const char *[]>(args.size() + 2);

        cArgs[0] = binaryPath.data();
        for (std::size_t i = 1; i <= args.size(); ++i) {
                cArgs[i] = args[i - 1].data();
        }
        cArgs[args.size() + 1] = nullptr;

        int pipes[2];
        if (::pipe(pipes) == -1) {
                throw std::system_error(errno, std::generic_category(), "pipe");
        }

        pid_t childId{ -1 };
        childId = fork();
        if (childId == -1) {
                throw std::system_error(errno, std::generic_category(), "fork");
        }

        int ret{ 0 };
        if (childId == 0) {
                ::close(pipes[0]);
                ::dup2(pipes[1], 1);

                ret = execvp(binaryPath.data(), (char **)(cArgs.get()));
                throw std::system_error(errno, std::generic_category(),
                                        "execvp");
        }

        ::close(pipes[1]);
        std::array<char, bufferSize> buffer{};

        ssize_t readCount{ 0 };
        while (true) {
                readCount = read(pipes[0], buffer.data(), bufferSize);
                if (readCount == -1) {
                        if (errno == EINTR) {
                                continue;
                        }

                        throw std::system_error(errno, std::generic_category(),
                                                "read");
                }

                if (readCount == 0) {
                        break;
                }

                output.append(buffer.data(), readCount);
        }

        int interruptTimes = 0;
        while (true) {
                if (::wait(&ret) != -1) {
                        break;
                }

                if (errno == EINTR) {
                        interruptTimes < 2 ? ++interruptTimes :
                                             kill(childId, SIGKILL);
                        continue;
                }

                throw std::system_error(errno, std::generic_category(),
                                        "wait: " + std::to_string(errno));
        }

        ::close(pipes[0]);
        return ret;
}

int runProcess(const std::string &binaryPath,
               const std::vector<std::string> &args)
{
        auto cArgs = std::make_unique<const char *[]>(args.size() + 2);

        cArgs[0] = binaryPath.data();
        for (std::size_t i = 1; i <= args.size(); ++i) {
                cArgs[i] = args[i - 1].data();
        }
        cArgs[args.size() + 1] = nullptr;

        pid_t childId{ -1 };
        childId = fork();
        if (childId == -1) {
                throw std::system_error(errno, std::generic_category(), "fork");
        }

        int ret{ 0 };
        if (childId == 0) {
                ret = execvp(binaryPath.data(), (char **)(cArgs.get()));
                throw std::system_error(errno, std::generic_category(),
                                        "execvp");
        }

        int interruptTimes = 0;
        while (true) {
                if (::wait(&ret) != -1) {
                        break;
                }

                if (errno == EINTR) {
                        interruptTimes < 2 ? ++interruptTimes :
                                             kill(childId, SIGKILL);
                        continue;
                }

                throw std::system_error(errno, std::generic_category(),
                                        "wait: " + std::to_string(errno));
        }

        return ret;
}
