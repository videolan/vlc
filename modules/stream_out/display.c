/*****************************************************************************
 * display.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: display.c,v 1.1 2003/04/13 20:00:21 fenrir Exp $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#include "codecs.h"

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int      Open    ( vlc_object_t * );
static void     Close   ( vlc_object_t * );

static sout_stream_id_t *Add ( sout_stream_t *, sout_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *, sout_buffer_t* );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Display stream") );
    set_capability( "sout stream", 50 );
    add_shortcut( "display" );
    set_callbacks( Open, Close );
vlc_module_end();

struct sout_stream_sys_t
{
    input_thread_t *p_input;

    vlc_bool_t     b_audio;
    vlc_bool_t     b_video;

    mtime_t        i_delay;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;
    char              *val;
    p_sys           = malloc( sizeof( sout_stream_sys_t ) );
    p_sys->p_input  = vlc_object_find( p_stream, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( !p_sys->p_input )
    {
        msg_Err( p_stream, "cannot find p_input" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->b_audio = VLC_TRUE;
    p_sys->b_video = VLC_TRUE;
    p_sys->i_delay = 100*1000;
    if( sout_cfg_find( p_stream->p_cfg, "noaudio" ) )
    {
        p_sys->b_audio = VLC_FALSE;
    }
    if( sout_cfg_find( p_stream->p_cfg, "novideo" ) )
    {
        p_sys->b_video = VLC_FALSE;
    }
    if( ( val = sout_cfg_find_value( p_stream->p_cfg, "delay" ) ) )
    {
        p_sys->i_delay = (mtime_t)atoi( val ) * (mtime_t)1000;
    }

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    p_stream->p_sys     = p_sys;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/

static void Close( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    vlc_object_release( p_sys->p_input );

    free( p_sys );
}

struct sout_stream_id_t
{
    es_descriptor_t *p_es;
};


static sout_stream_id_t * Add      ( sout_stream_t *p_stream, sout_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_t *id;

    if( ( p_fmt->i_cat == AUDIO_ES && !p_sys->b_audio )||
        ( p_fmt->i_cat == VIDEO_ES && !p_sys->b_video ) )
    {
        return NULL;
    }

    id = malloc( sizeof( sout_stream_id_t ) );

    vlc_mutex_lock( &p_sys->p_input->stream.stream_lock );
    id->p_es = input_AddES( p_sys->p_input,
                            NULL,           /* no program */
                            12,             /* es_id */
                            0 );            /* no extra data */

    if( !id->p_es )
    {
        vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );

        msg_Err( p_stream, "cannot create es" );
        free( id );
        return NULL;
    }
    id->p_es->i_stream_id   = 1;
    id->p_es->i_cat         = UNKNOWN_ES; //p_fmt->i_cat;
    id->p_es->i_fourcc      = p_fmt->i_fourcc;
    id->p_es->b_force_decoder = VLC_TRUE;
    switch( p_fmt->i_cat )
    {
        case AUDIO_ES:
            id->p_es->p_bitmapinfoheader = NULL;
            id->p_es->p_waveformatex =
                malloc( sizeof( WAVEFORMATEX ) + p_fmt->i_extra_data );
#define p_wf    ((WAVEFORMATEX*)id->p_es->p_waveformatex)
            p_wf->wFormatTag     = WAVE_FORMAT_UNKNOWN;
            p_wf->nChannels      = p_fmt->i_channels;
            p_wf->nSamplesPerSec = p_fmt->i_sample_rate;
            p_wf->nAvgBytesPerSec= p_fmt->i_bitrate / 8;
            p_wf->nBlockAlign    = p_fmt->i_block_align;
            p_wf->wBitsPerSample = 0;
            p_wf->cbSize         = p_fmt->i_extra_data;
            if( p_fmt->i_extra_data > 0 )
            {
                memcpy( &p_wf[1],
                        p_fmt->p_extra_data,
                        p_fmt->i_extra_data );
            }
#undef p_wf
            break;
        case VIDEO_ES:
            id->p_es->p_waveformatex = NULL;
            id->p_es->p_bitmapinfoheader = malloc( sizeof( BITMAPINFOHEADER ) + p_fmt->i_extra_data );
#define p_bih ((BITMAPINFOHEADER*)id->p_es->p_bitmapinfoheader)
            p_bih->biSize   = sizeof( BITMAPINFOHEADER ) + p_fmt->i_extra_data;
            p_bih->biWidth  = p_fmt->i_width;
            p_bih->biHeight = p_fmt->i_height;
            p_bih->biPlanes   = 0;
            p_bih->biBitCount = 0;
            p_bih->biCompression = 0;
            p_bih->biSizeImage   = 0;
            p_bih->biXPelsPerMeter = 0;
            p_bih->biYPelsPerMeter = 0;
            p_bih->biClrUsed       = 0;
            p_bih->biClrImportant  = 0;
            if( p_fmt->i_extra_data > 0 )
            {
                memcpy( &p_bih[1],
                        p_fmt->p_extra_data,
                        p_fmt->i_extra_data );
            }
#undef p_bih
            break;
        default:
            msg_Err( p_stream, "unknown es type" );
            free( id );
            return NULL;
    }

    if( input_SelectES( p_sys->p_input, id->p_es ) )
    {
        input_DelES( p_sys->p_input, id->p_es );
        vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );

        msg_Err( p_stream, "cannot select es" );
        free( id );
        return NULL;
    }
    vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );

    return id;
}

static int     Del      ( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    input_DelES( p_sys->p_input, id->p_es );

    free( id );

    return VLC_SUCCESS;
}

static int     Send     ( sout_stream_t *p_stream, sout_stream_id_t *id, sout_buffer_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    while( p_buffer )
    {
        sout_buffer_t *p_next;
        pes_packet_t *p_pes;
        data_packet_t   *p_data;

        if( p_buffer->i_size > 0 )
        {
            if( !( p_pes = input_NewPES( p_sys->p_input->p_method_data ) ) )
            {
                msg_Err( p_stream, "cannot allocate new PES" );
                return VLC_EGENERIC;
            }
            if( !( p_data = input_NewPacket( p_sys->p_input->p_method_data, p_buffer->i_size ) ) )
            {
                msg_Err( p_stream, "cannot allocate new data_packet" );
                return VLC_EGENERIC;
            }
            p_pes->i_dts = p_buffer->i_dts + p_sys->i_delay;
            p_pes->i_pts = p_buffer->i_pts + p_sys->i_delay;
            p_pes->p_first = p_pes->p_last = p_data;
            p_pes->i_nb_data = 1;
            p_pes->i_pes_size = p_buffer->i_size;

            p_stream->p_vlc->pf_memcpy( p_data->p_payload_start,
                                        p_buffer->p_buffer,
                                        p_buffer->i_size );

            if( id->p_es->p_decoder_fifo )
            {
                input_DecodePES( id->p_es->p_decoder_fifo, p_pes );
            }
            else
            {
                input_DeletePES( p_sys->p_input->p_method_data, p_pes );
            }
        }

        /* *** go to next buffer *** */
        p_next = p_buffer->p_next;
        sout_BufferDelete( p_stream->p_sout, p_buffer );
        p_buffer = p_next;
    }

    return VLC_SUCCESS;
}

