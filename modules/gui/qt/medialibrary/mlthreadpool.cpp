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

#include "mlthreadpool.hpp"

#include <QMutexLocker>

ThreadRunner::ThreadRunner()
{
    m_threadPool.setMaxThreadCount(4);
}

ThreadRunner::~ThreadRunner()
{
    assert(m_objectTasks.empty());
    assert(m_runningTasks.empty());
}

void ThreadRunner::setMaxThreadCount(size_t threadCount)
{
    m_threadPool.setMaxThreadCount(threadCount);
}


void ThreadRunner::start(QRunnable* task, const char* queue)
{
    if (queue == nullptr)
    {
        m_threadPool.start(task);
    }
    else
    {
        QMutexLocker lock(&m_serialTaskLock);
        if (m_serialTasks.contains(queue))
        {
            m_serialTasks[queue].push_back(task);
        }
        else
        {
            m_serialTasks[queue] = QQueue<QRunnable*>();
            m_serialTasks[queue].push_back(task);
            processQueueLocked(queue);
        }
    }
}

bool ThreadRunner::tryTake(QRunnable* task)
{
    bool ret = m_threadPool.tryTake(task);
    if (ret)
        return true;

    {
        QMutexLocker lock(&m_serialTaskLock);
        for (auto queueIt = m_serialTasks.begin(); queueIt != m_serialTasks.end(); ++queueIt)
        {
            auto& queue = queueIt.value();
            auto taskIt = std::find(queue.begin(), queue.end(), task);
            if (taskIt != queue.end())
            {
                queue.erase(taskIt);
                if (queue.empty())
                    m_serialTasks.erase(queueIt);
                return true;
            }
        }
    }
    return false;
}


QRunnable* ThreadRunner::getNextTaskFromQueue(const QString& queueName)
{
    QMutexLocker lock(&m_serialTaskLock);
    auto& queue = m_serialTasks[queueName];
    if (queue.empty())
    {
        m_serialTasks.remove(queueName);
        return nullptr;
    }
    QRunnable* task = queue.front();
    queue.pop_front();
    return task;
}

void ThreadRunner::processQueueLocked(const QString& queueName)
{
    if (!m_serialTasks.contains(queueName))
        return;

    auto taskId = m_taskId++;
    struct Ctx{};
    auto runnable = new RunOnThreadRunner<Ctx>(taskId, this,
        [this, queueName](Ctx&){
            QRunnable* task = getNextTaskFromQueue(queueName);
            if (!task)
            {
                return;
            }
            task->run();
            if (task->autoDelete())
                delete task;
        },
        [this, queueName](quint64, const Ctx&){
             QMutexLocker lock(&m_serialTaskLock);
             processQueueLocked(queueName);
        });
    connect(runnable, &RunOnThreadBaseRunner::done, this, &ThreadRunner::runOnThreadDone);
    m_runningTasks.insert(taskId, runnable);
    m_objectTasks.insert(this, taskId);

    m_threadPool.start(runnable);
}

void ThreadRunner::destroy()
{
    QMutexLocker locker{&m_lock};
    m_shuttingDown = true;
    //try to cancel as many tasks as possible
    for (auto taskIt = m_objectTasks.begin(); taskIt != m_objectTasks.end(); /**/)
    {
        const QObject* object = taskIt.key();
        quint64 key = taskIt.value();
        auto task = m_runningTasks.value(key, nullptr);
        if (tryTake(task))
        {
            delete task;
            m_runningTasks.remove(key);
            taskIt = m_objectTasks.erase(taskIt);
            if (m_objectTasks.count(object) == 0)
                disconnect(object, &QObject::destroyed, this, &ThreadRunner::runOnThreadTargetDestroyed);
        }
        else
            ++taskIt;
    }

    if (m_runningTasks.empty())
    {
        deleteLater();
    }
}

void ThreadRunner::cancelTask(const QObject* object, quint64 taskId)
{
    assert(taskId != 0);
    QMutexLocker locker{&m_lock};

    auto task = m_runningTasks.value(taskId, nullptr);
    if (!task)
        return;
    task->cancel();
    bool removed = tryTake(task);
    if (removed)
        delete task;
    m_runningTasks.remove(taskId);
    m_objectTasks.remove(object, taskId);
    if (m_objectTasks.count(object) == 0)
        disconnect(object, &QObject::destroyed, this, &ThreadRunner::runOnThreadTargetDestroyed);
}

void ThreadRunner::runOnThreadDone(RunOnThreadBaseRunner* runner, quint64 target, const QObject* object, int status)
{
    QMutexLocker locker{&m_lock};
    if (!m_runningTasks.contains(target))
    {
        runner->deleteLater();
        return;
    }

    m_runningTasks.remove(target);
    m_objectTasks.remove(object, target);
    if (m_objectTasks.count(object) == 0)
        disconnect(object, &QObject::destroyed, this, &ThreadRunner::runOnThreadTargetDestroyed);

    if (m_shuttingDown)
    {
        if (m_runningTasks.empty())
            deleteLater();
        runner->deleteLater();
        return;
    }

    if (status == ML_TASK_STATUS_SUCCEED)
    {
        if (object->thread() == this->thread())
        {
            locker.unlock();
            runner->runUICallback();
            runner->deleteLater();
        }
        else
        {
            //run the callback in the object thread
            QMetaObject::invokeMethod(
                const_cast<QObject*>(object),
                [runner](){
                    runner->runUICallback();
                    runner->deleteLater();
                }
            );
        }
    }
}

void ThreadRunner::runOnThreadTargetDestroyed(QObject * object)
{
    QMutexLocker locker{&m_lock};
    if (m_objectTasks.contains(object))
    {
        for (auto taskId : m_objectTasks.values(object))
        {
            auto task = m_runningTasks.value(taskId, nullptr);
            assert(task);
            bool removed = tryTake(task);
            if (removed)
                delete task;
            m_runningTasks.remove(taskId);
        }
        m_objectTasks.remove(object);
        //no need to disconnect QObject::destroyed, as object is currently being destroyed
    }
}
