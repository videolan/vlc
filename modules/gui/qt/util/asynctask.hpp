/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

#ifndef VLC_ASYNC_TASK_HPP
#define VLC_ASYNC_TASK_HPP

#include <atomic>
#include <cassert>
#include <memory>
#include <QMetaObject>
#include <QObject>
#include <QRunnable>
#include <QThreadPool>

/**
 * This class helps executing a processing on a `QThreadPool` and retrieve the
 * result on the main thread.
 *
 * It allows to properly handle the case where the receiver of the result is
 * destroyed while a processing is still running, without blocking (provided
 * that the `QThreadPool` is not destroyed).
 *
 * Here is how to use it properly.
 *
 * First, write a class inheriting `AsyncTask<T>` (`T` being the type of the
 * result), and implement the `execute()` method:
 *
 * \code{.cpp}
 *     struct MyTask : public AsyncTask<int>
 *     {
 *         int m_num;
 *
 *         MyTask(int num) : m_num(num) {}
 *
 *         int execute() override
 *         {
 *             // This will be executed from a separate thread
 *             sleep(1); // long-running task
 *             return m_num * 2;
 *         }
 *     };
 * \endcode
 *
 * To execute a new task asynchronously, instantiate it, connect the result
 * signal and start it:
 *
 * \code{.cpp}
 *     // To be called from the main thread
 *     void SomeQObject::asyncLoad(int num)
 *     {
 *         // Create a task and keep a reference in a field
 *         m_task = new MyTask(num);
 *
 *         // Connect the signal to be notified of the result
 *         connect(m_task, &BaseAsyncTask::result,
 *                 this, &SomeQObject::onResult);
 *
 *         // Submit the task to some thread pool
 *         m_start->start(m_threadPool);
 *     }
 * \endcode
 *
 * The task might be canceled at any time (it might continue running
 * asynchronously if it is already running, but the caller will not receive the
 * result):
 *
 * \code{.cpp}
 *     // To be called from the main thread
 *     void SomeQObject::cancelCurrentTask()
 *     {
 *         if (m_task)
 *         {
 *             m_task->abandon();
 *             m_task = nullptr;
 *         }
 *     }
 * \endcode
 *
 * The result slot must retrieve the result and _abandon_ the task:
 *
 * \code{.cpp}
 *     // Called on the main thread
 *     void SomeQObject::onResult()
 *     {
 *         // Retrieve the task instance from the slot, if necessary
 *         MyTask *task = static_cast<MyTask *>(sender());
 *
 *         // In basic cases (one task at a time), it should be the same:
 *         assert(task == m_task);
 *
 *         // Retrieve the result (can only be called once)
 *         int result = task->takeResult();
 *
 *         qDebug() << "The result of MyTask(" << task->m_num << ") is"
 *                  << result;
 *
 *         // Abandon the task (so that it can be destroyed)
 *         task->abandon();
 *         m_task = nullptr;
 *     }
 * \endcode
 *
 * The `abandon()` method allows to handle both cancelation and memory
 * management. A task is semantically owned by 2 components:
 *  - the caller
 *  - the QThreadPool
 *
 * Indeed, the caller must be able to _cancel_ it without race conditions, even
 * if the task run() has completed. Conversely, the task must not be destroyed
 * unilaterally by the caller, to avoid crashing if it is currently running.
 *
 * The running state is tracked internally, and `abandon()` allows the caller
 * to declare that it won't use it anymore (and don't want the result). When
 * both conditions are met, the task is deleted via `QObject::deleteLater()`.
 *
 * The task result is provided via a separate method `takeResult()` instead of
 * a slot parameter, because otherwise, Qt would require its type to be:
 *  - non-template (like `std::vector<T>`),
 *  - registered to the Qt type system (wrapped into a `QVariant`),
 *  - copiable (not movable-only, like `std::unique_ptr<T>`).
 */

class BaseAsyncTask : public QObject
{
    Q_OBJECT

signals:
    void result();
};

template <typename T>
class AsyncTask : public BaseAsyncTask
{
public:
    virtual T execute() = 0;

    /**
     * Start the task on the thread pool
     */
    void start(QThreadPool &threadPool)
    {
        m_threadPool = &threadPool;
        m_runnable = std::make_unique<Runnable>(this);
        threadPool.start(&*m_runnable);
    }

    /**
     * Abandon the task (cancel and request deletion when possible)
     *
     * This method must be called from the UI thread.
     *
     * After this call, the task must not be used and the `result()` signal will
     * never be triggered.
     */
    void abandon()
    {
        assert(m_runnable);
        assert(m_threadPool);

        bool removed = m_threadPool->tryTake(&*m_runnable);
        if (removed)
        {
            /* run() will never be called */
            deleteLater();
            return;
        }

        m_abandoned = true;
        if (m_completed)
            deleteLater();

        /* Else it will be deleted from the run() function */
    }

    /**
     * Retrieve the result (from the UI thread)
     *
     * This method consumes the result, and must be called exactly once (from
     * the UI thread, in the slot connected to the signal `result()`).
     */
    T takeResult()
    {
        return std::move(m_result);
    }

private:
    class Runnable;
    std::unique_ptr<Runnable> m_runnable;

    QThreadPool *m_threadPool = nullptr;

    /* Only read and written from the main thread */
    bool m_abandoned = false;
    bool m_completed = false;

    T m_result;
};

template <typename T>
class AsyncTask<T>::Runnable : public QRunnable
{
public:
    Runnable(AsyncTask<T> *task) : m_task(task)
    {
        /* The runnable will be owned by the AsyncTask, which needs to keep a
         * reference to be able to cancel it using QThreadPool::tryTake(). */
        setAutoDelete(false);
    }

    void run() override
    {
        m_task->m_result = m_task->execute();

        QMetaObject::invokeMethod(m_task, [task = m_task]
        {
            /* Check on the main thread to avoid a call to abandon() between the
             * call to invokeLater() and the actual execution of the callback */
            task->m_completed = true;
            if (task->m_abandoned)
                task->deleteLater();
            else
                emit task->result();
        });
    }

private:
    friend class AsyncTask<T>;
    AsyncTask<T> *m_task;
};

#endif
