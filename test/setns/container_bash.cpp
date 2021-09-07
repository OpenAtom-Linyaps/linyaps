// listen /tmp/rstdin
// write /tmp/rstdout

#include <unistd.h>
#include <sys/wait.h>
#include <QtConcurrent/QtConcurrentRun>
#include <QFile>

int main()
{
    pid_t parent2child[2], child2parent[2];
    pipe(parent2child);
    pipe(child2parent);

    int ppid = fork();

    if (ppid < 0) {
        return -1;
    }

    if (ppid == 0) {
        // child
        printf("child %d\n", ppid);
        close(parent2child[1]);
        close(child2parent[0]);
        dup2(parent2child[0], STDIN_FILENO);
        dup2(child2parent[1], STDOUT_FILENO);

        char *argv[] = {(char *)"/bin/bash", nullptr};
        printf("child getpid %d\n", getpid());
        int ret = execv("/bin/bash", argv);
        printf("execve %d %d %s\n", ret, errno, strerror(errno));

        return 0;
    }
    // parent
    printf("parent %d\n", ppid);
    close(parent2child[0]);
    close(child2parent[1]);

    // write to parent2child[1], read from child2parent[0]
    QtConcurrent::run([=]() {
        QFile in("/tmp/rstdin");
        in.open(QIODevice::ReadOnly);
        printf("in.isOpen() %d\n", in.isOpen());

        char buf[1024] = {0};
        do {
            auto sz = in.read(buf, 1024);
            if (sz < 0) {
                printf("read exit %lld\n", sz);
                break;
            }
            if (sz > 0) {
                printf("read size %lld\n", sz);
                write(parent2child[1], buf, sz);
            }
        } while (true);
    });

    QtConcurrent::run([=]() {
        QFile out("/tmp/rstdout");
        out.open(QIODevice::WriteOnly | QIODevice::Append);
        printf("out.isOpen() %d\n", out.isOpen());

        char buf[1024] = {0};
        do {
            auto sz = read(child2parent[0], buf, 1024);
            if (sz < 0) {
                printf("child read exit %ld\n", sz);
                break;
            }
            if (sz > 0) {
                printf("write size %ld %s\n", sz, buf);
                sz = out.write(buf, sz);
                out.flush();
                printf("write size %ld\n", sz);
            }
        } while (true);
    });

    waitpid(-1, nullptr, 0);
}