// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "task.h"

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <uuid.h>

namespace linglong::service {

bool Task::isDoneState(api::types::v1::State state) noexcept
{
    return state == api::types::v1::State::Canceled || state == api::types::v1::State::Failed
      || state == api::types::v1::State::Succeed;
}

Task::Task(std::function<void(Task &)> job)
    : m_job(std::move(job))
{
    uuid_t uuid;
    uuid_generate_random(uuid);
    m_taskID = fmt::format("{}", fmt::join(uuid, ""));
}

Task::Task(Task &&other) noexcept
{
    std::lock_guard lock(other.m_stateMutex);
    m_taskID = std::move(other.m_taskID);
    m_job = std::move(other.m_job);
    m_percentage = other.m_percentage;
    m_reporter = other.m_reporter;
    m_state = other.m_state;
    m_message = std::move(other.m_message);
    m_code = other.m_code;
}

Task &Task::operator=(Task &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    std::scoped_lock lock(m_stateMutex, other.m_stateMutex);
    m_taskID = std::move(other.m_taskID);
    m_job = std::move(other.m_job);
    m_percentage = other.m_percentage;
    m_reporter = other.m_reporter;
    m_state = other.m_state;
    m_message = std::move(other.m_message);
    m_code = other.m_code;
    return *this;
}

void Task::resetProgress(std::optional<std::string> message)
{
    {
        std::lock_guard lock(m_stateMutex);
        if (isDoneState(m_state)) {
            return;
        }
        m_percentage = 0;
        if (message) {
            m_message = std::move(message).value();
        }
    }

    if (m_reporter != nullptr) {
        m_reporter->onProgress();
    }
}

void Task::updateProgress(double percentage, std::optional<std::string> message)
{
    if (percentage < 0 || percentage > 100) {
        return;
    }

    {
        std::lock_guard lock(m_stateMutex);
        if (isDoneState(m_state) || percentage <= m_percentage) {
            return;
        }
        m_percentage = percentage;
        if (message) {
            m_message = std::move(message).value();
        }
    }

    if (m_reporter != nullptr) {
        m_reporter->onProgress();
    }
}

void Task::updateState(linglong::api::types::v1::State newState, const std::string &message)
{
    {
        std::lock_guard lock(m_stateMutex);
        if (isDoneState(m_state)) {
            return;
        }
        m_state = newState;
        m_message = message;
    }

    if (m_reporter != nullptr) {
        m_reporter->onStateChanged();
    }
}

void Task::updateStateMessage(const std::string &message) noexcept
{
    {
        std::lock_guard lock(m_stateMutex);
        if (isDoneState(m_state)) {
            return;
        }
        m_message = message;
    }

    if (m_reporter != nullptr) {
        m_reporter->onStateMessageChanged();
    }
}

void Task::reportError(linglong::utils::error::Error &&err) noexcept
{
    {
        std::lock_guard lock(m_stateMutex);
        if (isDoneState(m_state)) {
            return;
        }
        m_state = linglong::api::types::v1::State::Failed;
        m_message = err.message();
        m_code = static_cast<utils::error::ErrorCode>(err.code());
    }

    if (m_reporter != nullptr) {
        m_reporter->onStateChanged();
    }
}

void Task::reportDataArrived(uint arrived) noexcept
{
    if (m_reporter != nullptr) {
        m_reporter->onDataArrived(arrived);
    }
}

void Task::reportDataHandled(uint handled, uint total) noexcept
{
    if (m_reporter != nullptr) {
        m_reporter->onHandled(handled, total);
    }
}

void Task::sendMessage(const std::string &message) noexcept
{
    if (m_reporter != nullptr) {
        m_reporter->onMessage(message);
    }
}

bool Task::isTaskDone() const noexcept
{
    std::lock_guard lock(m_stateMutex);
    return isDoneState(m_state);
}

Task::StateSnapshot Task::stateSnapshot() const noexcept
{
    std::lock_guard lock(m_stateMutex);
    return {
        .state = m_state,
        .message = m_message,
        .code = m_code,
        .percentage = m_percentage,
    };
}

api::types::v1::State Task::state() const noexcept
{
    std::lock_guard lock(m_stateMutex);
    return m_state;
}

void Task::setState(api::types::v1::State newState) noexcept
{
    std::lock_guard lock(m_stateMutex);
    m_state = newState;
}

utils::error::ErrorCode Task::code() const noexcept
{
    std::lock_guard lock(m_stateMutex);
    return m_code;
}

void Task::setCode(utils::error::ErrorCode code) noexcept
{
    std::lock_guard lock(m_stateMutex);
    m_code = code;
}

std::string Task::message() const noexcept
{
    std::lock_guard lock(m_stateMutex);
    return m_message;
}

void Task::setMessage(const std::string &message) noexcept
{
    std::lock_guard lock(m_stateMutex);
    m_message = message;
}

double Task::percentage() const noexcept
{
    std::lock_guard lock(m_stateMutex);
    return m_percentage;
}

TaskContainer::TaskContainer(Task &owner, int count)
    : TaskContainer(owner, std::vector(count, 1))
{
}

TaskContainer::TaskContainer(Task &owner, std::vector<int> weight)
    : m_owner(owner)
    , m_weight(std::move(weight))
    , m_totalWeight(std::accumulate(m_weight.begin(), m_weight.end(), 0))
    , m_totalPercentage(100 - owner.percentage())
{
    m_parts.reserve(m_weight.size());
    for (size_t i = 0; i < m_weight.size(); ++i) {
        auto &part = m_parts.emplace_back(new TaskPart(owner));
        part->setReporter(this);
    }
}

TaskContainer::~TaskContainer()
{
    for (size_t i = 0; i < m_weight.size(); ++i) {
        delete m_parts[i];
    }
}

bool TaskContainer::hasNext() const
{
    return m_index + 1 < static_cast<int>(m_weight.size());
}

Task &TaskContainer::next()
{
    if (hasNext()) {
        if (m_index >= 0) {
            m_doneWeight += m_weight[m_index];
            m_curPartWeight = 0;
            m_owner.updateProgress(ownerPercentage());
        }
        m_index++;
    }

    return *m_parts[m_index];
}

void TaskContainer::onProgress() noexcept
{
    if (m_index >= 0 && static_cast<size_t>(m_index) < m_parts.size()) {
        m_curPartWeight = m_parts[m_index]->percentage() * m_weight[m_index] / 100;
        m_owner.updateProgress(ownerPercentage());
    }
}

double TaskContainer::percentage() const noexcept
{
    return (m_doneWeight + m_curPartWeight) / m_totalWeight * 100;
}

double TaskContainer::ownerPercentage() const noexcept
{
    return (100 - m_totalPercentage) + (percentage() / 100 * m_totalPercentage);
}

} // namespace linglong::service
