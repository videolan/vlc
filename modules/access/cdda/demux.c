/*****************************************************************************
 * demux.c: demux functions for dvdplay.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: demux.c,v 1.1 2003/11/30 02:41:00 rocky Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Rocky Bernstein <rocky@panix.com> 
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

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <cdio/cdio.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Demux     ( input_thread_t * p_input );

/*****************************************************************************
 * Private structure
 *****************************************************************************/
struct demux_sys_t
{
    es_out_id_t *p_es;
    mtime_t     i_pts;
};


/****************************************************************************
 * DemuxOpen:
 ****************************************************************************/
int  
E_(DemuxOpen)    ( vlc_object_t * p_this)
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys;

    es_format_t    fmt;

    if( p_input->stream.i_method != INPUT_METHOD_CDDA )
    {
        return VLC_EGENERIC;
    }

    p_input->pf_demux  = Demux;
    p_input->pf_rewind = NULL;
    p_input->pf_demux_control = demux_vaControlDefault;
    p_input->p_demux_data = p_sys = malloc( sizeof( es_descriptor_t ) );
    p_sys->i_pts = 0;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_input->stream.i_mux_rate = 4 * 44100 / 50;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC( 'a', 'r', 'a', 'w' ) );
    fmt.audio.i_channels = 2;
    fmt.audio.i_rate = 44100;
    fmt.audio.i_bitspersample = 16;
    fmt.audio.i_blockalign = 4;
    fmt.i_bitrate = 4 * 44100 * 8;

    p_sys->p_es =  es_out_Add( p_input->p_es_out, &fmt );

    return VLC_SUCCESS;
}

/****************************************************************************
 * DemuxClose:
 ****************************************************************************/
void 
E_(DemuxClose)( vlc_object_t * p_this)
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    demux_sys_t    *p_sys = (demux_sys_t*)p_input->p_demux_data;

    free( p_sys );
    return;
}

/****************************************************************************
 * Demux:
 ****************************************************************************/
static int  Demux( input_thread_t * p_input )
{
    demux_sys_t    *p_sys = (demux_sys_t*)p_input->p_demux_data;
    block_t        *p_block;


    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_sys->i_pts );

    if( ( p_block = stream_Block( p_input->s, CDIO_CD_FRAMESIZE_RAW ) ) == NULL )
    {
        /* eof */
        return 0;
    }
    p_block->i_dts =
    p_block->i_pts = input_ClockGetTS( p_input,
                                       p_input->stream.p_selected_program,
                                       p_sys->i_pts );
    p_block->i_length = (mtime_t)90000 * (mtime_t)p_block->i_buffer/44100/4;

    p_sys->i_pts += p_block->i_length;

    es_out_Send( p_input->p_es_out, p_sys->p_es, p_block );

    return 1;
}

