/*****************************************************************************
 * timer.cpp : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: timer.cpp,v 1.2 2002/11/20 14:24:00 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <errno.h>                                                 /* ENOMEM */
#include <string.h>                                            /* strerror() */
#include <stdio.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>

/* Let wxWindows take care of the i18n stuff */
#undef _

#ifdef WIN32                                                 /* mingw32 hack */
#undef Yield
#undef CreateDialog
#endif

#include <wx/wxprec.h>
#include <wx/wx.h>
#include <wx/timer.h>

#include "wxwindows.h"

/*****************************************************************************
 * Constructor.
 *****************************************************************************/
Timer::Timer( intf_thread_t *_p_intf, Interface *_p_main_interface )
{
    p_intf = _p_intf;
    p_main_interface = _p_main_interface;

    Start( 100 /*milliseconds*/, wxTIMER_CONTINUOUS );
}

Timer::~Timer()
{
}

/*****************************************************************************
 * Private methods.
 *****************************************************************************/
/*****************************************************************************
 * wxModeManage: actualise the aspect of the interface whenever the input
 * changes.
 *****************************************************************************
 * The lock has to be taken before you call the function.
 *****************************************************************************/
static int wxModeManage( intf_thread_t * p_intf )
{
    return 0;
}

/*****************************************************************************
 * wxSetupMenus: function that generates title/chapter/audio/subpic
 * menus with help from preceding functions
 *****************************************************************************
 * Function called with the lock on stream
 *****************************************************************************/
static int wxSetupMenus( intf_thread_t * p_intf )
{
    return 0;
}

/*****************************************************************************
 * Manage: manage main thread messages
 *****************************************************************************
 * In this function, called approx. 10 times a second, we check what the
 * main program wanted to tell us.
 *****************************************************************************/
void Timer::Notify()
{
    int i_start, i_stop;

    vlc_mutex_lock( &p_intf->change_lock );

    /* If the "display popup" flag has changed */
    if( p_intf->b_menu_change )
    {
        p_intf->b_menu_change = 0;
    }

    /* Update the log window */
    vlc_mutex_lock( p_intf->p_sys->p_sub->p_lock );
    i_stop = *p_intf->p_sys->p_sub->pi_stop;
    vlc_mutex_unlock( p_intf->p_sys->p_sub->p_lock );

    if( p_intf->p_sys->p_sub->i_start != i_stop )
    {
    }

    /* Update the input */
    if( p_intf->p_sys->p_input == NULL )
    {
        p_intf->p_sys->p_input = (input_thread_t *)vlc_object_find( p_intf,
                                                       VLC_OBJECT_INPUT,
                                                       FIND_ANYWHERE );
        /* Show slider */
        if(p_intf->p_sys->p_input)
        {
            p_main_interface->slider->Show();
            p_main_interface->statusbar->SetStatusText(
                p_intf->p_sys->p_input->psz_source, 1 );
        }
    }
    else if( p_intf->p_sys->p_input->b_dead )
    {
        /* Hide slider */
        if(p_intf->p_sys->p_input)
            p_main_interface->slider->Hide();

        p_main_interface->statusbar->SetStatusText( "", 1 );

        vlc_object_release( p_intf->p_sys->p_input );
        p_intf->p_sys->p_input = NULL;
    }

    if( p_intf->p_sys->p_input )
    {
        input_thread_t *p_input = p_intf->p_sys->p_input;

        vlc_mutex_lock( &p_input->stream.stream_lock );

        if( !p_input->b_die )
        {
            /* New input or stream map change */
            if( p_input->stream.b_changed )
            {
                wxModeManage( p_intf );
                wxSetupMenus( p_intf );
                p_intf->p_sys->b_playing = 1;
            }

            /* Manage the slider */
            if( p_input->stream.b_seekable && p_intf->p_sys->b_playing )
            {

                stream_position_t position;

                /* If the user hasn't touched the slider since the last time,
                 * then the input can safely change it */
                if( p_intf->p_sys->i_slider_pos ==
                    p_intf->p_sys->i_slider_oldpos )
                {
                    /* Update the value */
                    vlc_mutex_unlock( &p_input->stream.stream_lock );
                    input_Tell( p_input, &position );
                    vlc_mutex_lock( &p_input->stream.stream_lock );
                    p_intf->p_sys->i_slider_oldpos =
                        ( 100 * position.i_tell ) / position.i_size;

                    if( p_intf->p_sys->i_slider_pos !=
                        p_intf->p_sys->i_slider_oldpos )
                    {
                        p_intf->p_sys->i_slider_pos =
                            p_intf->p_sys->i_slider_oldpos;

                        p_main_interface->slider->SetValue(
                            p_intf->p_sys->i_slider_pos );
                    }
                }

                /* Otherwise, send message to the input if the user has
                 * finished dragging the slider */
                else if( p_intf->p_sys->b_slider_free )
                {
                    /* release the lock to be able to seek */
                    vlc_mutex_unlock( &p_input->stream.stream_lock );
                    input_Seek( p_input, p_intf->p_sys->i_slider_pos,
                                INPUT_SEEK_PERCENT | INPUT_SEEK_SET );
                    vlc_mutex_lock( &p_input->stream.stream_lock );

                    /* Update the old value */
                    p_intf->p_sys->i_slider_oldpos =
                        p_intf->p_sys->i_slider_pos;
                }
            }

            if( p_intf->p_sys->i_part !=
                p_input->stream.p_selected_area->i_part )
            {
                p_intf->p_sys->b_chapter_update = 1;
                wxSetupMenus( p_intf );
            }
        }

        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
    else if( p_intf->p_sys->b_playing && !p_intf->b_die )
    {
        wxModeManage( p_intf );
        p_intf->p_sys->b_playing = 0;
    }

    if( p_intf->b_die )
    {
        vlc_mutex_unlock( &p_intf->change_lock );

        /* Prepare to die, young Skywalker */
        p_main_interface->Close(TRUE);
        return;
    }

    vlc_mutex_unlock( &p_intf->change_lock );
}
