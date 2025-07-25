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

#pragma once

#include <QObject>
#include <QThread>
#include <QElapsedTimer>

// maintains a set of reusable worker threads
// class is not thread safe and must be accessed from Main thread
class WorkerThreadSet : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    ~WorkerThreadSet();

    // changes the thread affinity of 'obj' to a worker thread
    void assignToWorkerThread(QObject *obj);

private:
    struct Worker
    {
        QThread *thread = nullptr;
        int load = 0;
        QElapsedTimer inactiveTime;
    };

    const static int MAX_WORKER = 2;
    const static int CLEANUP_TIMEOUT = 10000; // 10seconds
    const static int MAX_INACTIVE_TIME = 6000; // 6seconds

    void finish(Worker &worker);

    // returns a worker thread after increasing it's load
    QThread *reserve();

    // reduces load of the previously allocated 'thread'
    // and makes it available for future operations
    // threads are automatically freed when they remain
    // inactive for extended amount of time
    void unreserve(QThread *thread);

    void cleanupInactiveWorker();

    Worker workers[MAX_WORKER] {};
};
