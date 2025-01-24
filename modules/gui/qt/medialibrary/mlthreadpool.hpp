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

class RunOnThreadBaseRunner;

class ThreadRunner : public QObject
{
    Q_OBJECT

public:
    enum MLTaskStatus {
        ML_TASK_STATUS_SUCCEED,
        ML_TASK_STATUS_CANCELED
    };

    ThreadRunner();
    ~ThreadRunner();

    void setMaxThreadCount(size_t threadCount);

    void destroy();
    void cancelTask(const QObject* object, quint64 taskId);

    template<typename Ctx>
    quint64 runOnThread(const QObject* obj,
                          std::function<void (Ctx&)> mlFun,
                          std::function<void (quint64 taskId, Ctx&)> uiFun,
                          const char* queue = nullptr);

private slots:
    void runOnThreadDone(RunOnThreadBaseRunner* runner, quint64 target, const QObject* object, int status);
    void runOnThreadTargetDestroyed(QObject * object);


private:
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

    QRunnable* getNextTaskFromQueue(const QString& queueName);
    void processQueueLocked(const QString& queueName);

private:
    QThreadPool m_threadPool;

    friend class MLThreadPoolSerialTask;
    QMutex m_serialTaskLock;
    QMap<QString, QQueue<QRunnable*>> m_serialTasks;

    bool m_shuttingDown = false;
    quint64 m_taskId = 1;
    QMap<quint64, RunOnThreadBaseRunner*> m_runningTasks;
    QMultiMap<const QObject*, quint64> m_objectTasks;
    QMutex m_lock;
};

class RunOnThreadBaseRunner : public QObject, public QRunnable
{
    Q_OBJECT
public:
    virtual ~RunOnThreadBaseRunner() = default;
    virtual void runUICallback() = 0;
    virtual void cancel() = 0;
signals:
    void done(RunOnThreadBaseRunner* runner, quint64 target, const QObject* object, int status);
};

template<typename Ctx>
class RunOnThreadRunner : public RunOnThreadBaseRunner {
public:
    RunOnThreadRunner(
        quint64 taskId,
        const QObject* obj,
        std::function<void (Ctx&)> mlFun,
        std::function<void (quint64, Ctx&)> uiFun
        )
        : RunOnThreadBaseRunner()
        , m_taskId(taskId)
        , m_obj(obj)
        , m_mlFun(mlFun)
        , m_uiFun(uiFun)
    {
        setAutoDelete(false);
    }

    void run() override
    {
        if (m_canceled)
        {
            emit done(this, m_taskId, m_obj, ThreadRunner::ML_TASK_STATUS_CANCELED);
            return;
        }
        m_mlFun(m_ctx);
        emit done(this, m_taskId, m_obj, ThreadRunner::ML_TASK_STATUS_SUCCEED);
    }

    //called from UI thread
    void runUICallback() override
    {
        m_uiFun(m_taskId, m_ctx);
    }

    void cancel() override
    {
        m_canceled = true;
    }
private:
    std::atomic_bool m_canceled {false};
    quint64 m_taskId;
    Ctx m_ctx; //default constructed
    const QObject* m_obj = nullptr;
    std::function<void (Ctx&)> m_mlFun;
    std::function<void (quint64, Ctx&)> m_uiFun;
};

template<typename Ctx>
quint64 ThreadRunner::runOnThread(const QObject* obj,
                                      std::function<void (Ctx&)> mlFun,
                                      std::function<void (quint64 taskId, Ctx&)> uiFun,
                                      const char* queue)
{
    QMutexLocker locker{&m_lock};

    if (m_shuttingDown)
        return 0;

    auto taskId = m_taskId++;
    auto runnable = new RunOnThreadRunner<Ctx>(taskId, obj, mlFun, uiFun);
    connect(runnable, &RunOnThreadBaseRunner::done, this, &ThreadRunner::runOnThreadDone);
    connect(obj, &QObject::destroyed, this, &ThreadRunner::runOnThreadTargetDestroyed, Qt::DirectConnection);
    m_runningTasks.insert(taskId, runnable);
    m_objectTasks.insert(obj, taskId);
    start(runnable, queue);
    return taskId;
}

#endif // MLTHREADPOOL_HPP
