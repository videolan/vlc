/*****************************************************************************
 * voutgl.m: MacOS X OpenGL provider
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Eric Petit <titer@m0k.org>
 *          Benjamin Pracht <bigben at videolan dot org>
 *          Damien Fouilleul <damienf at videolan dot org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "intf.h"
#include "voutgl.h"


static int WindowControl( vout_window_t *, int i_query, va_list );

int WindowOpen( vout_window_t *p_wnd, const vout_window_cfg_t *cfg )
{
    p_wnd->sys = malloc( sizeof( vout_window_sys_t ) );
    if( p_wnd->sys == NULL )
        return( 1 );

    memset( p_wnd->sys, 0, sizeof( vout_window_sys_t ) );


    if (cocoaglvoutviewInit(p_wnd, cfg)) {
        msg_Err( p_wnd, "Mac OS X VoutGLView couldnt be initialized" );
        return VLC_EGENERIC;
    }

    p_wnd->control = WindowControl;
    return VLC_SUCCESS;
}

static int WindowControl( vout_window_t *p_wnd, int i_query, va_list args )
{
    /* TODO */
    if( i_query == VOUT_WINDOW_SET_STATE )
        msg_Dbg( p_wnd, "WindowControl:VOUT_WINDOW_SET_STATE" );
    else if( i_query == VOUT_WINDOW_SET_SIZE )
    {
         msg_Dbg( p_wnd, "WindowControl:VOUT_WINDOW_SET_SIZE" );
    }
    else if( i_query == VOUT_WINDOW_SET_FULLSCREEN )
    {
        msg_Dbg( p_wnd, "WindowControl:VOUT_WINDOW_SET_FULLSCREEN" );
    }
    else
        msg_Dbg( p_wnd, "WindowControl: unknown query" );
    return VLC_SUCCESS;
}

void WindowClose( vout_window_t *p_wnd )
{
    cocoaglvoutviewEnd( p_wnd );
    /* Clean up */
    free( p_wnd->sys );
}
