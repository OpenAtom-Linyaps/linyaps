/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/error/error.h"

#include <optional>

namespace linglong::utils {

class EnvironmentVariableGuard
{
public:
    /**
     * @brief Constructs the guard, saving the original value and setting the new one.
     * @param variableName The name of the environment variable to manage.
     * @param newValue The new value to set for the environment variable.
     */
    EnvironmentVariableGuard(std::string variableName, const std::string &newValue);
    ~EnvironmentVariableGuard();

    EnvironmentVariableGuard(const EnvironmentVariableGuard &) = delete;
    EnvironmentVariableGuard &operator=(const EnvironmentVariableGuard &) = delete;
    EnvironmentVariableGuard(EnvironmentVariableGuard &&) = delete;
    EnvironmentVariableGuard &operator=(EnvironmentVariableGuard &&) = delete;

private:
    std::optional<std::string> getOriginalValue() const;
    void restoreOriginalValue();

    std::string m_variableName;
    std::optional<std::string> m_originalValue;
};

} // namespace linglong::utils
