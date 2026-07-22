// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/State.hpp"
#include "linglong/utils/error/error.h"

#include <gio/gio.h>

#include <mutex>

namespace linglong::service {

class TaskReporter
{
public:
    virtual ~TaskReporter() = default;
    virtual void onProgress() noexcept = 0;
    virtual void onStateChanged() noexcept = 0;
    virtual void onStateMessageChanged() noexcept = 0;
    virtual void onDataArrived(uint arrived) noexcept = 0;
    virtual void onHandled(uint handled, uint total) noexcept = 0;
    virtual void onMessage(const std::string &message) noexcept = 0;
};

using ProgressReporter = std::function<void(double)>;

class Task
{
public:
    struct StateSnapshot
    {
        api::types::v1::State state;
        std::string message;
        utils::error::ErrorCode code;
        double percentage;
    };

    Task(std::function<void(Task &)> job = {});
    Task(Task &&other) noexcept;
    Task &operator=(Task &&other) noexcept;
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

    virtual void resetProgress(std::optional<std::string> message = std::nullopt);
    virtual void updateProgress(double percentage,
                                std::optional<std::string> message = std::nullopt);
    virtual void updateState(linglong::api::types::v1::State state, const std::string &message);
    virtual void updateStateMessage(const std::string &message) noexcept;
    virtual void reportError(linglong::utils::error::Error &&err) noexcept;
    virtual void reportDataArrived(uint arrived) noexcept;
    virtual void reportDataHandled(uint handled, uint total) noexcept;
    virtual void sendMessage(const std::string &message) noexcept;

    [[nodiscard]] static bool isDoneState(api::types::v1::State state) noexcept;
    [[nodiscard]] virtual bool isTaskDone() const noexcept;

    [[nodiscard]] StateSnapshot stateSnapshot() const noexcept;

    virtual GCancellable *cancellable() noexcept { return nullptr; }

    [[nodiscard]] std::string taskID() const noexcept { return m_taskID; }

    [[nodiscard]] linglong::api::types::v1::State state() const noexcept;

    void setState(linglong::api::types::v1::State newState) noexcept;

    [[nodiscard]] utils::error::ErrorCode code() const noexcept;

    void setCode(utils::error::ErrorCode code) noexcept;

    [[nodiscard]] std::string message() const noexcept;

    void setMessage(const std::string &message) noexcept;

    double percentage() const noexcept;

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
    mutable std::mutex m_stateMutex;
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

    void updateStateMessage(const std::string &message) noexcept override
    {
        m_owner.get().updateStateMessage(message);
    }

    void reportError(linglong::utils::error::Error &&err) noexcept override
    {
        m_owner.get().reportError(std::move(err));
    }

    void sendMessage(const std::string &message) noexcept override
    {
        m_owner.get().sendMessage(message);
    }

    void reportDataArrived(uint arrived) noexcept override
    {
        m_owner.get().reportDataArrived(arrived);
    }

    void reportDataHandled(uint handled, uint total) noexcept override
    {
        m_owner.get().reportDataHandled(handled, total);
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

    void onStateMessageChanged() noexcept override { }

    void onDataArrived([[maybe_unused]] uint arrived) noexcept override { }

    void onHandled([[maybe_unused]] uint handled, [[maybe_unused]] uint total) noexcept override { }

    void onMessage(const std::string &) noexcept override { }

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
