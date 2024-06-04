/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "workerthreadset.hpp"

#include <QTimer>

WorkerThreadSet::~WorkerThreadSet()
{
    for (auto worker : workers)
    {
        if (!worker.thread)
            continue;

        finish( worker );
    }
}

void WorkerThreadSet::assignToWorkerThread(QObject *obj)
{
    auto worker = reserve();
    obj->moveToThread( worker );

    QObject::connect(obj, &QObject::destroyed, worker, [this, worker]()
    {
        unreserve( worker );
    });
}

void WorkerThreadSet::finish(Worker &worker)
{
    worker.thread->quit();
    worker.thread->wait();
    delete worker.thread;
}

QThread *WorkerThreadSet::reserve()
{
    auto itr = std::min_element(std::begin(workers)
                              , std::end(workers)
                              , [](const Worker &l, const Worker &r)
    {
        return l.load < r.load;
    });

    assert(itr != std::end(workers));

    if (!itr->thread)
    {
        itr->thread = new QThread;
        itr->thread->start();
    }

    itr->load++;
    itr->inactiveTime.invalidate();
    return itr->thread;
}

void WorkerThreadSet::unreserve(QThread *thread)
{
    auto itr = std::find_if(std::begin(workers)
                            , std::end(workers)
                            , [thread](const Worker &i) { return i.thread == thread; });

    if (itr == std::end(workers)) return; // impossible?

    const int load = --itr->load;
    if (load != 0)
        return;

    itr->inactiveTime.start();
    QTimer::singleShot(CLEANUP_TIMEOUT, this, &WorkerThreadSet::cleanupInactiveWorker);
}

void WorkerThreadSet::cleanupInactiveWorker()
{
    for (auto &worker : workers)
    {
        if ((worker.load == 0)
                && worker.inactiveTime.hasExpired(MAX_INACTIVE_TIME)
                && worker.thread)
        {
            finish( worker );

            worker.thread = nullptr;
        }
    }
}
