#include "linglong/utils/configure.h"
#include "permission.h"

#include <sys/timerfd.h>

#include <QApplication>
#include <QSocketNotifier>
#include <QTimer>
#include <QWidget>

#include <chrono>
#include <iostream>

#include <fcntl.h>
#include <unistd.h>

constexpr std::string_view exchange_data_version = "1";

bool ensureCallerCompatible()
{
    constexpr decltype(auto) specVersionPrefix = "CALLER SPECVERSION";
    using namespace std::chrono_literals;
    std::string line;

    QEventLoop loop;

    QSocketNotifier notifier(STDIN_FILENO, QSocketNotifier::Read);
    QTimer timer;
    timer.setInterval(4s);
    QObject::connect(&timer, &QTimer::timeout, [&loop] {
        std::cerr << "TIMEOUT" << std::endl;
        loop.exit(-1);
    });
    QObject::connect(&notifier, &QSocketNotifier::activated, [&line, &loop] {
        std::getline(std::cin, line);
        loop.exit(0);
    });
    timer.start();

    if (loop.exec() != 0) {
        return false;
    }

    if (line.rfind(specVersionPrefix, 0) != 0) { // NOLINT
        std::cerr << "INVALID DATA" << std::endl;
        return false;
    }

    if (line.substr(sizeof(specVersionPrefix)) != exchange_data_version) {
        std::cerr << "INCOMPATIBLE CALLEE" << std::endl;
        return false;
    }

    std::cout << "READY" << std::endl;
    return true;
}

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::ApplicationAttribute::AA_UseHighDpiPixmaps);
    QApplication::setAttribute(Qt::ApplicationAttribute::AA_EnableHighDpiScaling);
#endif
    QApplication app{ argc, argv };
    QApplication::setApplicationName("ll-dialog");
    QApplication::setApplicationVersion(LINGLONG_VERSION);

    if (!ensureCallerCompatible()) {
        return -1;
    }

    linglong::api::types::v1::RequiredPermissions perms;
    try {
        auto required = nlohmann::json::parse(std::cin);
        auto content = required.get<linglong::api::types::v1::RequiredPermissions>();
        perms = std::move(content);
    } catch (const nlohmann::json::parse_error &e) {
        std::cerr << "failed to parse input content:" << e.what() << std::endl;
        return -1;
    } catch (std::exception &e) {
        std::cerr << "caught exception:" << e.what() << std::endl;
        return -1;
    }

    PermissionDialog dialog{ perms };
    dialog.show();

    return QApplication::exec();
}
