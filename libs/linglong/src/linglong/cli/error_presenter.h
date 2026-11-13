// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <optional>
#include <string>
#include <vector>

namespace linglong::cli {

// 提示语里需要填的信息从哪儿来
enum class DetailSource {
    None,         // 纯静态提示
    PackageId,    // install 参数里的应用 id
    ErrorMessage, // 错误自身带的补充信息（如已安装的多版本列表）
};

struct ErrorPresentation
{
    std::string message; // 已翻译的提示文本，可能含一个 "{}"
    DetailSource detail{ DetailSource::None };
    bool viaNotifier{ false }; // 经桌面通知提示，而不是直接打印
};

// 一条命令的报错提示由「操作 + 错误码」决定；操作表没命中再退到通用表。
enum class ErrorOperation {
    Install,
    InstallFromFile,
    Uninstall,
    Upgrade,
};

// 操作专属的提示语（各操作提示不同，比如 install 的 "Install failed"）
std::optional<ErrorPresentation> operationPresentation(ErrorOperation operation,
                                                       utils::error::ErrorCode code);

// 各操作共用的兜底提示（网络、权限、取消、层不兼容）
std::optional<ErrorPresentation> commonPresentation(utils::error::ErrorCode code);

// 枚举里定义的全部错误码，供单测检查"每个码都有明确去处"
const std::vector<utils::error::ErrorCode> &allErrorCodes();

} // namespace linglong::cli
