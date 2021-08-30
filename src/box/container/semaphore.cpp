/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
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

#include "semaphore.h"

#include <sys/sem.h>

#include "util/logger.h"

namespace linglong {

union semun {
    int val;
    struct semid_ds *buf;
    ushort *array;
};

struct Semaphore::SemaphorePrivate {
    explicit SemaphorePrivate(Semaphore *parent = nullptr)
        : q_ptr(parent)
    {
        (void)q_ptr;
    }

    struct sembuf sem_lock = {0, -1, SEM_UNDO};

    struct sembuf sem_unlock = {0, 1, SEM_UNDO};

    int sem_id = -1;

    Semaphore *q_ptr;
};

Semaphore::Semaphore(int key)
    : dd_ptr(new SemaphorePrivate(this))
{
    dd_ptr->sem_id = semget(key, 1, IPC_CREAT | 0666);
    if (dd_ptr->sem_id < 0) {
        logErr() << "semget failed" << util::retErrString(dd_ptr->sem_id);
    }
    logDbg() << "Semaphore " << dd_ptr->sem_id;
}

Semaphore::~Semaphore() = default;

int Semaphore::init()
{
    union semun sem_union = {0};
    sem_union.val = 0;
    logDbg() << "semctl " << dd_ptr->sem_id;
    if (semctl(dd_ptr->sem_id, 0, SETVAL, sem_union) == -1) {
        logErr() << "semctl failed" << util::retErrString(-1);
    }
    return 0;
}

int Semaphore::passeren()
{
    return semop(dd_ptr->sem_id, &dd_ptr->sem_lock, 1);
}

int Semaphore::vrijgeven()
{
    return semop(dd_ptr->sem_id, &dd_ptr->sem_unlock, 1);
}

} // namespace linglong
