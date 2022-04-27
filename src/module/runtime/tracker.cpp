/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tracker.h"

#include <QProcess>
#include <QFile>
#include <QStandardPaths>
#include <unistd.h>
#include <sys/wait.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <QDir>

#include "module/util/yaml.h"
#include "module/util/uuid.h"
#include "module/util/json.h"

class TrackerPrivate
{
public:
    explicit TrackerPrivate(Tracker *parent)
        : q_ptr(parent)
    {
    }

    Runtime *r = nullptr;
    Tracker *q_ptr = nullptr;
    Q_DECLARE_PUBLIC(Tracker);
};

Tracker::Tracker(QObject *parent)
    : JsonSerialize(parent)
    , dd_ptr(new TrackerPrivate(this))
{
}

Tracker::~Tracker() = default;

int Tracker::start()
{
    //    Q_D(Tracker);
    //    QString containerRoot = ensureContainerPath();
    //    d->r->root->path = containerRoot;
    //
    //    d->prepare();
    //
    //    // write pid file
    //    QFile pidFile(containerRoot + QString("/%1.pid").arg(getpid()));
    //    pidFile.open(QIODevice::WriteOnly);
    //    pidFile.close();
    //
    //    qDebug() << "start container at" << containerRoot;
    //    auto json = QJsonDocument::fromVariant(toVariant<Runtime>(d->r)).toJson();
    //    auto data = json.toStdString();
    //
    //    int pipeEnds[2];
    //    if (pipe(pipeEnds) != 0) {
    //        return EXIT_FAILURE;
    //    }
    //
    //    prctl(PR_SET_PDEATHSIG, SIGKILL);
    //
    //    pid_t boxPid = fork();
    //    if (0 == boxPid) {
    //        // child process
    //        (void)close(pipeEnds[1]);
    //        if (dup2(pipeEnds[0], LINGLONG) == -1) {
    //            return EXIT_FAILURE;
    //        }
    //        (void)close(pipeEnds[0]);
    //        char const *const args[] = {"/usr/bin/ll-box", LL_TOSTRING(LINGLONG), NULL};
    //        execvp(args[0], (char **)args);
    //    } else {
    //        close(pipeEnds[0]);
    //        write(pipeEnds[1], data.c_str(), data.size());
    //        close(pipeEnds[1]);
    //
    //        // FIXME(interactive bash): if need keep interactive shell
    //        waitpid(boxPid, nullptr, 0);
    //    }
    //
    //    return EXIT_SUCCESS;
    return -1;
}