// listen /tmp/rstdin
// write /tmp/rstdout

#include <QtConcurrent/QtConcurrentRun>
#include <QFile>
#include <QCoreApplication>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QtConcurrent::run([=]() {
        QFile inRedirect("/tmp/rstdin");
        inRedirect.open(QIODevice::WriteOnly | QIODevice::Append);
        printf("inRedirect.isOpen() %d\n", inRedirect.isOpen());
        for (std::string line; std::getline(std::cin, line);) {
            line += "\n";
            auto sz = inRedirect.write(line.data(), line.size());
            printf("inRedirect.write() %d\n", sz);
            inRedirect.flush();
        }
    });

    QtConcurrent::run([=]() {
        QFile out("/tmp/rstdout");
        out.open(QIODevice::ReadOnly);
        printf("out.isOpen() %d\n", out.isOpen());

        char buf[1024] = {0};
        do {
            auto sz = out.read(buf, 1024);
            if (sz < 0) {
                printf("child read exit %lld\n", sz);
                break;
            }
            if (sz > 0) {
                printf("write size %lld %s\n", sz, buf);
                std::cout << buf;
            }
        } while (true);
    });

    return QCoreApplication::exec();
}