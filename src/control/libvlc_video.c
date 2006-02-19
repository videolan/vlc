/*****************************************************************************
 * video.c: ibvlc new API video functions
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id: core.c 14187 2006-02-07 16:37:40Z courmisch $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#include <libvlc_internal.h>
#include <vlc/libvlc.h>

#include <vlc/vout.h>
#include <vlc/intf.h>

static vout_thread_t *GetVout( libvlc_input_t *p_input,
                               libvlc_exception_t *p_exception )
{
    input_thread_t *p_input_thread;
    vout_thread_t *p_vout;

    if( !p_input )
    {
        libvlc_exception_raise( p_exception, "Input is NULL" );
        return NULL;
    }

    p_input_thread = (input_thread_t*)vlc_object_get(
                                 p_input->p_instance->p_vlc,
                                 p_input->i_input_id );
    if( !p_input_thread )
    {
        libvlc_exception_raise( p_exception, "Input does not exist" );
        return NULL;
    }

    p_vout = vlc_object_find( p_input_thread, VLC_OBJECT_VOUT, FIND_CHILD );
    if( !p_vout )
    {
        libvlc_exception_raise( p_exception, "No active video output" );
        return NULL;
    }
    return p_vout;
}
/**********************************************************************
 * Exported functions
 **********************************************************************/

void libvlc_set_fullscreen( libvlc_input_t *p_input, int b_fullscreen,
                            libvlc_exception_t *p_e )
{
    /* We only work on the first vout */
    vout_thread_t *p_vout1 = GetVout( p_input, p_e );
    vlc_value_t val; int i_ret;

    /* GetVout will raise the exception for us */
    if( !p_vout1 )
    {
        return;
    }

    if( b_fullscreen ) val.b_bool = VLC_TRUE;
    else               val.b_bool = VLC_FALSE;

    i_ret = var_Set( p_vout1, "fullscreen", val );
    if( i_ret )
        libvlc_exception_raise( p_e,
                        "Unexpected error while setting fullscreen value" );
}

int libvlc_get_fullscreen( libvlc_input_t *p_input,
                            libvlc_exception_t *p_e )
{
    /* We only work on the first vout */
    vout_thread_t *p_vout1 = GetVout( p_input, p_e );
    vlc_value_t val; int i_ret;

    /* GetVout will raise the exception for us */
    if( !p_vout1 )
        return 0;

    i_ret = var_Get( p_vout1, "fullscreen", &val );
    if( i_ret )
        libvlc_exception_raise( p_e,
                        "Unexpected error while looking up fullscreen value" );

    return val.b_bool == VLC_TRUE ? 1 : 0;
}

void libvlc_toggle_fullscreen( libvlc_input_t *p_input,
                               libvlc_exception_t *p_e )
{
    /* We only work on the first vout */
    vout_thread_t *p_vout1 = GetVout( p_input, p_e );
    vlc_value_t val; int i_ret;

    /* GetVout will raise the exception for us */
    if( !p_vout1 )
        return;

    i_ret = var_Get( p_vout1, "fullscreen", &val );
    if( i_ret )
        libvlc_exception_raise( p_e,
                        "Unexpected error while looking up fullscreen value" );

    val.b_bool = !val.b_bool;
    i_ret = var_Set( p_vout1, "fullscreen", val );
    if( i_ret )
        libvlc_exception_raise( p_e,
                        "Unexpected error while setting fullscreen value" );
}
