/*****************************************************************************
 * transrate.c: MPEG2 video transrating module
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: transrate.c,v 1.7 2004/03/03 11:20:52 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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
#include <stdio.h>
#include <stdlib.h>
#define NDEBUG 1
#include <assert.h>
#include <math.h>

#include <vlc/vlc.h>
#include <vlc/sout.h>
#include <vlc/input.h>

#include "transrate.h"

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int      Open    ( vlc_object_t * );
static void     Close   ( vlc_object_t * );

static sout_stream_id_t *Add ( sout_stream_t *, es_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *, sout_buffer_t * );

static int  transrate_video_process( sout_stream_t *, sout_stream_id_t *, sout_buffer_t *, sout_buffer_t ** );

void E_(process_frame)( sout_stream_t *p_stream,
               sout_stream_id_t *id, sout_buffer_t *in, sout_buffer_t **out );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("MPEG2 video transrating stream output") );
    set_capability( "sout stream", 50 );
    add_shortcut( "transrate" );
    set_callbacks( Open, Close );
vlc_module_end();

struct sout_stream_sys_t
{
    sout_stream_t   *p_out;

    int             i_vbitrate;
    mtime_t         i_shaping_delay;

    mtime_t         i_first_frame;
    mtime_t         i_dts, i_pts;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;
    char *val;

    p_sys = malloc( sizeof( sout_stream_sys_t ) );
    p_sys->p_out = sout_stream_new( p_stream->p_sout, p_stream->psz_next );

    p_sys->i_vbitrate   = 0;

    if( ( val = sout_cfg_find_value( p_stream->p_cfg, "vb" ) ) )
    {
        p_sys->i_vbitrate = atoi( val );
        if( p_sys->i_vbitrate < 16000 )
        {
            p_sys->i_vbitrate *= 1000;
        }
    }
    else
    {
        p_sys->i_vbitrate = 3000000;
    }

    p_sys->i_shaping_delay = 500000;
    if( ( val = sout_cfg_find_value( p_stream->p_cfg, "shaping" ) ) )
    {
        p_sys->i_shaping_delay = (int64_t)atoi( val ) * 1000;
        if( p_sys->i_shaping_delay <= 0 )
        {
            msg_Err( p_stream,
                     "invalid shaping ("I64Fd"ms) reseting to 500ms",
                     p_sys->i_shaping_delay / 1000 );
            p_sys->i_shaping_delay = 500000;
        }
    }

    p_sys->i_first_frame = 0;

    msg_Dbg( p_stream, "codec video %dkb/s max gop="I64Fd"us",
             p_sys->i_vbitrate / 1024, p_sys->i_shaping_delay );

    if( !p_sys->p_out )
    {
        msg_Err( p_stream, "cannot create chain" );
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    p_stream->p_sys     = p_sys;

    p_stream->p_sout->i_padding += 200;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t       *p_stream = (sout_stream_t *)p_this;
    sout_stream_sys_t   *p_sys = p_stream->p_sys;

    sout_stream_delete( p_sys->p_out );
    free( p_sys );
}


static sout_stream_id_t * Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_sys_t   *p_sys = p_stream->p_sys;
    sout_stream_id_t    *id;

    id = malloc( sizeof( sout_stream_id_t ) );
    id->id = NULL;

    if( p_fmt->i_cat == VIDEO_ES
            && p_fmt->i_codec == VLC_FOURCC('m', 'p', 'g', 'v') )
    {
        msg_Dbg( p_stream,
                 "creating video transrating for fcc=`%4.4s'",
                 (char*)&p_fmt->i_codec );

        id->p_current_buffer = NULL;
        id->p_next_gop = NULL;
        id->i_next_gop_duration = 0;
        id->i_next_gop_size = 0;
        memset( &id->tr, 0, sizeof( transrate_t ) );
        id->tr.bs.i_byte_in = id->tr.bs.i_byte_out = 0;
        id->tr.fact_x = 1.0;

        /* open output stream */
        id->id = p_sys->p_out->pf_add( p_sys->p_out, p_fmt );
        id->b_transrate = VLC_TRUE;
    }
    else
    {
        msg_Dbg( p_stream, "not transrating a stream (fcc=`%4.4s')", (char*)&p_fmt->i_codec );
        id->id = p_sys->p_out->pf_add( p_sys->p_out, p_fmt );
        id->b_transrate = VLC_FALSE;

        if( id->id == NULL )
        {
            free( id );
            return NULL;
        }
    }

    return id;
}

static int     Del      ( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t   *p_sys = p_stream->p_sys;

    if( id->id )
    {
        p_sys->p_out->pf_del( p_sys->p_out, id->id );
    }
    free( id );

    return VLC_SUCCESS;
}

static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 sout_buffer_t *p_buffer )
{
    sout_stream_sys_t   *p_sys = p_stream->p_sys;

    if( id->b_transrate )
    {
        sout_buffer_t *p_buffer_out;
        transrate_video_process( p_stream, id, p_buffer, &p_buffer_out );

        if( p_buffer_out )
        {
            return p_sys->p_out->pf_send( p_sys->p_out, id->id, p_buffer_out );
        }
        return VLC_SUCCESS;
    }
    else if( id->id != NULL )
    {
        return p_sys->p_out->pf_send( p_sys->p_out, id->id, p_buffer );
    }
    else
    {
        sout_BufferDelete( p_stream->p_sout, p_buffer );
        return VLC_EGENERIC;
    }
}

static int transrate_video_process( sout_stream_t *p_stream,
               sout_stream_id_t *id, sout_buffer_t *in, sout_buffer_t **out )
{
    transrate_t    *tr = &id->tr;
    bs_transrate_t *bs = &tr->bs;

    *out = NULL;

    while ( in != NULL )
    {
        sout_buffer_t * p_next = in->p_next;
        int i_flags = in->i_flags;

        in->p_next = NULL;
        sout_BufferChain( &id->p_next_gop, in );
        id->i_next_gop_duration += in->i_length;
        id->i_next_gop_size += in->i_size;
        in = p_next;

        if( ((i_flags & (BLOCK_FLAG_TYPE_I << SOUT_BUFFER_FLAGS_BLOCK_SHIFT))
                && id->i_next_gop_duration >= 300000)
              || (id->i_next_gop_duration > p_stream->p_sys->i_shaping_delay) )
        {
            mtime_t i_bitrate = (mtime_t)id->i_next_gop_size * 8000
                                    / (id->i_next_gop_duration / 1000);
            mtime_t i_new_bitrate;

            if ( i_bitrate > p_stream->p_sys->i_vbitrate )
            {
                tr->fact_x = (double)i_bitrate / p_stream->p_sys->i_vbitrate;
            }
            else
            {
                tr->fact_x = 1.0;
            }
            id->tr.i_current_gop_size = id->i_next_gop_size;
            id->tr.i_wanted_gop_size = (p_stream->p_sys->i_vbitrate)
                                    * (id->i_next_gop_duration / 1000) / 8000;
            id->tr.i_new_gop_size = 0;

            id->p_current_buffer = id->p_next_gop;

            while ( id->p_current_buffer != NULL )
            {
                sout_buffer_t * p_next = id->p_current_buffer->p_next;
                if ( tr->fact_x == 1.0 )
                {
                    bs->i_byte_out += id->p_current_buffer->i_size;
                    id->p_current_buffer->p_next = NULL;
                    sout_BufferChain( out, id->p_current_buffer );
                }
                else
                {
                    E_(process_frame)( p_stream, id, id->p_current_buffer, out );
                    sout_BufferDelete(p_stream->p_sout, id->p_current_buffer);
                }
                id->p_current_buffer = p_next;
            }

            if ( tr->fact_x != 1.0 )
            {
                i_new_bitrate = (mtime_t)tr->i_new_gop_size * 8000
                                    / (id->i_next_gop_duration / 1001);
                if (i_new_bitrate > p_stream->p_sys->i_vbitrate + 300000)
                    msg_Err(p_stream, "%lld -> %lld (%f, r:%f) d=%lld",
                            i_bitrate, i_new_bitrate, tr->fact_x,
                            (float)i_bitrate / i_new_bitrate,
                            id->i_next_gop_duration);
                    else
                        msg_Dbg(p_stream, "%lld -> %lld (%f, r:%f) d=%lld",
                            i_bitrate, i_new_bitrate, tr->fact_x,
                            (float)i_bitrate / i_new_bitrate,
                            id->i_next_gop_duration);
            }

            id->p_next_gop = NULL;
            id->i_next_gop_duration = 0;
            id->i_next_gop_size = 0;
        }
    }

    return VLC_SUCCESS;
}

