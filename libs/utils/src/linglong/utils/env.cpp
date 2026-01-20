/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "env.h"

#include "linglong/common/error.h"
#include "linglong/utils/log/log.h"

namespace linglong::utils {

EnvironmentVariableGuard::EnvironmentVariableGuard(std::string variableName,
                                                   const std::string &newValue)
    : m_variableName(std::move(variableName))
    , m_originalValue(getOriginalValue())
{
    if (::setenv(m_variableName.c_str(), newValue.c_str(), 1) != 0) {
        LogE("Failed to set environment variable {}: {}",
             m_variableName,
             common::error::errorString(errno));
    }
}

EnvironmentVariableGuard::~EnvironmentVariableGuard()
{
    restoreOriginalValue();
}

std::optional<std::string> EnvironmentVariableGuard::getOriginalValue() const
{
    const char *original = std::getenv(m_variableName.c_str());
    if (original != nullptr) {
        return std::string(original);
    }
    return std::nullopt;
}

void EnvironmentVariableGuard::restoreOriginalValue()
{
    if (m_originalValue) {
        // Restore the original value
        if (::setenv(m_variableName.c_str(), m_originalValue->c_str(), 1) != 0) {
            LogE("Failed to restore environment variable {}: {}",
                 m_variableName,
                 common::error::errorString(errno));
        }
    } else {
        // Unset the variable if it didn't exist originally
        if (::unsetenv(m_variableName.c_str()) != 0) {
            LogE("Failed to unset environment variable {}: {}",
                 m_variableName,
                 common::error::errorString(errno));
        }
    }
}

} // namespace linglong::utils
