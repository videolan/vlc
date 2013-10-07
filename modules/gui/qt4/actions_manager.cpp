/*****************************************************************************
 * actions_manager.cpp : Controller for the main interface
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
 *          Ilkka Ollakka <ileoo@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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
# include "config.h"
#endif

#include <vlc_vout.h>
#include <vlc_keys.h>

#include "actions_manager.hpp"
#include "dialogs_provider.hpp" /* Opening Dialogs */
#include "input_manager.hpp"
#include "main_interface.hpp" /* Show playlist */
#include "components/controller.hpp" /* Toggle FSC controller width */

ActionsManager * ActionsManager::instance = NULL;

ActionsManager::ActionsManager( intf_thread_t * _p_i, QObject *_parent )
               : QObject( _parent )
{
    p_intf = _p_i;
}

ActionsManager::~ActionsManager(){}

void ActionsManager::doAction( int id_action )
{
    switch( id_action )
    {
        case PLAY_ACTION:
            play(); break;
        case STOP_ACTION:
            THEMIM->stop(); break;
        case OPEN_ACTION:
            THEDP->openDialog(); break;
        case PREVIOUS_ACTION:
            THEMIM->prev(); break;
        case NEXT_ACTION:
            THEMIM->next(); break;
        case SLOWER_ACTION:
            THEMIM->getIM()->slower(); break;
        case FASTER_ACTION:
            THEMIM->getIM()->faster(); break;
        case FULLSCREEN_ACTION:
            fullscreen(); break;
        case EXTENDED_ACTION:
            THEDP->extendedDialog(); break;
        case PLAYLIST_ACTION:
            playlist(); break;
        case SNAPSHOT_ACTION:
            snapshot(); break;
        case RECORD_ACTION:
            record(); break;
        case FRAME_ACTION:
            frame(); break;
        case ATOB_ACTION:
            THEMIM->getIM()->setAtoB(); break;
        case REVERSE_ACTION:
            THEMIM->getIM()->reverse(); break;
        case SKIP_BACK_ACTION:
            skipBackward();
            break;
        case SKIP_FW_ACTION:
            skipForward();
            break;
        case QUIT_ACTION:
            THEDP->quit();  break;
        case RANDOM_ACTION:
            THEMIM->toggleRandom(); break;
        case INFO_ACTION:
            THEDP->mediaInfoDialog(); break;
        case OPEN_SUB_ACTION:
            THEDP->loadSubtitlesFile(); break;
        case FULLWIDTH_ACTION:
            if( p_intf->p_sys->p_mi )
                p_intf->p_sys->p_mi->getFullscreenControllerWidget()->toggleFullwidth();
            break;
        default:
            msg_Dbg( p_intf, "Action: %i", id_action );
            break;
    }
}

void ActionsManager::play()
{
    if( THEPL->current.i_size == 0 && THEPL->items.i_size == 0 )
    {
        /* The playlist is empty, open a file requester */
        THEDP->openFileDialog();
        return;
    }
    THEMIM->togglePlayPause();
}

/**
  * TODO
 * This functions toggle the fullscreen mode
 * If there is no video, it should first activate Visualisations...
 *  This has also to be fixed in enableVideo()
 */
void ActionsManager::fullscreen()
{
    bool fs = var_ToggleBool( THEPL, "fullscreen" );
    vout_thread_t *p_vout = THEMIM->getVout();
    if( p_vout)
    {
        var_SetBool( p_vout, "fullscreen", fs );
        vlc_object_release( p_vout );
    }
}

void ActionsManager::snapshot()
{
    vout_thread_t *p_vout = THEMIM->getVout();
    if( p_vout )
    {
        var_TriggerCallback( p_vout, "video-snapshot" );
        vlc_object_release( p_vout );
    }
}

void ActionsManager::playlist()
{
    if( p_intf->p_sys->p_mi ) p_intf->p_sys->p_mi->togglePlaylist();
}

void ActionsManager::record()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( p_input )
    {
        /* This method won't work fine if the stream can't be cut anywhere */
        var_ToggleBool( p_input, "record" );
#if 0
        else
        {
            /* 'record' access-filter is not loaded, we open Save dialog */
            input_item_t *p_item = input_GetItem( p_input );
            if( !p_item )
                return;

            char *psz = input_item_GetURI( p_item );
            if( psz )
                THEDP->streamingDialog( NULL, qfu(psz), true );
        }
#endif
    }
}

void ActionsManager::frame()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( p_input )
        var_TriggerCallback( p_input, "frame-next" );
}

void ActionsManager::toggleMuteAudio()
{
     playlist_MuteToggle( THEPL );
}

void ActionsManager::AudioUp()
{
    playlist_VolumeUp( THEPL, 1, NULL );
}

void ActionsManager::AudioDown()
{
    playlist_VolumeDown( THEPL, 1, NULL );
}

void ActionsManager::skipForward()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( p_input )
        THEMIM->getIM()->jumpFwd();
}

void ActionsManager::skipBackward()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( p_input )
        THEMIM->getIM()->jumpBwd();
}

