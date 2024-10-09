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
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* Define a builtin module for mocked parts */
#define MODULE_NAME renderer_manager_test
#undef VLC_DYNAMIC_PLUGIN

#include "../../../../test/libvlc/test.h"

#include <vlc/vlc.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_playlist.h>
#include <vlc_services_discovery.h>
#include <vlc_renderer_discovery.h>
#include <vlc_probe.h>
#include <vlc_interface.h>
#include <vlc_player.h>

#include "../../../../lib/libvlc_internal.h"

#include "qt.hpp"

namespace vlc {
class Compositor {};
}

static vlc_renderer_discovery_t* g_rd = nullptr;
static bool g_rd_probe_enabled = false;

static int OpenRD( vlc_object_t* p_this )
{
    g_rd = (vlc_renderer_discovery_t *)p_this;
    return VLC_SUCCESS;
}

static void CloseRD( vlc_object_t* )
{
    g_rd = nullptr;

}

static int vlc_rd_probe_open(vlc_object_t *obj) {
    auto probe = (struct vlc_probe_t *)obj;

    if (g_rd_probe_enabled)
        vlc_rd_probe_add(probe, "rd", "a fake renderer for testing purpose");
    //only probe ourself
    return VLC_PROBE_STOP;
}

static qt_intf_t* g_intf = nullptr;

static int OpenIntf(vlc_object_t* p_this)
{
    auto intfThread = reinterpret_cast<intf_thread_t*>(p_this);
    libvlc_int_t* libvlc = vlc_object_instance( p_this );

    /* Ensure initialization of objects in qt_intf_t. */
    g_intf = vlc_object_create<qt_intf_t>( libvlc );
    if (!g_intf)
        return VLC_ENOMEM;

    g_intf->obj.logger = vlc_LogHeaderCreate(libvlc->obj.logger, "qt");
    if (!g_intf->obj.logger)
    {
        vlc_object_delete(g_intf);
        return VLC_EGENERIC;
    }
    g_intf->intf = intfThread;
    intfThread->p_sys = reinterpret_cast<intf_sys_t*>(g_intf);
    return VLC_SUCCESS;
}

static void CloseIntf( vlc_object_t *p_this )
{
    intf_thread_t* intfThread = (intf_thread_t*)(p_this);
    auto p_intf = reinterpret_cast<qt_intf_t*>(intfThread->p_sys);
    if (!p_intf)
        return;
    vlc_LogDestroy(p_intf->obj.logger);
    vlc_object_delete(p_intf);
}

vlc_module_begin()
    set_callbacks(OpenIntf, CloseIntf)
    set_capability("interface", 0)
add_submodule()
    set_capability("renderer_discovery", 0)
    add_shortcut("rd")
    set_callbacks(OpenRD, CloseRD)
add_submodule()
    set_capability("renderer probe", 10000)
    set_callback(vlc_rd_probe_open)
vlc_module_end()

extern "C" {

const char vlc_module_name[] = MODULE_STRING;
VLC_EXPORT vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};
}

#include <QTest>
#include <QAbstractItemModelTester>
#include "../util/renderer_manager.hpp"
#include <vlc_cxx_helpers.hpp>

using RendererItemPtr = vlc_shared_data_ptr_type(vlc_renderer_item_t,
    vlc_renderer_item_hold,
    vlc_renderer_item_release);


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
        vlc_rd_add_item( g_rd, item.get() );
        return item;
    }

    void checkDummyRDItem(int row, int id = -1) {
        if (id == -1)
            id = row;
        QModelIndex idx = m_model->index(row, 0);
        QCOMPARE(m_model->data(idx, RendererManager::TYPE), "type");
        QCOMPARE(m_model->data(idx, RendererManager::NAME), QString("name%1").arg(id));
        QCOMPARE(m_model->data(idx, RendererManager::SOUT), QString("dummy{ip=%1.%1.%1.%1,port=%1,extra sout}").arg(id));
        QCOMPARE(m_model->data(idx, RendererManager::ICON_URI), QString("icon://"));
        QCOMPARE(m_model->data(idx, RendererManager::FLAGS), id);
    }

private slots:
    void initTestCase() {
        test_init();

        m_vlc = libvlc_new(test_defaults_nargs, test_defaults_args);
        libvlc_InternalAddIntf(m_vlc->p_libvlc_int, MODULE_STRING);
        libvlc_InternalPlay(m_vlc->p_libvlc_int);

        m_playlist = vlc_intf_GetMainPlaylist(g_intf->intf);
        m_player = vlc_playlist_GetPlayer( m_playlist );
    }

    void cleanupTestCase() {
        libvlc_release(m_vlc);
    }

    void init() {
        g_rd_probe_enabled = true;
        m_model = new RendererManager(g_intf, m_player);
        //QAbstractItemModelTester checks that QAbstractItemModel events are coherents
        m_modelTester = new QAbstractItemModelTester(m_model);
        QVERIFY(g_rd == nullptr);
    }

    void cleanup() {
        delete m_modelTester;
        delete m_model;
        QVERIFY(g_rd == nullptr);
    }

    void testEmpty() {
        QVERIFY(g_rd == nullptr);
        //model is empty before scan
        QCOMPARE(m_model->rowCount(), 0);
        m_model->StartScan();
        QVERIFY(g_rd != nullptr);
        QCOMPARE(m_model->rowCount(), 0);
        QCOMPARE(m_model->getStatus(), RendererManager::RUNNING);
        //scan didn't find anything
        m_model->StopScan();
        QVERIFY(g_rd == nullptr);
        QCOMPARE(m_model->rowCount(), 0);
        QCOMPARE(m_model->getStatus(), RendererManager::IDLE);
    }

    void testStatic() {
        QCOMPARE(m_model->rowCount(), 0);
        m_model->StartScan();
        QCOMPARE(m_model->rowCount(), 0);
        QCOMPARE(m_model->getStatus(), RendererManager::RUNNING);

        QVERIFY(g_rd != nullptr);

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
        QVERIFY(g_rd == nullptr);
    }

    void testTwoPassesIdentical() {
        m_model->StartScan();
        QVERIFY(g_rd != nullptr);
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
        vlc_rd_remove_item(g_rd, item.get());
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
        vlc_rd_remove_item( g_rd, r3.get() );

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
        vlc_rd_add_item( g_rd, r0.get() );
        vlc_rd_add_item( g_rd, r1.get() );
        vlc_rd_add_item( g_rd, r2.get() );
        //no r3
        vlc_rd_add_item( g_rd, r4.get() );
        m_model->StopScan();
        QCOMPARE(m_model->rowCount(), 4);

        //not finding the selected item again should keep it in the model
        selidx = m_model->index(2);
        m_model->setData(selidx, true, RendererManager::SELECTED);
        m_model->StartScan();
        vlc_rd_add_item( g_rd, r0.get() );
        vlc_rd_add_item( g_rd, r1.get() );
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
        g_rd_probe_enabled = false;
        QCOMPARE(m_model->getStatus(), RendererManager::IDLE);
        m_model->StartScan();
        QCOMPARE(m_model->getStatus(), RendererManager::FAILED);
        QCOMPARE(m_model->rowCount(), 0);
        QVERIFY(g_rd == nullptr);
        g_rd_probe_enabled = true;
    }
private:
    libvlc_instance_t* m_vlc = nullptr;
    vlc_playlist_t* m_playlist = nullptr;
    vlc_player_t* m_player = nullptr;
    QAbstractItemModelTester* m_modelTester = nullptr;
    RendererManager* m_model;
};

QTEST_GUILESS_MAIN(TestClass)
#include "test_renderer_manager_model.moc"
