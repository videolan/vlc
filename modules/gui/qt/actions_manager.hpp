/*****************************************************************************
 * actions_manager.hpp : Controller for the main interface
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
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

#ifndef QVLC_ACTIONS_MANAGER_H_
#define QVLC_ACTIONS_MANAGER_H_ 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include "util/singleton.hpp"
#include <QVector>

#include <QObject>
#include <QTimer>
class QAction;

typedef enum actionType_e
{
    PLAY_ACTION,
    STOP_ACTION,
    OPEN_ACTION,
    PREVIOUS_ACTION,
    NEXT_ACTION,
    SLOWER_ACTION,
    FASTER_ACTION,
    FULLSCREEN_ACTION,
    FULLWIDTH_ACTION,
    EXTENDED_ACTION,
    PLAYLIST_ACTION,
    SNAPSHOT_ACTION,
    RECORD_ACTION,
    FRAME_ACTION,
    ATOB_ACTION,
    REVERSE_ACTION,
    SKIP_BACK_ACTION,
    SKIP_FW_ACTION,
    QUIT_ACTION,
    RANDOM_ACTION,
    LOOP_ACTION,
    INFO_ACTION,
    OPEN_SUB_ACTION,
} actionType_e;

class ActionsManager : public QObject, public Singleton<ActionsManager>
{

    Q_OBJECT
    friend class Singleton<ActionsManager>;

public:

private:
    ActionsManager( intf_thread_t  *_p_i );
    virtual ~ActionsManager();

    intf_thread_t* const p_intf;
    QVector<vlc_renderer_discovery_t*> m_rds;
    QTimer m_stop_scan_timer;
    bool m_scanning;

    static void renderer_event_item_added( vlc_renderer_discovery_t *,
                                           vlc_renderer_item_t * );
    static void renderer_event_item_removed( vlc_renderer_discovery_t *,
                                             vlc_renderer_item_t * );
    static vlc_renderer_item_t* compareRenderers( const QVariant &m_obj,
                                                  vlc_renderer_item_t* p_item );

public slots:
    void toggleMuteAudio();
    void AudioUp();
    void AudioDown();
    void play();
    void record();
    void skipForward();
    void skipBackward();
    void StartRendererScan();
    void RendererMenuCountdown();
    void StopRendererScan();
    void RendererSelected( QAction * );

protected slots:
    void onRendererItemAdded( vlc_renderer_item_t* );
    void onRendererItemRemoved( vlc_renderer_item_t* );
    void fullscreen();
    void snapshot();
    void playlist();
    void frame();

    virtual void doAction( int );

signals:
    void rendererItemAdded( vlc_renderer_item_t* );
    void rendererItemRemoved( vlc_renderer_item_t* );
};

#endif

