// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/State.hpp"
#include "linglong/utils/error/error.h"

#include <gio/gio.h>

namespace linglong::service {

class TaskReporter
{
public:
    virtual ~TaskReporter() = default;
    virtual void onProgress() noexcept = 0;
    virtual void onStateChanged() noexcept = 0;
    virtual void onHandled(uint handled, uint total) noexcept = 0;
    virtual void onMessage() noexcept = 0;
};

using ProgressReporter = std::function<void(double)>;

class Task
{
public:
    Task(std::function<void(Task &)> job = {});
    Task(Task &&) = default;
    Task &operator=(Task &&) = default;
    Task(const Task &) = delete;
    Task &operator=(const Task &) = delete;
    virtual ~Task() = default;

    void setReporter(TaskReporter *reporter) { m_reporter = reporter; }

    virtual void run() noexcept
    {
        if (m_job) {
            m_job(*this);
        }
    }

    virtual void updateProgress(double percentage,
                                std::optional<std::string> message = std::nullopt);
    virtual void updateState(linglong::api::types::v1::State state, const std::string &message);
    virtual void reportError(linglong::utils::error::Error &&err) noexcept;
    virtual void reportDataHandled(uint handled, uint total) noexcept;
    virtual void updateMessage(const std::string &message) noexcept;

    [[nodiscard]] virtual bool isTaskDone() const noexcept;

    virtual GCancellable *cancellable() noexcept { return nullptr; }

    [[nodiscard]] std::string taskID() const noexcept { return m_taskID; }

    [[nodiscard]] linglong::api::types::v1::State state() const noexcept { return m_state; }

    void setState(linglong::api::types::v1::State newState) noexcept { m_state = newState; }

    [[nodiscard]] utils::error::ErrorCode code() const noexcept { return m_code; }

    void setCode(utils::error::ErrorCode code) noexcept { m_code = code; }

    [[nodiscard]] std::string message() const noexcept { return m_message; }

    void setMessage(const std::string &message) noexcept { m_message = message; }

    double percentage() const noexcept { return m_percentage; }

private:
    std::string m_taskID;
    std::function<void(Task &)> m_job;

    // progress
    double m_percentage{ 0 };

    TaskReporter *m_reporter{ nullptr };

    // status
    api::types::v1::State m_state{ api::types::v1::State::Queued };
    // last message
    std::string m_message;
    // last error code
    utils::error::ErrorCode m_code{ utils::error::ErrorCode::Unknown };
};

class TaskPart : public Task
{
public:
    explicit TaskPart(Task &owner)
        : m_owner(owner)
    {
    }

    TaskPart(TaskPart &&) = default;
    TaskPart &operator=(TaskPart &&) = default;
    TaskPart(const TaskPart &) = delete;
    TaskPart &operator=(const TaskPart &) = delete;
    ~TaskPart() override = default;

    GCancellable *cancellable() noexcept override { return m_owner.get().cancellable(); }

    void updateState(linglong::api::types::v1::State newState,
                     const std::string &message) noexcept override
    {
        m_owner.get().updateState(newState, message);
    }

    void reportError(linglong::utils::error::Error &&err) noexcept override
    {
        m_owner.get().reportError(std::move(err));
    }

    void updateMessage(const std::string &message) noexcept override
    {
        m_owner.get().updateMessage(message);
    }

    [[nodiscard]] bool isTaskDone() const noexcept override { return m_owner.get().isTaskDone(); }

private:
    std::reference_wrapper<Task> m_owner;
};

// TaskPart in TaskContainer is used for report progress only,
// task state and error will be handled by owner
class TaskContainer : public TaskReporter
{
public:
    TaskContainer(Task &owner, int count);
    TaskContainer(Task &owner, std::vector<int> weight);

    TaskContainer(TaskContainer &&other) = delete;
    TaskContainer &operator=(TaskContainer &&other) = delete;
    TaskContainer(const TaskContainer &other) = delete;
    TaskContainer &operator=(const TaskContainer &other) = delete;
    ~TaskContainer() override;

    [[nodiscard]] bool hasNext() const;
    Task &next();

    [[nodiscard]] double percentage() const noexcept;

private:
    void onProgress() noexcept override;

    void onStateChanged() noexcept override { }

    void onHandled([[maybe_unused]] uint handled, [[maybe_unused]] uint total) noexcept override { }

    void onMessage() noexcept override { }

    [[nodiscard]] double ownerPercentage() const noexcept;

    Task &m_owner;
    std::vector<int> m_weight;
    std::vector<Task *> m_parts;
    int m_index = -1;
    int m_totalWeight = 0;
    int m_doneWeight = 0;
    double m_curPartWeight = 0;
    double m_totalPercentage = 0;
};

} // namespace linglong::service
