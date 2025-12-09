// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "task.h"

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <uuid.h>

namespace linglong::service {

Task::Task(std::function<void(Task &)> job)
    : m_job(std::move(job))
{
    uuid_t uuid;
    uuid_generate_random(uuid);
    m_taskID = fmt::format("{}", fmt::join(uuid, ""));
}

void Task::updateProgress(double percentage, std::optional<std::string> message)
{
    if (percentage < 0 || percentage > 100) {
        return;
    }

    if (percentage > m_percentage) {
        m_percentage = percentage;

        if (message) {
            updateMessage(std::move(message).value());
        }

        if (m_reporter != nullptr) {
            m_reporter->onProgress();
        }
    }
}

void Task::updateState(linglong::api::types::v1::State newState, const std::string &message)
{
    m_state = newState;
    m_message = message;

    if (m_reporter != nullptr) {
        m_reporter->onStateChanged();
    }
}

void Task::reportError(linglong::utils::error::Error &&err) noexcept
{
    m_state = linglong::api::types::v1::State::Failed;
    m_message = err.message();
    m_code = static_cast<utils::error::ErrorCode>(err.code());

    if (m_reporter != nullptr) {
        m_reporter->onStateChanged();
    }
}

void Task::reportDataHandled(uint handled, uint total) noexcept
{
    if (m_reporter != nullptr) {
        m_reporter->onHandled(handled, total);
    }
}

void Task::updateMessage(const std::string &message) noexcept
{
    setMessage(message);

    if (m_reporter != nullptr) {
        m_reporter->onMessage();
    }
}

bool Task::isTaskDone() const noexcept
{
    return m_state == linglong::api::types::v1::State::Canceled
      || m_state == linglong::api::types::v1::State::Failed
      || m_state == linglong::api::types::v1::State::Succeed;
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
