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
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int      Open    ( vlc_object_t * );
static void     Close   ( vlc_object_t * );

vlc_module_begin();
    set_description( _("Gathering stream output") );
    set_capability( "sout stream", 50 );
    add_shortcut( "gather" );
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_STREAM );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static sout_stream_id_t *Add ( sout_stream_t *, es_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *,
                               block_t* );

struct sout_stream_id_t
{
    vlc_bool_t    b_used;

    es_format_t fmt;
    void          *id;
};

struct sout_stream_sys_t
{
    sout_stream_t   *p_out;

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
    p_sys->p_out    = sout_StreamNew( p_stream->p_sout, p_stream->psz_next );
    if( p_sys->p_out == NULL )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_sys->i_id         = 0;
    p_sys->id           = NULL;

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

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
        p_sys->p_out->pf_del( p_sys->p_out, p_sys->id[i]->id );
        free( p_sys->id[i] );
    }
    free( p_sys->id );

    sout_StreamDelete( p_sys->p_out );
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

    /* search a output compatible */
    for( i = 0; i < p_sys->i_id; i++ )
    {
        id = p_sys->id[i];
        if( !id->b_used &&
            id->fmt.i_cat == p_fmt->i_cat &&
            id->fmt.i_codec == p_fmt->i_codec &&
            ( ( id->fmt.i_cat == AUDIO_ES &&
                id->fmt.audio.i_rate == p_fmt->audio.i_rate &&
                id->fmt.audio.i_channels == p_fmt->audio.i_channels &&
                id->fmt.audio.i_blockalign == p_fmt->audio.i_blockalign ) ||
              ( id->fmt.i_cat == VIDEO_ES &&
                id->fmt.video.i_width == p_fmt->video.i_width &&
                id->fmt.video.i_height == p_fmt->video.i_height ) ) )
        {
            msg_Dbg( p_stream, "reusing already opened output" );
            id->b_used = VLC_TRUE;
            return id;
        }
    }

    /* destroy all output of the same categorie */
    for( i = 0; i < p_sys->i_id; i++ )
    {
        id = p_sys->id[i];
        if( !id->b_used && id->fmt.i_cat == p_fmt->i_cat )
        {
            TAB_REMOVE( p_sys->i_id, p_sys->id, id );
            p_sys->p_out->pf_del( p_sys->p_out, id );
            free( id );

            i = 0;
            continue;
        }
    }

    id = malloc( sizeof( sout_stream_id_t ) );
    msg_Dbg( p_stream, "creating new output" );
    memcpy( &id->fmt, p_fmt, sizeof( es_format_t ) );
    id->fmt.i_extra = 0;
    id->fmt.p_extra = NULL;
    id->b_used           = VLC_TRUE;
    id->id               = p_sys->p_out->pf_add( p_sys->p_out, p_fmt );
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
    id->b_used = VLC_FALSE;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Send:
 *****************************************************************************/
static int Send( sout_stream_t *p_stream,
                 sout_stream_id_t *id, block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    return p_sys->p_out->pf_send( p_sys->p_out, id->id, p_buffer );
}
