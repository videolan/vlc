/*****************************************************************************
 * demux.c
 *****************************************************************************
 * Copyright (C) 1999-2003 VideoLAN
 * $Id: demux.c,v 1.3 2003/09/13 17:42:16 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#include <stdlib.h>
#include <vlc/vlc.h>
#include <vlc/input.h>

#include "ninput.h"

int  demux_vaControl( input_thread_t *p_input, int i_query, va_list args )
{
    if( p_input->pf_demux_control )
    {
        return p_input->pf_demux_control( p_input, i_query, args );
    }
    return VLC_EGENERIC;
}

int  demux_Control  ( input_thread_t *p_input, int i_query, ...  )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = demux_vaControl( p_input, i_query, args );
    va_end( args );

    return i_result;
}

static void SeekOffset( input_thread_t *p_input, int64_t i_pos );

int  demux_vaControlDefault( input_thread_t *p_input, int i_query, va_list args )
{
    int     i_ret;
    double  f, *pf;
    int64_t i64, *pi64;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    switch( i_query )
    {
        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double * );
            if( p_input->stream.p_selected_area->i_size <= 0 )
            {
                *pf = 0.0;
            }
            else
            {
                *pf = (double)p_input->stream.p_selected_area->i_tell /
                      (double)p_input->stream.p_selected_area->i_size;
            }
            i_ret = VLC_SUCCESS;
            break;

        case DEMUX_SET_POSITION:
            f = (double)va_arg( args, double );
            if( p_input->stream.b_seekable && p_input->pf_seek != NULL && f >= 0.0 && f <= 1.0 )
            {
                SeekOffset( p_input, (int64_t)(f * (double)p_input->stream.p_selected_area->i_size) );
                i_ret = VLC_SUCCESS;
            }
            else
            {
                i_ret = VLC_EGENERIC;
            }
            break;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_input->stream.i_mux_rate > 0 )
            {
                *pi64 = (int64_t)1000000 *
                        ( p_input->stream.p_selected_area->i_tell / 50 ) /
                        p_input->stream.i_mux_rate;
                i_ret = VLC_SUCCESS;
            }
            else
            {
                *pi64 = 0;
                i_ret = VLC_EGENERIC;
            }
            break;

        case DEMUX_SET_TIME:
            i64 = (int64_t)va_arg( args, int64_t );
            if( p_input->stream.i_mux_rate > 0 &&
                p_input->stream.b_seekable && p_input->pf_seek != NULL && i64 >= 0 )
            {
                SeekOffset( p_input, i64 * 50 *
                                     (int64_t)p_input->stream.i_mux_rate /
                                     (int64_t)1000000 );
                i_ret = VLC_SUCCESS;
            }
            else
            {
                i_ret = VLC_EGENERIC;
            }
            break;

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_input->stream.i_mux_rate > 0 )
            {
                *pi64 = (int64_t)1000000 *
                        ( p_input->stream.p_selected_area->i_size / 50 ) /
                        p_input->stream.i_mux_rate;
                i_ret = VLC_SUCCESS;
            }
            else
            {
                *pi64 = 0;
                i_ret = VLC_EGENERIC;
            }
            break;
        case DEMUX_GET_FPS:
            i_ret = VLC_EGENERIC;
            break;

        default:
            msg_Err( p_input, "unknown query in demux_vaControlDefault !!!" );
            i_ret = VLC_EGENERIC;
            break;
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return i_ret;
}


static void SeekOffset( input_thread_t *p_input, int64_t i_pos )
{
    /* Reinitialize buffer manager. */
    input_AccessReinit( p_input );

    vlc_mutex_unlock( &p_input->stream.stream_lock );
    p_input->pf_seek( p_input, i_pos );
    vlc_mutex_lock( &p_input->stream.stream_lock );
}
