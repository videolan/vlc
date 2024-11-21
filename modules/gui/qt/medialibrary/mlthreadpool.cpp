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

MLThreadPoolSerialTask::MLThreadPoolSerialTask(MLThreadPool* parent, const QString& queueName)
    : m_parent(parent)
    , m_queueName(queueName)
{
    assert(m_parent);
}


void MLThreadPoolSerialTask::run()
{
    QRunnable* task = m_parent->getNextTaskFromQueue(m_queueName);
    if (!task)
    {
        deleteLater();
        return;
    }
    task->run();
    if (task->autoDelete())
        delete task;
    m_parent->start(this, nullptr);
}

MLThreadPool::MLThreadPool()
{
}


MLThreadPool::~MLThreadPool()
{
}


void MLThreadPool::setMaxThreadCount(size_t poolsize)
{
    m_threadpool.setMaxThreadCount(poolsize);
}

void MLThreadPool::start(QRunnable* task, const char* queue)
{
    if (queue == nullptr)
    {
        m_threadpool.start(task);
    }
    else
    {
        QMutexLocker lock(&m_lock);
        if (m_serialTasks.contains(queue))
        {
            m_serialTasks[queue].push_back(task);
        }
        else
        {
            m_serialTasks[queue] = QQueue<QRunnable*>();
            m_serialTasks[queue].push_back(task);
            auto serialTasks = new MLThreadPoolSerialTask(this, queue);
            serialTasks->setAutoDelete(false);
            m_threadpool.start(serialTasks);
        }
    }
}

bool MLThreadPool::tryTake(QRunnable* task)
{
    bool ret = m_threadpool.tryTake(task);
    if (ret)
        return true;

    {
        QMutexLocker lock(&m_lock);
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


QRunnable* MLThreadPool::getNextTaskFromQueue(const QString& queueName)
{
    QMutexLocker lock(&m_lock);
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

ThreadRunner::ThreadRunner()
{
    m_threadPool.setMaxThreadCount(4);
}

ThreadRunner::~ThreadRunner()
{
    assert(m_objectTasks.empty());
    assert(m_runningTasks.empty());
}

void ThreadRunner::destroy()
{
    m_shuttingDown = true;
    //try to cancel as many tasks as possible
    for (auto taskIt = m_objectTasks.begin(); taskIt != m_objectTasks.end(); /**/)
    {
        const QObject* object = taskIt.key();
        quint64 key = taskIt.value();
        auto task = m_runningTasks.value(key, nullptr);
        if (m_threadPool.tryTake(task))
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

    auto task = m_runningTasks.value(taskId, nullptr);
    if (!task)
        return;
    task->cancel();
    bool removed = m_threadPool.tryTake(task);
    if (removed)
        delete task;
    m_runningTasks.remove(taskId);
    m_objectTasks.remove(object, taskId);
    if (m_objectTasks.count(object) == 0)
        disconnect(object, &QObject::destroyed, this, &ThreadRunner::runOnThreadTargetDestroyed);
}

void ThreadRunner::runOnThreadDone(RunOnThreadBaseRunner* runner, quint64 target, const QObject* object, int status)
{
    if (m_shuttingDown)
    {
        if (m_runningTasks.contains(target))
        {
            m_runningTasks.remove(target);
            m_objectTasks.remove(object, target);
            if (m_objectTasks.count(object) == 0)
                disconnect(object, &QObject::destroyed, this, &ThreadRunner::runOnThreadTargetDestroyed);
        }
        if (m_runningTasks.empty())
            deleteLater();
    }
    else if (m_runningTasks.contains(target))
    {
        if (status == ML_TASK_STATUS_SUCCEED)
            runner->runUICallback();
        m_runningTasks.remove(target);
        m_objectTasks.remove(object, target);
        if (m_objectTasks.count(object) == 0)
            disconnect(object, &QObject::destroyed, this, &ThreadRunner::runOnThreadTargetDestroyed);
    }
    runner->deleteLater();
}

void ThreadRunner::runOnThreadTargetDestroyed(QObject * object)
{
    if (m_objectTasks.contains(object))
    {
        for (auto taskId : m_objectTasks.values(object))
        {
            auto task = m_runningTasks.value(taskId, nullptr);
            assert(task);
            bool removed = m_threadPool.tryTake(task);
            if (removed)
                delete task;
            m_runningTasks.remove(taskId);
        }
        m_objectTasks.remove(object);
        //no need to disconnect QObject::destroyed, as object is currently being destroyed
    }
}
