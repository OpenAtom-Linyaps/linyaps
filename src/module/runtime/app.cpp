/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QProcess>
#include <QFile>
#include <unistd.h>
#include <sys/wait.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include "app.h"

#include "module/util/yaml.h"
#include "module/util/uuid.h"
#include "module/util/json.h"

#define LINGLONG 118

#define LL_VAL(str) #str
#define LL_TOSTRING(str) LL_VAL(str)

namespace {

// const char *DefaultRuntimeID = "com.deepin.Runtime";

//! create an uuid dir, and run container
//! TODO: use file lock to make sure only one process create
//! \return
QString ensureContainerPath()
{
    auto containerID = linglong::util::genUUID();
    //    auto path = util::str_vec_join({util::xdg::userRuntimeDir().path().toStdString(), "linglong", containerID}, '/');
    //    util::fs::create_directories(util::fs::path(path), 0755);
    //    return path.c_str();
}

} // namespace

class AppPrivate
{
public:
    explicit AppPrivate(App *parent)
        : q_ptr(parent)
    {
    }

    bool init()
    {
        QFile jsonFile("../../test/data/demo/config-mini.json");
        jsonFile.open(QIODevice::ReadOnly);
        auto json = QJsonDocument::fromJson(jsonFile.readAll());
        r = fromVariant<Runtime>(json.toVariant());
        return false;
    }

    Runtime *r = nullptr;
    App *q_ptr = nullptr;
};

App::App(QObject *parent)
    : JsonSerialize(parent)
    , dd_ptr(new AppPrivate(this))
{
}

App *App::load(const QString &configFilepath)
{
    try {
        YAML::Node doc = YAML::LoadFile(configFilepath.toStdString());
        auto app = formYaml<App>(doc);
        app->dd_ptr->init();
        return app;
    } catch (...) {
        return nullptr;
    }
}

int App::start()
{
    Q_D(App);
    QString containerRoot = ensureContainerPath();
    d->r->root->path = containerRoot;

    //    write pid file
    QFile pidFile(containerRoot + QString("/%1.pid").arg(getpid()));
    pidFile.open(QIODevice::WriteOnly);
    pidFile.close();

    auto json = QJsonDocument::fromVariant(toVariant<Runtime>(d->r)).toJson();
    auto data = json.toStdString();

    int pipeEnds[2];
    if (pipe(pipeEnds) != 0) {
        return EXIT_FAILURE;
    }

    prctl(PR_SET_PDEATHSIG, SIGKILL);

    pid_t boxPid = fork();
    if (0 == boxPid) {
        // child process
        (void)close(pipeEnds[1]);
        if (dup2(pipeEnds[0], LINGLONG) == -1) {
            return EXIT_FAILURE;
        }
        (void)close(pipeEnds[0]);
        char const *const args[] = {"/usr/bin/ll-box", LL_TOSTRING(LINGLONG), NULL};
        execvp(args[0], (char **)args);
    } else {
        close(pipeEnds[0]);
        write(pipeEnds[1], data.c_str(), data.size());
        close(pipeEnds[1]);

        // FIXME(interactive bash): if need keep interactive shell
        waitpid(boxPid, nullptr, 0);
    }

    return EXIT_SUCCESS;
}

App::~App() = default;
