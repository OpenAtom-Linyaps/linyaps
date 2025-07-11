/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/error/error.h"

#include <QStringList>

#include <optional>

namespace linglong::utils::command {

error::Result<QString> Exec(const QString &command, const QStringList &args) noexcept;
QStringList getUserEnv(const QStringList &filters);

class EnvironmentVariableGuard
{
public:
    /**
     * @brief Constructs the guard, saving the original value and setting the new one.
     * @param variableName The name of the environment variable to manage.
     * @param newValue The new value to set for the environment variable.
     */
    EnvironmentVariableGuard(std::string variableName, const std::string &newValue)
        : m_variableName(std::move(variableName))
        , m_originalValue(getOriginalValue())
    {
        if (::setenv(m_variableName.c_str(), newValue.c_str(), 1) != 0) {
            qCritical() << "Failed to set environment variable"
                        << QString::fromStdString(m_variableName) << "Error:" << errno
                        << strerror(errno);
        }
    }

    ~EnvironmentVariableGuard() { restoreOriginalValue(); }

    EnvironmentVariableGuard(const EnvironmentVariableGuard &) = delete;
    EnvironmentVariableGuard &operator=(const EnvironmentVariableGuard &) = delete;
    EnvironmentVariableGuard(EnvironmentVariableGuard &&) = delete;
    EnvironmentVariableGuard &operator=(EnvironmentVariableGuard &&) = delete;

private:
    std::optional<std::string> getOriginalValue() const
    {
        const char *original = std::getenv(m_variableName.c_str());
        if (original != nullptr) {
            return std::string(original);
        }
        return std::nullopt;
    }

    void restoreOriginalValue()
    {
        if (m_originalValue) {
            // Restore the original value
            if (::setenv(m_variableName.c_str(), m_originalValue->c_str(), 1) != 0) {
                qCritical() << "Failed to restore environment variable"
                            << QString::fromStdString(m_variableName) << "Error:" << errno
                            << strerror(errno);
            }
        } else {
            // Unset the variable if it didn't exist originally
            if (::unsetenv(m_variableName.c_str()) != 0) {
                qCritical() << "Failed to unset environment variable"
                            << QString::fromStdString(m_variableName) << "Error:" << errno
                            << strerror(errno);
            }
        }
    }

    std::string m_variableName;
    std::optional<std::string> m_originalValue;
};

} // namespace linglong::utils::command
