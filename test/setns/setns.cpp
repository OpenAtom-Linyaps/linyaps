
#include <sched.h>
#include <unistd.h>

#include <string>
#include <vector>
#include <fcntl.h>
#include <cstring>

int main(int argc, char *argv[])
{
    // sudo ./enter /proc/29272/ns/ "/run/user/1000/linglong/c5385fea-0ef2-11ec-99cc-60f262152f11"
    auto prefix = argv[1];
    auto root = argv[2];

    const std::string nsPrefix = std::string(prefix);
    const std::string containerRoot = std::string(root);

    std::vector<std::string> nsList = {
        "mnt",
        "ipc",
        "uts",
        "pid",
        "pid_for_children",
        "user",
    };

    for (auto const &ns : nsList) {
        int fd = open(std::string(nsPrefix + ns).c_str(), O_RDONLY);
        printf("open %s %d %d %s\n", std::string(nsPrefix + ns).c_str(), fd, errno, strerror(errno));
        int ret = setns(fd, 0);
        printf("setns %d %d %s\n", ret, errno, strerror(errno));
        close(fd);
    }

    chdir(root);
    chroot(root);

    setuid(1000);
    setgid(1000);

    char *eargv[] = {(char *)"/bin/bash", nullptr};
    printf("child getpid %d\n", getpid());
    int ret = execv("/bin/bash", eargv);
    printf("execve %d %d %s\n", ret, errno, strerror(errno));

    return 0;
}