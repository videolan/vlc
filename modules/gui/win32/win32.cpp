/*****************************************************************************
 * win32.cpp : Win32 interface plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 * $Id: win32.cpp,v 1.13 2003/01/26 02:22:59 ipkiss Exp $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vcl.h>
#include <stdlib.h>                                      /* malloc(), free() */
#include <errno.h>                                                /* ENOMEM */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "mainframe.h"
#include "menu.h"
#include "win32_common.h"

/*****************************************************************************
 * Exported interface functions.
 *****************************************************************************/
extern "C" __declspec(dllexport)
    int __VLC_SYMBOL( vlc_entry ) ( module_t *p_module );

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );
static void Run    ( intf_thread_t * );

int Win32Manage( void *p_data );

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = (intf_sys_t *) malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return( 1 );
    };

    p_intf->pf_run = Run;

    p_intf->p_sys->p_sub = msg_Subscribe( p_intf );

    /* Initialize Win32 thread */
    p_intf->p_sys->b_playing = 0;
    p_intf->p_sys->b_popup_changed = 0;

    p_intf->p_sys->p_input = NULL;
    p_intf->p_sys->i_playing = -1;
    p_intf->p_sys->b_play_when_adding = VLC_TRUE;

    p_intf->p_sys->b_slider_free = 1;

    p_intf->p_sys->b_aout_update = VLC_FALSE;
    p_intf->p_sys->b_vout_update = VLC_FALSE;
    p_intf->p_sys->b_program_update = VLC_FALSE;
    p_intf->p_sys->b_title_update = VLC_FALSE;
    p_intf->p_sys->b_chapter_update = VLC_FALSE;
    p_intf->p_sys->b_audio_update = VLC_FALSE;
    p_intf->p_sys->b_spu_update = VLC_FALSE;

    return( 0 );
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    if( p_intf->p_sys->p_input )
    {
        vlc_object_release( p_intf->p_sys->p_input );
    }

    msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    p_intf->p_sys->p_window = new TMainFrameDlg( NULL, p_intf );
    p_intf->p_sys->p_playwin = new TPlaylistDlg( NULL, p_intf );
    p_intf->p_sys->p_messages = new TMessagesDlg( NULL, p_intf );
    p_intf->p_sys->p_menus = new TMenusGen( p_intf );

    /* show main window and wait until it is closed */
    p_intf->p_sys->p_window->ShowModal();

    if( p_intf->p_sys->p_disc ) delete p_intf->p_sys->p_disc;
    if( p_intf->p_sys->p_network ) delete p_intf->p_sys->p_network;
    if( p_intf->p_sys->p_preferences ) delete p_intf->p_sys->p_preferences;
    delete p_intf->p_sys->p_menus;
    delete p_intf->p_sys->p_messages;
    delete p_intf->p_sys->p_playwin;
    delete p_intf->p_sys->p_window;
}

/*****************************************************************************
 * Win32Manage: manage main thread messages
 *****************************************************************************
 * In this function, called approx. 10 times a second, we check what the
 * main program wanted to tell us.
 *****************************************************************************/
int Win32Manage( intf_thread_t *p_intf )
{
    vlc_mutex_lock( &p_intf->change_lock );

    /* If the "display popup" flag has changed */
    if( p_intf->b_menu_change )
    {
        /* FIXME: It would be nice to close the popup when the user left-clicks
        elsewhere, or to actualize the position when he right-clicks again,
        but i couldn't find a way to close it :-( */
        TPoint MousePos = Mouse->CursorPos;
        p_intf->p_sys->p_window->PopupMenuMain->Popup( MousePos.x, MousePos.y );
        p_intf->b_menu_change = 0;
    }

    /* Update the log window */
    p_intf->p_sys->p_messages->UpdateLog();

    /* Update the playlist */
    p_intf->p_sys->p_playwin->Manage();

    /* Update the input */
    if( p_intf->p_sys->p_input == NULL )
    {
        p_intf->p_sys->p_input = (input_thread_t *)
            vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    }
    else if( p_intf->p_sys->p_input->b_dead )
    {
        vlc_object_release( p_intf->p_sys->p_input );
        p_intf->p_sys->p_input = NULL;
    }

    if( p_intf->p_sys->p_input != NULL && !p_intf->p_sys->p_input->b_die )
    {
        vlc_bool_t b_need_menus = VLC_FALSE;
        input_thread_t  * p_input = p_intf->p_sys->p_input;
        vlc_object_t * p_aout = NULL;
        vlc_object_t * p_vout = NULL;

        vlc_mutex_lock( &p_input->stream.stream_lock );

        /* New input or stream map change */
        if( p_input->stream.b_changed )
        {
            p_intf->p_sys->p_window->ModeManage();
            b_need_menus = VLC_TRUE;
            p_intf->p_sys->b_playing = 1;
        }

        /* Manage the slider */
        if( p_input->stream.b_seekable && p_intf->p_sys->b_playing )
        {
            TTrackBar * TrackBar = p_intf->p_sys->p_window->TrackBar;
            off_t NewValue = TrackBar->Position;

#define p_area p_input->stream.p_selected_area
            /* If the user hasn't touched the slider since the last time,
             * then the input can safely change it */
            if( NewValue == p_intf->p_sys->OldValue )
            {
                /* Update the value */
                TrackBar->Position = p_intf->p_sys->OldValue =
                    ( (off_t)SLIDER_MAX_VALUE * p_area->i_tell ) /
                      p_area->i_size;
            }
            /* Otherwise, send message to the input if the user has
             * finished dragging the slider */
            else if( p_intf->p_sys->b_slider_free )
            {
                off_t i_seek = ( NewValue * p_area->i_size ) /
                                 (off_t)SLIDER_MAX_VALUE;

                /* release the lock to be able to seek */
                vlc_mutex_unlock( &p_input->stream.stream_lock );
                input_Seek( p_input, i_seek, INPUT_SEEK_SET );
                vlc_mutex_lock( &p_input->stream.stream_lock );

                /* Update the old value */
                p_intf->p_sys->OldValue = NewValue;
            }
#    undef p_area

        }

        if( p_intf->p_sys->i_part != p_input->stream.p_selected_area->i_part )
        {
            p_intf->p_sys->b_chapter_update = 1;
            b_need_menus = VLC_TRUE;
        }

        /* Does the audio output require to update the menus ? */
        p_aout = (vlc_object_t *)vlc_object_find( p_intf, VLC_OBJECT_AOUT,
                                                  FIND_ANYWHERE );
        if( p_aout != NULL )
        {
            vlc_value_t val;
            if( var_Get( p_aout, "intf-change", &val ) >= 0
                && val.b_bool )
            {
                p_intf->p_sys->b_aout_update = 1;
                b_need_menus = VLC_TRUE;
            }

            vlc_object_release( p_aout );
        }

        /* Does the video output require to update the menus ? */
        p_vout = (vlc_object_t *)vlc_object_find( p_intf, VLC_OBJECT_VOUT,
                                                   FIND_ANYWHERE );
        if( p_vout != NULL )
        {
            vlc_value_t val;
            if( var_Get( p_vout, "intf-change", &val ) >= 0
                && val.b_bool )
            {
                p_intf->p_sys->b_vout_update = 1;
                b_need_menus = VLC_TRUE;
            }

            if( var_Get( p_vout, "directx-on-top", &val ) >= 0 )
            {
                p_intf->p_sys->p_window->MenuOnTop->Checked = val.b_bool;
                p_intf->p_sys->p_window->PopupOnTop->Checked = val.b_bool;
            }

            vlc_object_release( p_vout );
        }

        if( b_need_menus )
        {
            //p_intf->p_sys->p_menus->SetupMenus();
        }

        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
    else if( p_intf->p_sys->b_playing && !p_intf->b_die )
    {
        p_intf->p_sys->p_window->ModeManage();
        p_intf->p_sys->b_playing = 0;
    }

    if( p_intf->b_die )
    {
        vlc_mutex_unlock( &p_intf->change_lock );

        /* Prepare to die, young Skywalker */
        p_intf->p_sys->p_window->ModalResult = mrOk;

        /* Just in case */
        return( FALSE );
    }

    vlc_mutex_unlock( &p_intf->change_lock );

    return( TRUE );
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define MAX_LINES_TEXT N_("maximum number of lines in the log window")
#define MAX_LINES_LONGTEXT N_( \
    "You can set the maximum number of lines that the log window will display."\
    " Enter -1 if you want to keep all messages." )

vlc_module_begin();
    add_category_hint( N_("Miscellaneous"), NULL );
    add_integer( "intfwin-max-lines", 500, NULL, MAX_LINES_TEXT, MAX_LINES_LONGTEXT );
    set_description( _("Native Windows interface module") );
    set_capability( "interface", 100 );
    set_callbacks( E_(Open), E_(Close) );
    add_shortcut( "win" );
    add_shortcut( "win32" );
vlc_module_end();
