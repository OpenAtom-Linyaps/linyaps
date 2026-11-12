/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/cli/error_presenter.h"

#include <set>

namespace linglong::cli {

namespace {

using utils::error::ErrorCode;

bool hasPresentation(ErrorCode code)
{
    if (commonPresentation(code)) {
        return true;
    }
    for (auto operation : { ErrorOperation::Install,
                            ErrorOperation::InstallFromFile,
                            ErrorOperation::Uninstall,
                            ErrorOperation::Upgrade }) {
        if (operationPresentation(operation, code)) {
            return true;
        }
    }
    return false;
}

// 没有专门提示语、只能把错误原样打给用户的码。加进来之前先想清楚：
// 是不是应该给它写一句提示（那要同时更新 po/ 里的翻译）。
const std::set<ErrorCode> &fallbackCodes()
{
    static const std::set<ErrorCode> codes = {
        ErrorCode::Failed,
        ErrorCode::Success,
        // 生产路径用的是 AppInstallNotFoundFromRemote(2002)，这个码只剩单测在用
        ErrorCode::AppNotFoundFromRemote,
        ErrorCode::AppNotFoundFromLocal,
        // 下面两个目前全仓库没有产生点，等补上服务端校验再接提示语
        ErrorCode::AppInstallArchNotMatch,
        // 产生点是 layer_packager / uab_file，CLI 侧没有对应提示
        ErrorCode::AppInstallErofsNotFound,
        // 客户端在 install 参数解析阶段就自己打印了提示，不走这里
        ErrorCode::AppInstallUnsupportedFileFormat,
        ErrorCode::InvalidFuzzyReference,
        ErrorCode::UnknownArchitecture,
    };
    return codes;
}

} // namespace

// 每个枚举值都必须有明确去处：要么在表里，要么登记到 fallbackCodes。
// 这个用例的用处是拦"加了新错误码但忘了加提示语"。
TEST(ErrorPresenter, EveryErrorCodeHasExplicitHandling)
{
    for (const auto code : allErrorCodes()) {
        const bool presented = hasPresentation(code);
        if (fallbackCodes().count(code) != 0) {
            EXPECT_FALSE(presented) << "错误码 " << static_cast<int>(code)
                                    << " 已经有提示语了，请从 fallbackCodes 里删掉";
        } else {
            EXPECT_TRUE(presented)
              << "错误码 " << static_cast<int>(code) << " 既没有提示语，也没有登记为 fallback";
        }
    }
}

// 新增枚举值时，allErrorCodes 里必须同步补上，否则上一个用例会漏掉它。
TEST(ErrorPresenter, AllErrorCodesCoversEveryEnumerator)
{
    // 枚举里的最后一个值，用来提醒"改了 enum 记得改这个用例"
    EXPECT_EQ(allErrorCodes().back(), ErrorCode::UnknownArchitecture);
    EXPECT_EQ(std::set<ErrorCode>(allErrorCodes().begin(), allErrorCodes().end()).size(),
              allErrorCodes().size())
      << "allErrorCodes 里出现了重复项";
}

// 提示语本身不能空，否则用户会看到一行空白。
TEST(ErrorPresenter, MessagesAreNotEmpty)
{
    for (auto operation : { ErrorOperation::Install,
                            ErrorOperation::InstallFromFile,
                            ErrorOperation::Uninstall,
                            ErrorOperation::Upgrade }) {
        for (const auto code : allErrorCodes()) {
            auto presentation = operationPresentation(operation, code);
            if (presentation) {
                EXPECT_FALSE(presentation->message.empty());
            }
        }
    }

    for (const auto code : allErrorCodes()) {
        auto presentation = commonPresentation(code);
        if (presentation) {
            EXPECT_FALSE(presentation->message.empty());
        }
    }
}

// 带 {} 占位符的提示必须声明参数来源，否则会原样把 "{}" 打给用户。
TEST(ErrorPresenter, PlaceholdersDeclareDetailSource)
{
    for (auto operation : { ErrorOperation::Install,
                            ErrorOperation::InstallFromFile,
                            ErrorOperation::Uninstall,
                            ErrorOperation::Upgrade }) {
        for (const auto code : allErrorCodes()) {
            auto presentation = operationPresentation(operation, code);
            if (!presentation) {
                continue;
            }
            if (presentation->message.find("{}") != std::string::npos) {
                EXPECT_NE(presentation->detail, DetailSource::None)
                  << "提示语里有占位符但没声明参数来源: " << presentation->message;
            }
        }
    }
}

} // namespace linglong::cli
