/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include <QTest>
#include <QAbstractItemModelTester>

#include "vlc_stub_modules.hpp"

#include "../util/renderer_manager.hpp"

using RendererItemPtr = ::vlc::vlc_shared_data_ptr<vlc_renderer_item_t,
    &vlc_renderer_item_hold,
    &vlc_renderer_item_release>;

class TestClass : public QObject
{
    Q_OBJECT

private:
    RendererItemPtr pushDummyRDItem(int i) {
        QString name = QString("name%1").arg(i);
        QString sout = QString("dummy://%1.%1.%1.%1:%1").arg(i);
        RendererItemPtr item(vlc_renderer_item_new(
                "type", qtu(name), qtu(sout), "extra sout",
                nullptr, "icon://", i ));
        vlc_rd_add_item( rd(), item.get() );
        return item;
    }

    void checkDummyRDItem(int row, int id = -1) {
        if (id == -1)
            id = row;
        QModelIndex idx = m_model->index(row, 0);
        QCOMPARE(m_model->data(idx, RendererManager::TYPE), "type");
        QCOMPARE(m_model->data(idx, RendererManager::NAME), QString("name%1").arg(id));
        QCOMPARE(m_model->data(idx, RendererManager::SOUT), QString("dummy{ip=%1.%1.%1.%1,port=%1,device-name=name%1,extra sout}").arg(id));
        QCOMPARE(m_model->data(idx, RendererManager::ICON_URI), QString("icon://"));
        QCOMPARE(m_model->data(idx, RendererManager::FLAGS), id);
    }

    vlc_renderer_discovery_t* rd() const
    {
        return m_env->renderDiscovery;
    }

private slots:
    void initTestCase() {
        m_env = std::make_unique<VLCTestingEnv>();
        QVERIFY(m_env->init());
        m_player = m_env->intf->p_player;
    }

    void cleanupTestCase() {
        m_player = nullptr;
        m_env.reset();
    }

    void init() {
        m_env->renderDiscoveryProbeEnabled = true;

        m_model = new RendererManager(m_env->intf, m_player);
        //QAbstractItemModelTester checks that QAbstractItemModel events are coherents
        m_modelTester = std::make_unique<QAbstractItemModelTester>(m_model);
        QVERIFY(rd() == nullptr);
    }

    void cleanup() {
        m_modelTester.reset();
        delete m_model;
        QVERIFY(rd() == nullptr);
    }

    void testEmpty() {
        QVERIFY(rd() == nullptr);
        //model is empty before scan
        QCOMPARE(m_model->rowCount(), 0);
        m_model->StartScan();
        QVERIFY(rd() != nullptr);
        QCOMPARE(m_model->rowCount(), 0);
        QCOMPARE(m_model->getStatus(), RendererManager::RUNNING);
        //scan didn't find anything
        m_model->StopScan();
        QVERIFY(rd() == nullptr);
        QCOMPARE(m_model->rowCount(), 0);
        QCOMPARE(m_model->getStatus(), RendererManager::IDLE);
    }

    void testStatic() {
        QCOMPARE(m_model->rowCount(), 0);
        m_model->StartScan();
        QCOMPARE(m_model->rowCount(), 0);
        QCOMPARE(m_model->getStatus(), RendererManager::RUNNING);

        QVERIFY(rd() != nullptr);

        for (int i = 0; i < 5; ++i) {
            pushDummyRDItem(i);
            QCOMPARE(m_model->rowCount(), i+1);
            checkDummyRDItem(i);
        }
        m_model->StopScan();

        //items are still present
        QCOMPARE(m_model->rowCount(), 5);
        QCOMPARE(m_model->getStatus(), RendererManager::IDLE);

        for (int i = 0; i < 5; ++i) {
            checkDummyRDItem(i);
        }

        //module is closed
        QVERIFY(rd() == nullptr);
    }

    void testTwoPassesIdentical() {
        m_model->StartScan();
        QVERIFY(rd() != nullptr);
        QCOMPARE(m_model->rowCount(), 0);
        for (int i = 0; i < 5; ++i) {
            pushDummyRDItem(i);
        }
        QCOMPARE(m_model->rowCount(), 5);
        m_model->StopScan();
        QCOMPARE(m_model->rowCount(), 5);

        //second pass will yield the same items
        m_model->StartScan();
        QCOMPARE(m_model->rowCount(), 5);
        for (int i = 0; i < 5; ++i) {
            pushDummyRDItem(i);
        }
        QCOMPARE(m_model->rowCount(), 5);
        m_model->StopScan();
        QCOMPARE(m_model->rowCount(), 5);
        QCOMPARE(m_model->getStatus(), RendererManager::IDLE);

        for (int i = 0; i < 5; ++i) {
            checkDummyRDItem(i);
        }
    }

    void testTwoPassesDifferent() {
        m_model->StartScan();
        for (int i = 0; i < 5; ++i) {
            pushDummyRDItem(i);
        }
        m_model->StopScan();

        //second pass will yield the same items
        m_model->StartScan();
        QCOMPARE(m_model->rowCount(), 5);
        for (int i = 0; i < 3; ++i) {
            pushDummyRDItem(i);
        }
        QCOMPARE(m_model->rowCount(), 5);
        m_model->StopScan();
        //items from former passes have been removed
        QCOMPARE(m_model->rowCount(), 3);
        QCOMPARE(m_model->getStatus(), RendererManager::IDLE);
        for (int i = 0; i < 3; ++i) {
            checkDummyRDItem(i);
        }
    }

    void testRemovedItem() {
        m_model->StartScan();
        for (int i = 0; i < 3; ++i) {
            pushDummyRDItem(i);
        }
        auto item = pushDummyRDItem(3);
        for (int i = 4; i <= 6; ++i) {
            pushDummyRDItem(i);
        }
        QCOMPARE(m_model->rowCount(), 7);
        vlc_rd_remove_item(rd(), item.get());
        QCOMPARE(m_model->rowCount(), 6);
        m_model->StopScan();
        QCOMPARE(m_model->rowCount(), 6);

        for (int i = 0; i < 3; ++i) {
            checkDummyRDItem(i, i);
        }
        for (int i = 3; i < 6; ++i) {
            checkDummyRDItem(i, i+1);
        }
    }

    void testRendererSelection() {
        m_model->StartScan();

        auto r0 = pushDummyRDItem(0);
        auto r1 = pushDummyRDItem(1);
        auto r2 = pushDummyRDItem(2);
        auto r3 = pushDummyRDItem(3);
        auto r4 = pushDummyRDItem(4);

        //item are not selected by default
        QCOMPARE(m_model->useRenderer(), false);
        for (int i = 0; i < 5; ++i) {
            QModelIndex idx = m_model->index(i, 0);
            QCOMPARE(m_model->data(idx, RendererManager::SELECTED), false);
        }

        //select a first renderer
        QModelIndex selidx = m_model->index(3);
        m_model->setData(selidx, true, RendererManager::SELECTED);
        QCOMPARE(m_model->useRenderer(), true);
        for (int i = 0; i < 5; ++i) {
            QModelIndex idx = m_model->index(i, 0);
            if (i == 3)
                QCOMPARE(m_model->data(idx, RendererManager::SELECTED), true);
            else
                QCOMPARE(m_model->data(idx, RendererManager::SELECTED), false);
        }

        //player renderer is actually set
        {
            vlc_player_Lock(m_player);
            QCOMPARE(vlc_player_GetRenderer(m_player), r3.get());
            vlc_player_Unlock(m_player);
        }

        //select another renderer
        selidx = m_model->index(2);
        m_model->setData(selidx, true, RendererManager::SELECTED);
        QCOMPARE(m_model->useRenderer(), true);
        for (int i = 0; i < 5; ++i) {
            QModelIndex idx = m_model->index(i, 0);
            if (i == 2)
                QCOMPARE(m_model->data(idx, RendererManager::SELECTED), true);
            else
                QCOMPARE(m_model->data(idx, RendererManager::SELECTED), false);
        }

        //player renderer has actually changed
        {
            vlc_player_Lock(m_player);
            QCOMPARE(vlc_player_GetRenderer(m_player), r2.get());
            vlc_player_Unlock(m_player);
        }

        //deselect renderer from index
        m_model->setData(selidx, false, RendererManager::SELECTED);
        QCOMPARE(m_model->useRenderer(), false);
        for (int i = 0; i < 5; ++i) {
            QModelIndex idx = m_model->index(i, 0);
            QCOMPARE(m_model->data(idx, RendererManager::SELECTED), false);
        }

        //player renderer is actually unset
        {
            vlc_player_Lock(m_player);
            QCOMPARE(vlc_player_GetRenderer(m_player), nullptr);
            vlc_player_Unlock(m_player);
        }

        //re-select renderer then deselect it using disableRenderer
        m_model->setData(selidx, true, RendererManager::SELECTED);
        m_model->disableRenderer();
        QCOMPARE(m_model->useRenderer(), false);
        for (int i = 0; i < 5; ++i) {
            QModelIndex idx = m_model->index(i, 0);
            QCOMPARE(m_model->data(idx, RendererManager::SELECTED), false);
        }


        //make the player select the renderer itself
        {
            vlc_player_Lock(m_player);
            vlc_player_SetRenderer(m_player, r4.get());
            vlc_player_Unlock(m_player);
        }
        QCOMPARE(m_model->useRenderer(), true);
        for (int i = 0; i < 5; ++i) {
            QModelIndex idx = m_model->index(i, 0);
            if (i == 4)
                QCOMPARE(m_model->data(idx, RendererManager::SELECTED), true);
            else
                QCOMPARE(m_model->data(idx, RendererManager::SELECTED), false);
        }
        //the  make the player unselect the renderer
        {
            vlc_player_Lock(m_player);
            vlc_player_SetRenderer(m_player, nullptr);
            vlc_player_Unlock(m_player);
        }
        QCOMPARE(m_model->useRenderer(), false);
        for (int i = 0; i < 5; ++i) {
            QModelIndex idx = m_model->index(i, 0);
            QCOMPARE(m_model->data(idx, RendererManager::SELECTED), false);
        }
    }

    void testSelectionLost() {
        m_model->StartScan();

        auto r0 = pushDummyRDItem(0);
        auto r1 = pushDummyRDItem(1);
        auto r2 = pushDummyRDItem(2);
        auto r3 = pushDummyRDItem(3);
        auto r4 = pushDummyRDItem(4);

        QModelIndex selidx = m_model->index(3);
        m_model->setData(selidx, true, RendererManager::SELECTED);

        //selected item is removed
        QCOMPARE(m_model->rowCount(), 5);
        vlc_rd_remove_item( rd(), r3.get() );

        //item is held by model
        QCOMPARE(m_model->useRenderer(), true);
        QCOMPARE(m_model->rowCount(), 5);

        //disable the renderer
        m_model->setData(selidx, false, RendererManager::SELECTED);
        QCOMPARE(m_model->useRenderer(), false);
        //item is kept by model until next scan
        QCOMPARE(m_model->rowCount(), 5);
        m_model->StopScan();
        QCOMPARE(m_model->rowCount(), 5);

        //item should be gone after next scan
        m_model->StartScan();
        vlc_rd_add_item( rd(), r0.get() );
        vlc_rd_add_item( rd(), r1.get() );
        vlc_rd_add_item( rd(), r2.get() );
        //no r3
        vlc_rd_add_item( rd(), r4.get() );
        m_model->StopScan();
        QCOMPARE(m_model->rowCount(), 4);

        //not finding the selected item again should keep it in the model
        selidx = m_model->index(2);
        m_model->setData(selidx, true, RendererManager::SELECTED);
        m_model->StartScan();
        vlc_rd_add_item( rd(), r0.get() );
        vlc_rd_add_item( rd(), r1.get() );
        m_model->StopScan();
        QCOMPARE(m_model->rowCount(), 3);
        QCOMPARE(m_model->data(selidx, RendererManager::SELECTED), true);
        QCOMPARE(m_model->useRenderer(), true);
        m_model->StartScan();
        m_model->StopScan();
        selidx = m_model->index(0);
        QCOMPARE(m_model->rowCount(), 1);
        QCOMPARE(m_model->data(selidx, RendererManager::SELECTED), true);
        QCOMPARE(m_model->useRenderer(), true);
    }

    //deleting the model without stopping the scan
    void testNoStopScanDeletion() {
        m_model->StartScan();
    }

    //failed state when no renderer manager is found
    void testNoProbes() {
        m_env->renderDiscoveryProbeEnabled = false;
        QCOMPARE(m_model->getStatus(), RendererManager::IDLE);
        m_model->StartScan();
        QCOMPARE(m_model->getStatus(), RendererManager::FAILED);
        QCOMPARE(m_model->rowCount(), 0);
        QVERIFY(rd() == nullptr);
        m_env->renderDiscoveryProbeEnabled = true;
    }
private:

    std::unique_ptr<VLCTestingEnv> m_env;
    vlc_player_t* m_player = nullptr;
    std::unique_ptr<QAbstractItemModelTester> m_modelTester;
    RendererManager* m_model = nullptr;
};

QTEST_GUILESS_MAIN(TestClass)
#include "test_renderer_manager_model.moc"
