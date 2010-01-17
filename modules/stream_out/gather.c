/*****************************************************************************
 * gather.c: gathering stream output module
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_sout.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int      Open    ( vlc_object_t * );
static void     Close   ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("Gathering stream output") )
    set_capability( "sout stream", 50 )
    add_shortcut( "gather" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static sout_stream_id_t *Add ( sout_stream_t *, es_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *, block_t* );

struct sout_stream_id_t
{
    bool    b_used;

    es_format_t fmt;
    void          *id;
};

struct sout_stream_sys_t
{
    int              i_id;
    sout_stream_id_t **id;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;

    p_stream->p_sys = p_sys = malloc( sizeof( sout_stream_sys_t ) );
    if( p_sys == NULL )
        return VLC_EGENERIC;

    if( !p_stream->p_next )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    TAB_INIT( p_sys->i_id, p_sys->id );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    int i;

    for( i = 0; i < p_sys->i_id; i++ )
    {
        sout_stream_id_t *id = p_sys->id[i];

        sout_StreamIdDel( p_stream->p_next, id->id );
        es_format_Clean( &id->fmt );
        free( id );
    }
    TAB_CLEAN( p_sys->i_id, p_sys->id );

    free( p_sys );
}

/*****************************************************************************
 * Add:
 *****************************************************************************/
static sout_stream_id_t * Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_t  *id;
    int i;

    /* search a compatible output */
    for( i = 0; i < p_sys->i_id; i++ )
    {
        id = p_sys->id[i];
        if( id->b_used )
            continue;

        if( id->fmt.i_cat != p_fmt->i_cat || id->fmt.i_codec != p_fmt->i_codec )
            continue;

        if( id->fmt.i_cat == AUDIO_ES )
        {
            audio_format_t *p_a = &id->fmt.audio;
            if( p_a->i_rate != p_fmt->audio.i_rate ||
                p_a->i_channels != p_fmt->audio.i_channels ||
                p_a->i_blockalign != p_fmt->audio.i_blockalign )
                continue;
        }
        else if( id->fmt.i_cat == VIDEO_ES )
        {
            video_format_t *p_v = &id->fmt.video;
            if( p_v->i_width != p_fmt->video.i_width ||
                p_v->i_height != p_fmt->video.i_height )
                continue;
        }

        /* */
        msg_Dbg( p_stream, "reusing already opened output" );
        id->b_used = true;
        return id;
    }

    /* destroy all outputs from the same category */
    for( i = 0; i < p_sys->i_id; i++ )
    {
        id = p_sys->id[i];
        if( !id->b_used && id->fmt.i_cat == p_fmt->i_cat )
        {
            TAB_REMOVE( p_sys->i_id, p_sys->id, id );
            sout_StreamIdDel( p_stream->p_next, id->id );
            es_format_Clean( &id->fmt );
            free( id );

            i = 0;
            continue;
        }
    }

    msg_Dbg( p_stream, "creating new output" );
    id = malloc( sizeof( sout_stream_id_t ) );
    if( id == NULL )
        return NULL;
    es_format_Copy( &id->fmt, p_fmt );
    id->b_used           = true;
    id->id               = sout_StreamIdAdd( p_stream->p_next, &id->fmt );
    if( id->id == NULL )
    {
        free( id );
        return NULL;
    }
    TAB_APPEND( p_sys->i_id, p_sys->id, id );

    return id;
}

/*****************************************************************************
 * Del:
 *****************************************************************************/
static int Del( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    VLC_UNUSED(p_stream);
    id->b_used = false;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Send:
 *****************************************************************************/
static int Send( sout_stream_t *p_stream,
                 sout_stream_id_t *id, block_t *p_buffer )
{
    return sout_StreamIdSend( p_stream->p_next, id->id, p_buffer );
}
