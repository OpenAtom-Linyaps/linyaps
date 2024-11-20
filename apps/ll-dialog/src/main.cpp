#include "linglong/utils/configure.h"
#include "permission.h"

#include <QApplication>
#include <QWidget>

#include <iostream>

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::ApplicationAttribute::AA_UseHighDpiPixmaps);
    QApplication::setAttribute(Qt::ApplicationAttribute::AA_EnableHighDpiScaling);
#endif
    QApplication app{ argc, argv };
    QApplication::setApplicationName("ll-dialog");
    QApplication::setApplicationVersion(LINGLONG_VERSION);

    linglong::api::types::v1::RequiredPermissions perms;
    try {
        auto required = nlohmann::json::parse(std::cin, nullptr, false);
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
