// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

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
    explicit InstallTask(const QUuid &taskID, QObject *parent = nullptr);
    ~InstallTask() override;

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

    void updateTask(double currentPercentage,
                    double totalPercentage,
                    const QString &message = "") noexcept;
    void updateStatus(Status newStatus, const QString &message = "") noexcept;
    void updateStatus(Status newStatus, linglong::utils::error::Result<void>) noexcept;

    [[nodiscard]] Status currentStatus() const noexcept { return m_status; }

    [[nodiscard]] utils::error::Result<void> *currentError() const noexcept { return m_err.data(); }

    [[nodiscard]] QString taskID() const noexcept
    {
        return m_taskID.toString(QUuid::WithoutBraces);
    }

    void cancelTask() noexcept;

    auto cancellable() noexcept { return m_cancelFlag; }

Q_SIGNALS:
    void
    TaskChanged(QString taskID, QString percentage, QString message, Status status, QPrivateSignal);
    void
    PartChanged(QString taskID, QString percentage, QString message, Status status, QPrivateSignal);

private:
    QString formatPercentage(double increase = 0) const noexcept;
    Status m_status{ Queued };
    QSharedPointer<utils::error::Result<void>> m_err;
    double m_statePercentage{ 0 };
    QUuid m_taskID;
    GCancellable *m_cancelFlag{ nullptr };

    inline static QMap<Status, double> partsMap{ { Queued, 0 },       { Canceled, 0 },
                                                 { preInstall, 10 },  { installRuntime, 20 },
                                                 { installBase, 20 }, { installApplication, 20 },
                                                 { postInstall, 20 }, { Success, 10 },
                                                 { Failed, 10 } };
};

} // namespace linglong::service
