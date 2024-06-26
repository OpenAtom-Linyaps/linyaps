// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/package/reference.h"
#include "linglong/utils/error/error.h"

#include <gio/gio.h>

#include <QMap>
#include <QObject>
#include <QString>
#include <QUuid>

namespace linglong::service {

class InstallTask : public QObject
{
    Q_OBJECT
public:
    explicit InstallTask(const package::Reference &ref,
                         const QString &module,
                         QObject *parent = nullptr);
    explicit InstallTask(const package::Reference &ref,
                         const std::string &module,
                         QObject *parent = nullptr);
    InstallTask(InstallTask &&other) noexcept;
    InstallTask &operator=(InstallTask &&other) noexcept;
    ~InstallTask() override;

    static InstallTask createTemporaryTask() noexcept;

    enum Status {
        Queued,
        preInstall,
        installRuntime,
        installBase,
        installApplication,
        postInstall,
        Success,
        Failed,
        Canceled
    };
    Q_ENUM(Status)

    friend bool operator==(const InstallTask &lhs, const InstallTask &rhs)
    {
        return lhs.m_layer == rhs.m_layer;
    }

    friend bool operator!=(const InstallTask &lhs, const InstallTask &rhs) { return !(lhs == rhs); }

    void updateTask(double currentPercentage,
                    double totalPercentage,
                    const QString &message = "") noexcept;
    void updateStatus(Status newStatus, const QString &message = "") noexcept;
    void updateStatus(Status newStatus, linglong::utils::error::Error) noexcept;

    [[nodiscard]] Status currentStatus() const noexcept { return m_status; }

    [[nodiscard]] utils::error::Error currentError() && noexcept { return std::move(m_err); }

    [[nodiscard]] QString taskID() const noexcept
    {
        return m_taskID.toString(QUuid::WithoutBraces);
    }

    void cancelTask() noexcept;

    auto cancellable() noexcept { return m_cancelFlag; }

    [[nodiscard]] const QString &layer() const noexcept { return m_layer; }

Q_SIGNALS:
    void
    TaskChanged(QString taskID, QString percentage, QString message, Status status, QPrivateSignal);
    void
    PartChanged(QString taskID, QString percentage, QString message, Status status, QPrivateSignal);

private:
    InstallTask();
    [[nodiscard]] QString formatPercentage(double increase = 0) const noexcept;
    Status m_status{ Queued };
    utils::error::Error m_err;
    double m_statePercentage{ 0 };
    QUuid m_taskID;
    QString m_layer;
    GCancellable *m_cancelFlag{ nullptr };

    inline static QMap<Status, double> partsMap{ { Queued, 0 },       { Canceled, 0 },
                                                 { preInstall, 10 },  { installRuntime, 20 },
                                                 { installBase, 20 }, { installApplication, 20 },
                                                 { postInstall, 20 }, { Success, 10 },
                                                 { Failed, 10 } };
};

} // namespace linglong::service
