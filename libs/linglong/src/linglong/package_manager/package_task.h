// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/State.hpp"
#include "linglong/api/types/v1/SubState.hpp"
#include "linglong/utils/dbus/properties_forwarder.h"
#include "linglong/utils/error/error.h"

#include <gio/gio.h>

#include <QDBusContext>
#include <QDBusObjectPath>
#include <QMap>
#include <QObject>
#include <QString>
#include <QUuid>

#include <functional>
#include <optional>

Q_DECLARE_METATYPE(linglong::api::types::v1::State)
Q_DECLARE_METATYPE(linglong::api::types::v1::SubState)

namespace linglong::service {

class PackageTask : public QObject, protected QDBusContext
{
    Q_OBJECT
public:
    Q_PROPERTY(int State MEMBER m_state NOTIFY StateChanged)
    Q_PROPERTY(int SubState MEMBER m_subState NOTIFY SubStateChanged)
    Q_PROPERTY(double Percentage READ getPercentage NOTIFY PercentageChanged)
    Q_PROPERTY(QString Message MEMBER m_message NOTIFY MessageChanged)

    explicit PackageTask(QDBusConnection connection, QStringList refs, QObject *parent = nullptr);
    PackageTask(PackageTask &&other) = delete;
    PackageTask &operator=(PackageTask &&other) = delete;
    ~PackageTask() override;

    static PackageTask createTemporaryTask() noexcept;
    std::unique_ptr<PackageTask> creatSubTask(double partOfTotal) noexcept;

    friend bool operator==(const PackageTask &lhs, const PackageTask &rhs)
    {
        return lhs.m_refs == rhs.m_refs;
    }

    friend bool operator!=(const PackageTask &lhs, const PackageTask &rhs) { return !(lhs == rhs); }

    void updateTask(uint part, uint whole, const QString &message) noexcept;
    void
    updateState(linglong::api::types::v1::State newState,
                const QString &message,
                std::optional<linglong::api::types::v1::SubState> optDone = std::nullopt) noexcept;
    void updateSubState(linglong::api::types::v1::SubState newSubState,
                        const QString &message) noexcept;
    void reportError(linglong::utils::error::Error &&err) noexcept;

    [[nodiscard]] utils::error::Error &&takeError() && noexcept { return std::move(m_err); }

    [[nodiscard]] linglong::api::types::v1::State state() const noexcept
    {
        return static_cast<linglong::api::types::v1::State>(m_state);
    }

    void setState(linglong::api::types::v1::State newState) noexcept
    {
        m_state = static_cast<int>(newState);
    }

    [[nodiscard]] linglong::api::types::v1::SubState subState() const noexcept
    {
        return static_cast<linglong::api::types::v1::SubState>(m_subState);
    }

    void setSubState(linglong::api::types::v1::SubState newSubState) noexcept
    {
        m_subState = static_cast<int>(newSubState);
    }

    [[nodiscard]] QString message() const noexcept { return m_message; }

    void setMessage(const QString &message) noexcept { m_message = message; }

    [[nodiscard]] QString taskID() const noexcept { return m_taskID.toString(QUuid::Id128); }

    [[nodiscard]] QString taskObjectPath() const noexcept
    {
        return "/org/deepin/linglong/Task1/" + taskID();
    }

    auto cancellable() noexcept { return m_cancelFlag; }

    bool isRefExist(const QString &ref) const noexcept { return m_refs.contains(ref); }

    auto getJob() { return m_job; }

    void setJob(std::function<void()> job) { m_job = job; };

    [[nodiscard]] double getPercentage() const noexcept
    {
        if (m_subState == static_cast<int>(linglong::api::types::v1::SubState::AllDone)
            || m_subState
              == static_cast<int>(linglong::api::types::v1::SubState::PackageManagerDone)) {
            return 100;
        }

        return m_totalPercentage
          + (m_curStagePercentage
             * m_subStateMap[static_cast<api::types::v1::SubState>(m_subState)]);
    };

public Q_SLOTS:
    void Cancel() noexcept;

Q_SIGNALS:
    void StateChanged(int newState);
    void SubStateChanged(int newSubState);
    void PercentageChanged(double newPercentage);
    void MessageChanged(QString newMessage);
    void PartChanged(QString percentage, QPrivateSignal);

private:
    PackageTask();
    int m_state{ static_cast<int>(linglong::api::types::v1::State::Queued) };
    int m_subState{ static_cast<int>(linglong::api::types::v1::SubState::Unknown) };
    utils::error::Error m_err;
    double m_totalPercentage{ 0 };
    double m_curStagePercentage{ 0 };
    QString m_message;
    QUuid m_taskID;
    QStringList m_refs;
    uint m_taskParts{ 0 };
    GCancellable *m_cancelFlag{ nullptr };
    std::optional<std::function<void()>> m_job;
    utils::dbus::PropertiesForwarder *m_forwarder{ nullptr };

    inline static QMap<linglong::api::types::v1::SubState, double> m_subStateMap{
        { linglong::api::types::v1::SubState::PreAction, 10 },
        // install
        { linglong::api::types::v1::SubState::InstallBase, 20 },
        { linglong::api::types::v1::SubState::InstallRuntime, 20 },
        { linglong::api::types::v1::SubState::InstallApplication, 20 },
        // uninstall
        { linglong::api::types::v1::SubState::Uninstall, 80 },
        // export/unexportReference
        { linglong::api::types::v1::SubState::PostAction, 5 },
    };

    void changePropertiesDone() const noexcept;
};

} // namespace linglong::service
