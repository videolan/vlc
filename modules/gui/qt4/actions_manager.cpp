/*****************************************************************************
 * Controller.cpp : Controller for the main interface
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
        case PREVIOUS_ACTION:
            prev(); break;
        case NEXT_ACTION:
            next(); break;
        case STOP_ACTION:
            stop(); break;
        case SLOWER_ACTION:
            slower(); break;
        case FASTER_ACTION:
            faster(); break;
        case FULLSCREEN_ACTION:
            fullscreen(); break;
        case EXTENDED_ACTION:
            extSettings(); break;
        case PLAYLIST_ACTION:
            playlist(); break;
        case SNAPSHOT_ACTION:
            snapshot(); break;
        case RECORD_ACTION:
            record(); break;
        case ATOB_ACTION:
            THEMIM->getIM()->setAtoB(); break;
        case FRAME_ACTION:
            frame(); break;
        case REVERSE_ACTION:
            reverse(); break;
        case SKIP_BACK_ACTION:
            var_SetInteger( p_intf->p_libvlc, "key-pressed",
                    ACTIONID_JUMP_BACKWARD_SHORT );
            break;
        case SKIP_FW_ACTION:
            var_SetInteger( p_intf->p_libvlc, "key-pressed",
                    ACTIONID_JUMP_FORWARD_SHORT );
            break;
        default:
            msg_Dbg( p_intf, "Action: %i", id_action );
            break;
    }
}

inline void ActionsManager::stop()
{
    THEMIM->stop();
}

void ActionsManager::play()
{
    if( THEPL->current.i_size == 0 )
    {
        /* The playlist is empty, open a file requester */
        THEDP->openFileDialog();
        return;
    }
    THEMIM->togglePlayPause();
}

inline void ActionsManager::prev()
{
    THEMIM->prev();
}

inline void ActionsManager::next()
{
    THEMIM->next();
}

/**
  * TODO
 * This functions toggle the fullscreen mode
 * If there is no video, it should first activate Visualisations...
 *  This has also to be fixed in enableVideo()
 */
void ActionsManager::fullscreen()
{
    vout_thread_t *p_vout =
      (vout_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT, FIND_ANYWHERE );
    if( p_vout)
    {
        var_SetBool( p_vout, "fullscreen", !var_GetBool( p_vout, "fullscreen" ) );
        vlc_object_release( p_vout );
    }
}

void ActionsManager::snapshot()
{
    vout_thread_t *p_vout =
      (vout_thread_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT, FIND_ANYWHERE );
    if( p_vout )
    {
        vout_Control( p_vout, VOUT_SNAPSHOT );
        vlc_object_release( p_vout );
    }
}

inline void ActionsManager::extSettings()
{
    THEDP->extendedDialog();
}

inline void ActionsManager::reverse()
{
    THEMIM->getIM()->reverse();
}

inline void ActionsManager::slower()
{
    THEMIM->getIM()->slower();
}

inline void ActionsManager::faster()
{
    THEMIM->getIM()->faster();
}

inline void ActionsManager::playlist()
{
    if( p_intf->p_sys->p_mi ) p_intf->p_sys->p_mi->togglePlaylist();
}

void ActionsManager::record()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( p_input )
    {
        /* This method won't work fine if the stream can't be cut anywhere */
        const bool b_recording = var_GetBool( p_input, "record" );
        var_SetBool( p_input, "record", !b_recording );
#if 0
        else
        {
            /* 'record' access-filter is not loaded, we open Save dialog */
            input_item_t *p_item = input_GetItem( p_input );
            if( !p_item )
                return;

            char *psz = input_item_GetURI( p_item );
            if( psz )
                THEDP->streamingDialog( NULL, psz, true );
        }
#endif
    }
}

void ActionsManager::frame()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( p_input )
        var_SetVoid( p_input, "frame-next" );
}


