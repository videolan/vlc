/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#ifndef MLTHREADPOOL_HPP
#define MLTHREADPOOL_HPP

#include <QObject>
#include <QRunnable>
#include <QQueue>
#include <QMap>
#include <QThreadPool>
#include <QMutex>

class MLThreadPool;

//internal task MLThreadPool
class MLThreadPoolSerialTask : public QObject, public QRunnable
{
    Q_OBJECT
public:
    MLThreadPoolSerialTask(MLThreadPool* parent, const QString& queueName);

    void run() override;

private:
    MLThreadPool* m_parent = nullptr;
    QString m_queueName;
};

/**
 * @brief The MLThreadPool act like a QThreadPool, with the difference that it allows tasks
 * to be run sequentially by specifying a queue name when starting the task.
 */
class MLThreadPool
{
public:
    explicit MLThreadPool();
    ~MLThreadPool();


    void setMaxThreadCount(size_t threadCount);

    /**
     * @brief start enqueue a QRunnable to be executed on the threadpool
     * @param task is the task to enqueue
     * @param queue, the name of the queue, all task with the same queue name will be
     * ran sequentially, if queue is null, task will be run without additional specific
     * constraint (like on QThreadPool)
     */
    void start(QRunnable* task, const char* queue = nullptr);

    /**
     * @brief tryTake atempt to the specified task from the queue if the task has not started
     * @return true if the task has been removed from the queue and was not started yet
     */
    bool tryTake(QRunnable* task);

private:
    friend class MLThreadPoolSerialTask;

    QRunnable* getNextTaskFromQueue(const QString& queueName);

    QMutex m_lock;
    QThreadPool m_threadpool;
    QMap<QString, QQueue<QRunnable*>> m_serialTasks;
};

#endif // MLTHREADPOOL_HPP
