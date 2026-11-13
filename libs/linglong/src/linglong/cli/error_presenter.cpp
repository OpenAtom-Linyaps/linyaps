// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/cli/error_presenter.h"

#include "linglong/utils/error/error.h"
#include "linglong/utils/gettext.h"

#include <array>
#include <cstddef>
#include <vector>

namespace linglong::cli {

namespace {

using utils::error::ErrorCode;

// 表里的一行：错误码 + 提示文本 + 文本里缺的信息从哪儿来 + 是否走桌面通知。
// 提示文本只存 `_()` 求值后的 msgid 指针，所以这一行是可平凡复制的字面量，
// 表本身也就没有堆分配，不必担心第一次构造时抛异常。
struct ErrorEntry
{
    ErrorCode code;
    const char *message; // 已翻译的提示文本，可能含一个 "{}"
    DetailSource detail{ DetailSource::None };
    bool viaNotifier{ false };
};

// 表在第一次查表时才构造：gettext 的 domain 在 main 里绑定，静态初始化阶段
// 取到的会是未翻译的原文。
//
// ⚠️ 提示文本必须和原来散在 handle*Error 里的 `_()` 串逐字一致，否则 po/ 里
//    已有的翻译会全部失效（xgettext 按 msgid 匹配）。改文案要单独走翻译流程。
//
// ⚠️ 这几张表是 `_()` 串的唯一来源，新增/移动这类文件时记得同步更新
//    po/POTFILES.in，否则 msgid 会从 .pot 里消失、已有翻译跟着作废。

const std::array<ErrorEntry, 9> &installMessages()
{
    static const std::array<ErrorEntry, 9> table = { {
      { ErrorCode::AppInstallModuleRequireAppFirst,
        _("To install the module, one must first install the app.") },
      { ErrorCode::AppInstallModuleAlreadyExists, _("Module is already installed.") },
      { ErrorCode::AppInstallModuleNotFound, _("The module could not be found remotely.") },
      { ErrorCode::AppInstallAlreadyInstalled, _("Application already installed") },
      { ErrorCode::AppInstallNotFoundFromRemote,
        _("Application {} is not found in remote repo."),
        DetailSource::PackageId },
      { ErrorCode::AppInstallModuleNoVersion,
        _("Cannot specify a version when installing a module.") },
      { ErrorCode::AppInstallNeedDowngrade,
        _("The latest version has been installed. If you want to "
          "replace it, try using 'll-cli install {} --force'"),
        DetailSource::PackageId },
      { ErrorCode::AppInstallFailed, _("Install failed") },
      { ErrorCode::Unknown, _("Install failed") },
    } };
    return table;
}

const std::array<ErrorEntry, 1> &installFromFileMessages()
{
    static const std::array<ErrorEntry, 1> table = { {
      { ErrorCode::AppInstallAlreadyInstalled, _("Application already installed") },
    } };
    return table;
}

const std::array<ErrorEntry, 6> &uninstallMessages()
{
    static const std::array<ErrorEntry, 6> table = { {
      { ErrorCode::AppUninstallAppIsRunning,
        _("The application is currently running and cannot be "
          "uninstalled. Please turn off the application and try again."),
        DetailSource::None,
        true },
      { ErrorCode::AppUninstallNotFoundFromLocal, _("Application is not installed.") },
      { ErrorCode::AppUninstallMultipleVersions,
        _("Multiple versions of the package are installed. Please specify a single "
          "version to uninstall:\n{}"),
        DetailSource::ErrorMessage },
      { ErrorCode::AppUninstallBaseOrRuntime,
        _("Base or runtime cannot be uninstalled, please use 'll-cli prune'.") },
      { ErrorCode::AppUninstallFailed, _("Uninstall failed") },
      { ErrorCode::Unknown, _("Uninstall failed") },
    } };
    return table;
}

const std::array<ErrorEntry, 3> &upgradeMessages()
{
    static const std::array<ErrorEntry, 3> table = { {
      { ErrorCode::AppUpgradeLocalNotFound, _("Application is not installed.") },
      { ErrorCode::AppUpgradeFailed, _("Upgrade failed") },
      { ErrorCode::Unknown, _("Upgrade failed") },
    } };
    return table;
}

const std::array<ErrorEntry, 4> &commonMessages()
{
    static const std::array<ErrorEntry, 4> table = { {
      { ErrorCode::NetworkError,
        _("Network connection failed. Please:"
          "\n1. Check your internet connection"
          "\n2. Verify network proxy settings if used") },
      { ErrorCode::LayerCompatibilityError, _("Package not found") },
      { ErrorCode::Canceled, _("Operation canceled") },
      { ErrorCode::PermissionDenied, _("Permission denied, authentication is required") },
    } };
    return table;
}

// 表都很小（最多 9 行），线性查一遍比建哈希表更省事。
template <std::size_t N>
std::optional<ErrorPresentation> lookup(const std::array<ErrorEntry, N> &table, ErrorCode code)
{
    for (const auto &entry : table) {
        if (entry.code == code) {
            return ErrorPresentation{ entry.message, entry.detail, entry.viaNotifier };
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<ErrorPresentation> operationPresentation(ErrorOperation operation, ErrorCode code)
{
    switch (operation) {
    case ErrorOperation::Install:
        return lookup(installMessages(), code);
    case ErrorOperation::InstallFromFile:
        return lookup(installFromFileMessages(), code);
    case ErrorOperation::Uninstall:
        return lookup(uninstallMessages(), code);
    case ErrorOperation::Upgrade:
        return lookup(upgradeMessages(), code);
    }

    return std::nullopt;
}

std::optional<ErrorPresentation> commonPresentation(ErrorCode code)
{
    return lookup(commonMessages(), code);
}

const std::vector<ErrorCode> &allErrorCodes()
{
    static const std::vector<ErrorCode> codes = {
        ErrorCode::Failed,
        ErrorCode::Success,
        ErrorCode::Canceled,
        ErrorCode::PermissionDenied,
        ErrorCode::Unknown,
        ErrorCode::AppNotFoundFromRemote,
        ErrorCode::AppNotFoundFromLocal,
        ErrorCode::AppInstallFailed,
        ErrorCode::AppInstallNotFoundFromRemote,
        ErrorCode::AppInstallAlreadyInstalled,
        ErrorCode::AppInstallNeedDowngrade,
        ErrorCode::AppInstallModuleNoVersion,
        ErrorCode::AppInstallModuleRequireAppFirst,
        ErrorCode::AppInstallModuleAlreadyExists,
        ErrorCode::AppInstallArchNotMatch,
        ErrorCode::AppInstallModuleNotFound,
        ErrorCode::AppInstallErofsNotFound,
        ErrorCode::AppInstallUnsupportedFileFormat,
        ErrorCode::AppUninstallFailed,
        ErrorCode::AppUninstallNotFoundFromLocal,
        ErrorCode::AppUninstallAppIsRunning,
        ErrorCode::LayerCompatibilityError,
        ErrorCode::AppUninstallMultipleVersions,
        ErrorCode::AppUninstallBaseOrRuntime,
        ErrorCode::AppUpgradeFailed,
        ErrorCode::AppUpgradeLocalNotFound,
        ErrorCode::NetworkError,
        ErrorCode::InvalidFuzzyReference,
        ErrorCode::UnknownArchitecture,
    };
    return codes;
}

} // namespace linglong::cli
