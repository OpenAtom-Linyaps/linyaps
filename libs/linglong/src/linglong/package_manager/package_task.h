// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/State.hpp"
#include "linglong/api/types/v1/SubState.hpp"
#include "linglong/package/reference.h"
#include "linglong/utils/error/error.h"

#include <gio/gio.h>

#include <QMap>
#include <QObject>
#include <QString>
#include <QUuid>

#include <functional>
#include <optional>

namespace linglong::service {

class PackageTask : public QObject
{
    Q_OBJECT
public:
    using State = linglong::api::types::v1::State;
    using SubState = linglong::api::types::v1::SubState;

    explicit PackageTask(const package::Reference &ref,
                         const QString &module,
                         QObject *parent = nullptr);
    explicit PackageTask(const package::Reference &ref,
                         const std::string &module,
                         QObject *parent = nullptr);
    PackageTask(PackageTask &&other) noexcept;
    PackageTask &operator=(PackageTask &&other) noexcept;
    ~PackageTask() override;

    static PackageTask createTemporaryTask() noexcept;

    friend bool operator==(const PackageTask &lhs, const PackageTask &rhs)
    {
        return lhs.m_layer == rhs.m_layer;
    }

    friend bool operator!=(const PackageTask &lhs, const PackageTask &rhs) { return !(lhs == rhs); }

    void updateTask(uint part, uint whole, const QString &message = "") noexcept;
    void updateState(State newState, const QString &message = "") noexcept;
    void updateSubState(SubState newSubState, const QString &message = "") noexcept;
    void reportError(linglong::utils::error::Error &&err) noexcept;

    [[nodiscard]] State state() const noexcept { return m_state; }

    [[nodiscard]] SubState subState() const noexcept { return m_subState; }

    [[nodiscard]] utils::error::Error &&takeError() && noexcept { return std::move(m_err); }

    [[nodiscard]] QString taskID() const noexcept
    {
        return m_taskID.toString(QUuid::Id128);
    }

    [[nodiscard]] QString taskObjectPath() const noexcept
    {
        return "/org/deepin/linglong/Task1/" + taskID();
    }

    void cancelTask() noexcept;

    auto cancellable() noexcept { return m_cancelFlag; }

    [[nodiscard]] QString layer() const noexcept { return m_layer; }

    auto getJob() { return m_job; }

    void setJob(std::function<void()> job) { m_job = job; };

    [[nodiscard]] double currentPercentage(double increase = 0) noexcept;

Q_SIGNALS:
    void TaskChanged(QString taskObjectPath, QString message, QPrivateSignal);
    void PartChanged(QString percentage, QPrivateSignal);

private:
    PackageTask();
    State m_state = State::Queued;
    SubState m_subState = SubState::Unknown;
    utils::error::Error m_err;
    double m_curPercentage{ 0 };
    QUuid m_taskID;
    QString m_layer;
    GCancellable *m_cancelFlag{ nullptr };
    std::optional<std::function<void()>> m_job;

    inline static QMap<SubState, double> m_subStateMap{ { SubState::PreAction, 10 },
                                                        // install
                                                        { SubState::InstallBase, 30 },
                                                        { SubState::InstallRuntime, 30 },
                                                        { SubState::InstallApplication, 20 },
                                                        // exportReference
                                                        { SubState::PostAction, 10 },
                                                        // unexportReference
                                                        { SubState::PreRemove, 10 },
                                                        { SubState::Uninstall, 30 } };
};

} // namespace linglong::service
