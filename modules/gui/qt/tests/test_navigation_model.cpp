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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "../maininterface/navigationmodel.hpp"
#include "vlc_stub_modules.hpp"

#include <memory>
#include <vector>

#include <QCoreApplication>
#include <QTest>
#include <QAbstractItemModelTester>
#include <QSignalSpy>

class TestVLCNavigationModel : public QObject
{
    Q_OBJECT

private:
    enum NodeState {
        Leaf = 0,
        Closed = 2,
        Opened = 3
    };

    bool compareRow(int row,
                    const char* title, const char* title_str,
                    NodeState state, const char*  state_str,
                    //bool expandable, const char*  expandable_str,
                    //bool expanded, const char*  expanded_str,
                    int line )
    {
        if (!QTest::qCompare(m_model->data(m_model->index(row), NavigationModel::TITLE), title, "title", "title", __FILE__, __LINE__))\
                return false;

        if (!QTest::qCompare(
                m_model->data(m_model->index(row), NavigationModel::TITLE), title,
                "title", title_str,
                __FILE__, line))
            return false;

        if (!QTest::qCompare(
                m_model->data(m_model->index(row), NavigationModel::EXPANDABLE), !!(state & 2),
                "expandable", state_str,
                __FILE__, line))
            return false;

        if (!QTest::qCompare(
                m_model->data(m_model->index(row), NavigationModel::EXPANDED), !!(state & 1),
                "expanded", state_str,
                __FILE__, line))
            return false;

        return true;
    }
#define COMPARE_ROW(row, title, state) do {\
    if (!compareRow(row, title, #title, state, #state, __LINE__))\
        return;\
} while (false)


    bool verif_inserted(
        int first, const char* first_str,
        int last, const char* last_str,
        int line)
    {
        if (!QTest::qCompare(
                m_insertedSpy->size(), 1,
               "m_insertedSpy->size", "1",
                __FILE__, line))
            return false;
        QList<QVariant> arguments = m_insertedSpy->takeFirst();
        if (!QTest::qCompare(
                arguments[1].toInt(), first,
               "inserted first", first_str,
                __FILE__, line))
            return false;
        if (!QTest::qCompare(
                arguments[2].toInt(), last,
               "inserted last", last_str,
                __FILE__, line))
            return false;
        return true;
    }

#define VERIFY_INSERTED(first, last) do { \
    if (!verif_inserted(first,#first, last, #last, __LINE__))\
        return;\
} while (false)
#define VERIFY_NOT_INSERTED QCOMPARE(m_insertedSpy->size(), 0)

    bool verif_removed(
        int first, const char* first_str,
        int last, const char* last_str,
        int line)
    {
        if (!QTest::qCompare(
                m_removedSpy->size(), 1,
               "m_removedSpy->size", "1",
                __FILE__, line))
            return false;

        QList<QVariant> arguments = m_removedSpy->takeFirst();
        if (!QTest::qCompare(
                arguments[1].toInt(), first,
               "removed first", first_str,
                __FILE__, line))
            return false;
        if (!QTest::qCompare(
                arguments[2].toInt(), last,
               "removed last", last_str,
                __FILE__, line))
            return false;
        return true;
    }

#define VERIFY_REMOVED(first, last) do { \
    if (!verif_removed(first,#first, last, #last, __LINE__))\
        return;\
} while (false)
#define VERIFY_NOT_REMOVED QCOMPARE(m_removedSpy->size(), 0)

    void initWithMedialib(bool hasMedialib) {
        m_model->classBegin();
        m_model->setHasMedialib(hasMedialib);
        m_model->componentComplete();
    }

private slots:
    void initTestCase() {
        m_env = std::make_unique<VLCTestingEnv>();
        QVERIFY(m_env->init());
    }

    void cleanupTestCase() {
        m_env.reset();
    }

    void init() {
        //m_medialib = std::make_unique<MediaLib>(m_env->intf, nullptr);
        m_model = std::make_unique<NavigationModel>();
        new QAbstractItemModelTester(m_model.get(), m_model.get());

        m_insertedSpy = std::make_unique<QSignalSpy>(m_model.get(), &QAbstractItemModel::rowsAboutToBeInserted);
        m_removedSpy = std::make_unique<QSignalSpy>(m_model.get(), &QAbstractItemModel::rowsAboutToBeRemoved);
    }

    void cleanup() {
        m_model.reset();
        //m_medialib.reset();
    }

    void testInitialValues() {
        initWithMedialib(true);
        //all is collapsed
        QCOMPARE(m_model->rowCount(), 5);
        int i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Video", Closed);
        COMPARE_ROW(i++, "Music", Closed);
        COMPARE_ROW(i++, "Browse", Leaf);
        COMPARE_ROW(i++, "Discover", Closed);
    }

    void testInitialValuesNoML() {
        initWithMedialib(false);
        //all is collapsed
        QCOMPARE(m_model->rowCount(), 3);
        int i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Browse", Leaf);
        COMPARE_ROW(i++, "Discover", Closed);
    }

    void testExpandOOB() {
        initWithMedialib(true);
        QVERIFY(m_model->setData(m_model->index(1), true, NavigationModel::EXPANDED));
        QCOMPARE(m_model->rowCount(), 7);
        QVERIFY(!m_model->setData(m_model->index(7), true, NavigationModel::EXPANDED));
        QVERIFY(!m_model->setData(m_model->index(7), false, NavigationModel::EXPANDED));
    }

    void testExpandExpandedOOB() {
        initWithMedialib(true);
        QVERIFY(!m_model->setData(m_model->index(5), true, NavigationModel::EXPANDED));
        QVERIFY(!m_model->setData(m_model->index(5), false, NavigationModel::EXPANDED));
    }

    void testExpandVideo() {
        initWithMedialib(true);
        QVERIFY(m_model->setData(m_model->index(1), true, NavigationModel::EXPANDED));

        QCOMPARE(m_model->rowCount(), 7);
        VERIFY_INSERTED(2,3);
        VERIFY_NOT_REMOVED;

        int i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Video", Opened);
        {
            COMPARE_ROW(i++, "All", Leaf);
            COMPARE_ROW(i++, "Playlists", Leaf);
        }
        COMPARE_ROW(i++, "Music", Closed);
        COMPARE_ROW(i++, "Browse", Leaf);
        COMPARE_ROW(i++, "Discover", Closed);
    }

    void testURI()
    {
        initWithMedialib(true);
        //expand video
        QVERIFY(m_model->setData(m_model->index(1), true, NavigationModel::EXPANDED));
        QCOMPARE(m_model->rowCount(), 7);

        QStringList homeUri = {"home"};
        QCOMPARE(
            m_model->data(m_model->index(0), NavigationModel::URI),
            homeUri
            );
        qWarning() << homeUri << m_model->data(m_model->index(0), NavigationModel::DEPTH);

        QStringList videoALLUri = {"video", "all"};
        QCOMPARE(
            m_model->data(m_model->index(2), NavigationModel::URI),
            videoALLUri
            );
        qWarning() << videoALLUri << m_model->data(m_model->index(2), NavigationModel::DEPTH);

        QStringList musicUri = {"music"};
        QCOMPARE(
            m_model->data(m_model->index(4), NavigationModel::URI),
            musicUri
            );
    }

    void testExpandMusic() {
        initWithMedialib(true);
        QVERIFY(m_model->setData(m_model->index(2), true, NavigationModel::EXPANDED));

        QCOMPARE(m_model->rowCount(), 10);
        VERIFY_INSERTED(3, 7);
        VERIFY_NOT_REMOVED;

        int i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Video", Closed);
        COMPARE_ROW(i++, "Music", Opened);
        {
            COMPARE_ROW(i++, "Albums", Leaf);
            COMPARE_ROW(i++, "Artists", Leaf);
            COMPARE_ROW(i++, "Tracks", Leaf);
            COMPARE_ROW(i++, "Genres", Leaf);
            COMPARE_ROW(i++, "Playlists", Leaf);
        }
        COMPARE_ROW(i++, "Browse", Leaf);
        COMPARE_ROW(i++, "Discover", Closed);
    }

    void testExpandMusicTwice() {
        initWithMedialib(true);
        QVERIFY(m_model->setData(m_model->index(2), true, NavigationModel::EXPANDED));

        QCOMPARE(m_model->rowCount(), 10);
        VERIFY_INSERTED(3, 7);
        VERIFY_NOT_REMOVED;

        int i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Video", Closed);
        COMPARE_ROW(i++, "Music", Opened);
        {
            COMPARE_ROW(i++, "Albums", Leaf);
            COMPARE_ROW(i++, "Artists", Leaf);
            COMPARE_ROW(i++, "Tracks", Leaf);
            COMPARE_ROW(i++, "Genres", Leaf);
            COMPARE_ROW(i++, "Playlists", Leaf);
        }
        COMPARE_ROW(i++, "Browse", Leaf);
        COMPARE_ROW(i++, "Discover", Closed);

        QVERIFY(m_model->setData(m_model->index(2), true, NavigationModel::EXPANDED));

        QCOMPARE(m_model->rowCount(), 10);
        VERIFY_NOT_INSERTED;
        VERIFY_NOT_REMOVED;

        i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Video", Closed);
        COMPARE_ROW(i++, "Music", Opened);
        {
            COMPARE_ROW(i++, "Albums", Leaf);
            COMPARE_ROW(i++, "Artists", Leaf);
            COMPARE_ROW(i++, "Tracks", Leaf);
            COMPARE_ROW(i++, "Genres", Leaf);
            COMPARE_ROW(i++, "Playlists", Leaf);
        }
        COMPARE_ROW(i++, "Browse", Leaf);
        COMPARE_ROW(i++, "Discover", Closed);
    }

    void testExpandBrowse() {
        initWithMedialib(true);
        QVERIFY(m_model->setData(m_model->index(3), true, NavigationModel::EXPANDED));

        QCOMPARE(m_model->rowCount(), 5);
        VERIFY_NOT_INSERTED;
        VERIFY_NOT_REMOVED;

        int i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Video", Closed);
        COMPARE_ROW(i++, "Music", Closed);
        COMPARE_ROW(i++, "Browse", Leaf);
        COMPARE_ROW(i++, "Discover", Closed);
    }

    void testRetractBrowse() {
        initWithMedialib(true);
        QVERIFY(m_model->setData(m_model->index(3), false, NavigationModel::EXPANDED));

        QCOMPARE(m_model->rowCount(), 5);
        VERIFY_NOT_INSERTED;
        VERIFY_NOT_REMOVED;

        int i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Video", Closed);
        COMPARE_ROW(i++, "Music", Closed);
        COMPARE_ROW(i++, "Browse", Leaf);
        COMPARE_ROW(i++, "Discover", Closed);
    }

    void testExpandVideoRetractVideo() {
        initWithMedialib(true);
        QVERIFY(m_model->setData(m_model->index(1), true, NavigationModel::EXPANDED));

        QCOMPARE(m_model->rowCount(), 7);
        VERIFY_INSERTED(2, 3);
        VERIFY_NOT_REMOVED;

        int i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Video", Opened);
        {
            COMPARE_ROW(i++, "All", Leaf);
            COMPARE_ROW(i++, "Playlists", Leaf);
        }
        COMPARE_ROW(i++, "Music", Closed);
        COMPARE_ROW(i++, "Browse", Leaf);
        COMPARE_ROW(i++, "Discover", Closed);

        QVERIFY(m_model->setData(m_model->index(1), false, NavigationModel::EXPANDED));

        QCOMPARE(m_model->rowCount(), 5);
        VERIFY_REMOVED(2, 3);
        VERIFY_NOT_INSERTED;

        i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Video", Closed);
        COMPARE_ROW(i++, "Music", Closed);
        COMPARE_ROW(i++, "Browse", Leaf);
        COMPARE_ROW(i++, "Discover", Closed);
    }

    void testExpandVideoRetractAll() {
        initWithMedialib(true);
        QVERIFY(m_model->setData(m_model->index(1), true, NavigationModel::EXPANDED));

        QCOMPARE(m_model->rowCount(), 7);
        VERIFY_INSERTED(2, 3);
        VERIFY_NOT_REMOVED;

        int i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Video", Opened);
        {
            COMPARE_ROW(i++, "All", Leaf);
            COMPARE_ROW(i++, "Playlists", Leaf);
        }
        COMPARE_ROW(i++, "Music", Closed);
        COMPARE_ROW(i++, "Browse", Leaf);
        COMPARE_ROW(i++, "Discover", Closed);

        QVERIFY(!m_model->setData(m_model->index(8), false, NavigationModel::EXPANDED));

        QCOMPARE(m_model->rowCount(), 7);
        VERIFY_NOT_INSERTED;
        VERIFY_NOT_REMOVED;

        i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Video", Opened);
        {
            COMPARE_ROW(i++, "All", Leaf);
            COMPARE_ROW(i++, "Playlists", Leaf);
        }
        COMPARE_ROW(i++, "Music", Closed);
        COMPARE_ROW(i++, "Browse", Leaf);
        COMPARE_ROW(i++, "Discover", Closed);
    }


    void testExpandVideoExpandAudio() {
        initWithMedialib(true);
        QVERIFY(m_model->setData(m_model->index(1), true, NavigationModel::EXPANDED));

        QCOMPARE(m_model->rowCount(), 7);
        VERIFY_INSERTED(2,3);
        VERIFY_NOT_REMOVED;

        int i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Video", Opened);
        {
            COMPARE_ROW(i++, "All", Leaf);
            COMPARE_ROW(i++, "Playlists", Leaf);
        }
        COMPARE_ROW(i++, "Music", Closed);
        COMPARE_ROW(i++, "Browse", Leaf);
        COMPARE_ROW(i++, "Discover", Closed);

        QVERIFY(m_model->setData(m_model->index(4), true, NavigationModel::EXPANDED));

        QCOMPARE(m_model->rowCount(), 10);
        VERIFY_REMOVED(2, 3);
        VERIFY_INSERTED(3, 7);

        i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Video", Closed);
        COMPARE_ROW(i++, "Music", Opened);
        {
            COMPARE_ROW(i++, "Albums", Leaf);
            COMPARE_ROW(i++, "Artists", Leaf);
            COMPARE_ROW(i++, "Tracks", Leaf);
            COMPARE_ROW(i++, "Genres", Leaf);
            COMPARE_ROW(i++, "Playlists", Leaf);
        }
        COMPARE_ROW(i++, "Browse", Leaf);
        COMPARE_ROW(i++, "Discover", Closed);
    }

    void testExpandVideoRetractAudio() {
        initWithMedialib(true);
        QVERIFY(m_model->setData(m_model->index(1), true, NavigationModel::EXPANDED));

        QCOMPARE(m_model->rowCount(), 7);
        VERIFY_INSERTED(2, 3);
        VERIFY_NOT_REMOVED;

        int i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Video", Opened);
        {
            COMPARE_ROW(i++, "All", Leaf);
            COMPARE_ROW(i++, "Playlists", Leaf);
        }
        COMPARE_ROW(i++, "Music", Closed);
        COMPARE_ROW(i++, "Browse", Leaf);
        COMPARE_ROW(i++, "Discover", Closed);

        QVERIFY(m_model->setData(m_model->index(4), false, NavigationModel::EXPANDED));

        QCOMPARE(m_model->rowCount(), 7);
        VERIFY_NOT_INSERTED;
        VERIFY_NOT_REMOVED;

        i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Video", Opened);
        {
            COMPARE_ROW(i++, "All", Leaf);
            COMPARE_ROW(i++, "Playlists", Leaf);
        }
        COMPARE_ROW(i++, "Music", Closed);
        COMPARE_ROW(i++, "Browse", Leaf);
        COMPARE_ROW(i++, "Discover", Closed);
    }

    void testExpandVideoExpandHome() {
        initWithMedialib(true);
        QVERIFY(m_model->setData(m_model->index(1), true, NavigationModel::EXPANDED));

        QCOMPARE(m_model->rowCount(), 7);
        VERIFY_INSERTED(2, 3);
        VERIFY_NOT_REMOVED;

        int i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Video", Opened);
        {
            COMPARE_ROW(i++, "All", Leaf);
            COMPARE_ROW(i++, "Playlists", Leaf);
        }
        COMPARE_ROW(i++, "Music", Closed);
        COMPARE_ROW(i++, "Browse", Leaf);
        COMPARE_ROW(i++, "Discover", Closed);

        QVERIFY(m_model->setData(m_model->index(0), true, NavigationModel::EXPANDED));

        QCOMPARE(m_model->rowCount(), 5);
        VERIFY_NOT_INSERTED;
        VERIFY_REMOVED(2, 3);

        i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Video", Closed);
        COMPARE_ROW(i++, "Music", Closed);
        COMPARE_ROW(i++, "Browse", Leaf);
        COMPARE_ROW(i++, "Discover", Closed);
    }


private:
    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<NavigationModel> m_model;
    std::unique_ptr<QSignalSpy> m_insertedSpy;
    std::unique_ptr<QSignalSpy> m_removedSpy;
};


QTEST_GUILESS_MAIN(TestVLCNavigationModel)
#include "test_navigation_model.moc"
