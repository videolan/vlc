/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#include <QTest>
#include <QSignalSpy>
#include <QWaitCondition>
#include "../medialibrary/mlthreadpool.hpp"
#include "qtestcase.h"

static const int TASK_TIMEOUT = 300; //MS

class Dummy : public QObject
{
    Q_OBJECT
};

//simple context for testing
struct Ctx {
    int id;
};

struct Barrier {
    Barrier(size_t count)
        : count(count)
    {}

    bool wait(unsigned long timeout = 0) {
        QMutexLocker lock{&mutex};
        --count;
        if (count > 0)
        {
            bool ret = condition.wait(&mutex, timeout);
            if (!ret)
                return false;
        }
        else
            condition.wakeAll();
        return true;
    }
private:
    int count;
    QMutex mutex;
    QWaitCondition condition;
};

class RunnerInThread : public QThread
{
    Q_OBJECT

public:
    void run() override {
        m_runner = new ThreadRunner();
        emit ready();
        exec();
        if (m_runner)
        {
            QSignalSpy deletedSpy(m_runner.get(), &QObject::destroyed);
            m_runner->destroy();
            deletedSpy.wait(TASK_TIMEOUT);
        }
    }

    QPointer<ThreadRunner> m_runner;

signals:
    void ready();
};


class TestThreadRunner : public QObject
{
    Q_OBJECT

private:

    ThreadRunner* getRunner()
    {
        QFETCH_GLOBAL(bool, threaded);
        if (threaded)
            return m_runnerThread->m_runner.get();
        else
            return m_runner.get();
    }

    enum TaskStatus {
        PENDING,
        FAILED,
        SUCCESS
    };

#define CHECK_TASK_TIMEOUT(task) QCOMPARE(QTest::qWaitFor([&task](){ return task != PENDING;}, TASK_TIMEOUT), false);
#define CHECK_TASK_COMPLETE(task) QCOMPARE(QTest::qWaitFor([&task](){ return task != PENDING;}, TASK_TIMEOUT), true); QCOMPARE(task, SUCCESS);

private slots:
    void init() {
        m_runner = new ThreadRunner();
        m_runnerThread = std::make_unique<RunnerInThread>();
        QSignalSpy startSpy(m_runnerThread.get(), &RunnerInThread::ready);
        m_runnerThread->start();
        startSpy.wait(TASK_TIMEOUT);
    }

    void cleanup() {
        if (m_runner) {
            QSignalSpy deletedSpy(m_runner.get(), &QObject::destroyed);
            m_runner->destroy();
            deletedSpy.wait();
        }
        m_runnerThread->quit();
        m_runnerThread->wait();
    }

    void initTestCase_data()
    {
        QTest::addColumn<bool>("threaded");
        QTest::newRow("same thread") << false;
        QTest::newRow("different thread") << true;
    }

    void testSimpleTask()
    {
        ThreadRunner* runner = getRunner();
        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();
        TaskStatus ret = PENDING;
        runner->runOnThread<Ctx>(dummy.get(), [](Ctx& ctx){
            ctx.id = 42;
        }, [&ret](size_t, const Ctx& ctx){
            if (ctx.id == 42)
                ret = SUCCESS;
            else
                ret = FAILED;
        });
        CHECK_TASK_COMPLETE(ret);
    }

    void testParallelTask()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(3);

        TaskStatus ret1 = PENDING;
        TaskStatus ret2 = PENDING;

        runner->runOnThread<Ctx>(
            dummy.get(),
            //T1
            [&barrierPre, &ret1](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    ret1 = FAILED;
                    return;
                }
            },
            [&ret1](size_t, const Ctx&){
                if (ret1 != FAILED)
                    ret1 = SUCCESS;
            }
        );

        runner->runOnThread<Ctx>(
            dummy.get(),
            //T2
            [&barrierPre, &ret2](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    ret2 = FAILED;
                    return;
                }
            },
            [&ret2](size_t, const Ctx&){
                if (ret2 != FAILED)
                    ret2 = SUCCESS;
            }
        );

        QVERIFY(barrierPre.wait(TASK_TIMEOUT));

        CHECK_TASK_COMPLETE(ret1);
        CHECK_TASK_COMPLETE(ret2);
    }


    void testParallelTaskQueued()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();
        QMutex mutex;

        Barrier barrierPre(3);

        TaskStatus ret1 = PENDING;
        TaskStatus ret2 = PENDING;

        runner->runOnThread<Ctx>(
            dummy.get(),
            //T1
            [&mutex, &barrierPre, &ret1](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    ret1 = FAILED;
                    return;
                }
            },
            [&ret1](size_t, const Ctx&){
                if (ret1 != FAILED)
                    ret1 = SUCCESS;
            },
            "queue1"
        );

        runner->runOnThread<Ctx>(
            dummy.get(),
            //T2
            [&barrierPre, &ret2](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    ret2 = FAILED;
                    return;
                }
            },
            [&ret2](size_t, const Ctx&){
                if (ret2 != FAILED)
                    ret2 = SUCCESS;
            },
            "queue2" //not the same queue
        );

        //Both tasks should be running at the same time
        QVERIFY(barrierPre.wait(TASK_TIMEOUT));

        CHECK_TASK_COMPLETE(ret1);
        CHECK_TASK_COMPLETE(ret2);
    }

    void testQueuedTask()
    {
        ThreadRunner* runner = getRunner();
        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        //theses tasks should execute sequentially
        int ret = 0;
        runner->runOnThread<Ctx>(dummy.get(),
            [](Ctx& ){
                //N/A
            }, [&ret](size_t, const Ctx&){
                if (ret != 0)
                    ret = -1;
                else
                    ret = 1;
             }, "queue");

        runner->runOnThread<Ctx>(dummy.get(),
            [](Ctx& ){
                //N/A
            },
            [&ret](size_t, const Ctx& ){
                if (ret != 1)
                    ret = -2;
                else
                    ret = 2;
            }, "queue");

        runner->runOnThread<Ctx>(dummy.get(),
             [](Ctx& ){
                 //N/A
             },
             [&ret](size_t, const Ctx& ){
                 if (ret != 2)
                     ret = -3;
                 else
                     ret = 3;
             }, "queue");

        QTRY_COMPARE_WITH_TIMEOUT(ret, 3, TASK_TIMEOUT);
    }


    void testTargetDeletedDuringTask()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(2);
        Barrier barrierPost(2);

        TaskStatus ret = PENDING;

        runner->runOnThread<Ctx>(
            dummy.get(),
            [&barrierPre, &barrierPost, &ret](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    ret = FAILED;
                    return;
                }

                //dummy is destroyed here in the other thread

                if (!barrierPost.wait(TASK_TIMEOUT))
                {
                    ret = FAILED;
                    return;
                }
            },
            [&ret](size_t, const Ctx&){
                //this should not be executed
                ret = FAILED;
            });

        {
            //wait for the task to be started
            QVERIFY(barrierPre.wait(TASK_TIMEOUT));
            dummy.reset();
            QVERIFY(barrierPost.wait(TASK_TIMEOUT));
        }
        CHECK_TASK_TIMEOUT(ret);
    }

    void testTargetDeletedDuringTaskQueued()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(2);
        Barrier barrierPost(2);

        TaskStatus ret = PENDING;

        runner->runOnThread<Ctx>(
            dummy.get(),
            [&barrierPre, &barrierPost, &ret](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    ret = FAILED;
                    return;
                }

                //dummy is destroyed here in the other thread

                if (!barrierPost.wait(TASK_TIMEOUT))
                {
                    ret = FAILED;
                    return;
                }
            },
            [&ret](size_t, const Ctx&){
                //this should not be executed
                ret = FAILED;
            }, "queued");

        //wait for the task to be started
        QVERIFY(barrierPre.wait(TASK_TIMEOUT));
        dummy.reset();
        QVERIFY(barrierPost.wait(TASK_TIMEOUT));

        CHECK_TASK_TIMEOUT(ret);
    }

    void testTargetDeletedBeforeStart()
    {
        ThreadRunner* runner = getRunner();


        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(2);
        Barrier barrierPost(2);

        TaskStatus task1ret = PENDING;
        TaskStatus task1UIret = PENDING;
        TaskStatus task2ret = PENDING;

        //there is only 1 worker thread so tasks will be executed consecutivelly
        runner->setMaxThreadCount(1);

        runner->runOnThread<Ctx>(
            dummy.get(),
            [&barrierPre, &barrierPost, &task1ret](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    task1ret = FAILED;
                    return;
                }

                //dummy is destroyed here in the other thread

                if (!barrierPost.wait(TASK_TIMEOUT))
                {
                    task1ret = FAILED;
                    return;
                }
                task1ret = SUCCESS;
            },
            [&task1UIret](size_t, const Ctx&){
                //target is deleted this should not be executed
                task1UIret = FAILED;
            });

        runner->runOnThread<Ctx>(
            dummy.get(),
            [&task2ret](Ctx&) {
                //this should not be executed
                task2ret = FAILED;
            },
            [&task2ret](size_t, const Ctx&){
                //this should not be executed
                task2ret = FAILED;
            });

        //wait for the task to be started
        QVERIFY(barrierPre.wait(TASK_TIMEOUT));
        dummy.reset();
        QVERIFY(barrierPost.wait(TASK_TIMEOUT));

        //task1 thread callback execute
        CHECK_TASK_COMPLETE(task1ret);
        //this should timeout
        CHECK_TASK_TIMEOUT(task2ret);
        QCOMPARE(task1UIret, PENDING);
    }

    void testTargetDeletedBeforeStartQueued()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(2);
        Barrier barrierPost(2);

        TaskStatus task1ret = PENDING;
        TaskStatus task1UIret = PENDING;
        TaskStatus task2ret = PENDING;

        //task are on the same queue so they will execute consecutively
        runner->runOnThread<Ctx>(
            dummy.get(),
            [&barrierPre, &barrierPost, &task1ret](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    task1ret = FAILED;
                    return;
                }

                //dummy is destroyed here in the other thread

                if (!barrierPost.wait(TASK_TIMEOUT))
                {
                    task1ret = FAILED;
                    return;
                }
                task1ret = SUCCESS;
            },
            [&task1UIret](size_t, const Ctx&){
                //target is deleted this should not be executed
                task1UIret = FAILED;
            },
            "samequeue");

        runner->runOnThread<Ctx>(
            dummy.get(),
            [&task2ret](Ctx&) {
                //this should not be executed
                task2ret = FAILED;
            },
            [&task2ret](size_t, const Ctx&){
                //this should not be executed
                task2ret = FAILED;
            },
            "samequeue");

        //wait for the task to be started
        QVERIFY(barrierPre.wait(TASK_TIMEOUT));
        dummy.reset();
        QVERIFY(barrierPost.wait(TASK_TIMEOUT));

        //task1 thread callback execute
        CHECK_TASK_COMPLETE(task1ret);
        //this should timeout
        CHECK_TASK_TIMEOUT(task2ret);
        QCOMPARE(task1UIret, PENDING);
    }


    void testTaskCanceledBeforeStart()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(2);
        Barrier barrierPost(2);

        TaskStatus task1ret = PENDING;
        TaskStatus task2ret = PENDING;

        //task are on the same queue so they will execute consecutively
        runner->runOnThread<Ctx>(
            dummy.get(),
            [&barrierPre, &barrierPost, &task1ret](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    task1ret = FAILED;
                    return;
                }

                //task is canceled here in the other thread

                if (!barrierPost.wait(TASK_TIMEOUT))
                {
                    task1ret = FAILED;
                    return;
                }
            },
            [&task1ret](size_t, const Ctx&){
                if (task1ret != FAILED)
                    task1ret = SUCCESS;
            },
            "samequeue");

        quint64 task2 = runner->runOnThread<Ctx>(
            dummy.get(),
            [&task2ret](Ctx&) {
                //this should not be executed
                task2ret = FAILED;
            },
            [&task2ret](size_t, const Ctx&){
                //this should not be executed
                task2ret = FAILED;
            },
            "samequeue");

        //wait for the task to be started
        QVERIFY(barrierPre.wait(TASK_TIMEOUT));
        runner->cancelTask(dummy.get(), task2);
        QVERIFY(barrierPost.wait(TASK_TIMEOUT));

        CHECK_TASK_COMPLETE(task1ret);
        CHECK_TASK_TIMEOUT(task2ret);
    }

    void testTaskCanceledDuringExecution()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(2);
        Barrier barrierPost(2);

        TaskStatus taskRet = PENDING;

        quint64 taskId = runner->runOnThread<Ctx>(
            dummy.get(),
            [&barrierPre, &barrierPost, &taskRet](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    taskRet = FAILED;
                    return;
                }

                //task is canceled here in the other thread

                if (!barrierPost.wait(TASK_TIMEOUT))
                {
                    taskRet = FAILED;
                    return;
                }
            },
            [&taskRet](size_t, const Ctx&){
                //this should not be executed
                taskRet = FAILED;
            });

        QVERIFY(barrierPre.wait(TASK_TIMEOUT));
        runner->cancelTask(dummy.get(), taskId);
        QVERIFY(barrierPost.wait(TASK_TIMEOUT));

        CHECK_TASK_TIMEOUT(taskRet);
    }

    void testTaskCanceledDuringExecutionQueued()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(2);
        Barrier barrierPost(2);

        TaskStatus taskRet = PENDING;

        quint64 taskId = runner->runOnThread<Ctx>(
            dummy.get(),
            [&barrierPre, &barrierPost, &taskRet](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    taskRet = FAILED;
                    return;
                }

                //task is canceled here in the other thread

                if (!barrierPost.wait(TASK_TIMEOUT))
                {
                    taskRet = FAILED;
                    return;
                }
            },
            [&taskRet](size_t, const Ctx&){
                //this should not be executed
                taskRet = FAILED;
            }, "queued");

        QVERIFY(barrierPre.wait(TASK_TIMEOUT));
        runner->cancelTask(dummy.get(), taskId);
        QVERIFY(barrierPost.wait(TASK_TIMEOUT));

        CHECK_TASK_TIMEOUT(taskRet);
    }

    void testRunnerDestroyedDuringExecution()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(2);
        Barrier barrierPost(2);

        TaskStatus taskRet = PENDING;

        runner->runOnThread<Ctx>(
            dummy.get(),
            [&barrierPre, &barrierPost, &taskRet](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    taskRet = FAILED;
                    return;
                }

                //runner is destroyed here in the other thread

                if (!barrierPost.wait(TASK_TIMEOUT))
                {
                    taskRet = FAILED;
                    return;
                }
            },
            [&taskRet](size_t, const Ctx&){
                //this should not be executed
                taskRet = FAILED;
            });

        QVERIFY(barrierPre.wait(TASK_TIMEOUT));
        //destroy runner in its thread
        if (runner->thread() == this->thread())
            runner->destroy();
        else
            QMetaObject::invokeMethod(runner, &ThreadRunner::destroy, Qt::BlockingQueuedConnection);
        QVERIFY(barrierPost.wait(TASK_TIMEOUT));

        CHECK_TASK_TIMEOUT(taskRet);
    }

    void testRunnerDestroyedDuringExecutionQueued()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        Barrier barrierPre(2);
        Barrier barrierPost(2);

        TaskStatus taskRet = PENDING;

        runner->runOnThread<Ctx>(
            dummy.get(),
            [&barrierPre, &barrierPost, &taskRet](Ctx&) {
                if (!barrierPre.wait(TASK_TIMEOUT))
                {
                    taskRet = FAILED;
                    return;
                }

                //runner is destroyed here in the other thread

                if (!barrierPost.wait(TASK_TIMEOUT))
                {
                    taskRet = FAILED;
                    return;
                }
            },
            [&taskRet](size_t, const Ctx&){
                //this should not be executed
                taskRet = FAILED;
            }, "queue");

        QVERIFY(barrierPre.wait(TASK_TIMEOUT));
        //destroy runner in its thread
        if (runner->thread() == this->thread())
            runner->destroy();
        else
            QMetaObject::invokeMethod(runner, &ThreadRunner::destroy, Qt::BlockingQueuedConnection);
        QVERIFY(barrierPost.wait(TASK_TIMEOUT));

        CHECK_TASK_TIMEOUT(taskRet);
    }


    void testNestedTask()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        TaskStatus taskRet = PENDING;

        runner->runOnThread<Ctx>(
            dummy.get(),
            [](Ctx& ctx) {
                //run on worker thread
                ctx.id = 1;
            },
            [runner, &taskRet, dummy = dummy.get()](size_t, const Ctx& ctx){
                if (ctx.id != 1)
                {
                    taskRet = FAILED;
                    return;
                }
                runner->runOnThread<Ctx>(
                    dummy,
                    [](Ctx& ctx) {
                        ctx.id = 2;
                    },
                    [&taskRet](size_t, const Ctx& ctx){
                        taskRet = (ctx.id == 2 ? SUCCESS : FAILED);
                    });
            }
            );
        CHECK_TASK_COMPLETE(taskRet);
    }

    void testNestedTaskQueued()
    {
        ThreadRunner* runner = getRunner();

        std::unique_ptr<Dummy> dummy = std::make_unique<Dummy>();

        TaskStatus taskRet = PENDING;

        runner->runOnThread<Ctx>(
            dummy.get(),
            [](Ctx& ctx) {
                //run on worker thread
                ctx.id = 1;
            },
            [runner, &taskRet, dummy = dummy.get()](size_t, const Ctx& ctx){
                if (ctx.id != 1)
                {
                    taskRet = FAILED;
                    return;
                }
                runner->runOnThread<Ctx>(
                    dummy,
                    [](Ctx& ctx) {
                        ctx.id = 2;
                    },
                    [&taskRet](size_t, const Ctx& ctx){
                        taskRet = (ctx.id == 2 ? SUCCESS : FAILED);
                    },
                    "queue"
                );
            },
            "queue"
            );
        CHECK_TASK_COMPLETE(taskRet);
    }

private:
    //A runner living in the main thread
    QPointer<ThreadRunner> m_runner;

    //A runner living in a different thread
    std::unique_ptr<RunnerInThread> m_runnerThread;
};

QTEST_GUILESS_MAIN(TestThreadRunner)
#include "test_thread_runner.moc"
