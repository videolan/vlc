/*****************************************************************************
 * autodel.c: monitor mux inputs and automatically add/delete streams
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-autodel-"

vlc_module_begin ()
    set_shortname( N_("Autodel"))
    set_description( N_("Automatically add/delete input streams"))
    set_capability( "sout stream", 50 )
    add_shortcut( "autodel" )
    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static sout_stream_id_t *Add   ( sout_stream_t *, es_format_t * );
static int               Del   ( sout_stream_t *, sout_stream_id_t * );
static int               Send  ( sout_stream_t *, sout_stream_id_t *, block_t * );

struct sout_stream_id_t
{
    sout_stream_id_t *id;
    es_format_t fmt;
    mtime_t i_last;
    bool b_error;
};

struct sout_stream_sys_t
{
    sout_stream_id_t **pp_es;
    int i_es_num;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;

    p_sys          = malloc( sizeof( sout_stream_sys_t ) );

    if( !p_stream->p_next )
    {
        msg_Err( p_stream, "cannot create chain" );
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_sys->pp_es = NULL;
    p_sys->i_es_num = 0;

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
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;

    free( p_sys );
}

static sout_stream_id_t * Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;
    sout_stream_id_t *p_es = malloc( sizeof(sout_stream_id_t) );

    p_es->fmt = *p_fmt;
    p_es->id = NULL;
    p_es->i_last = VLC_TS_INVALID;
    p_es->b_error = false;
    TAB_APPEND( p_sys->i_es_num, p_sys->pp_es, p_es );

    return p_es;
}

static int Del( sout_stream_t *p_stream, sout_stream_id_t *p_es )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;
    sout_stream_id_t *id = p_es->id;

    TAB_REMOVE( p_sys->i_es_num, p_sys->pp_es, p_es );
    free( p_es );

    if ( id != NULL )
        return p_stream->p_next->pf_del( p_stream->p_next, id );
    else
        return VLC_SUCCESS;
}

static int Send( sout_stream_t *p_stream, sout_stream_id_t *p_es,
                 block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;
    mtime_t i_current = mdate();
    int i;

    p_es->i_last = p_buffer->i_dts;
    if ( !p_es->id && !p_es->b_error )
    {
        p_es->id = p_stream->p_next->pf_add( p_stream->p_next, &p_es->fmt );
        if ( p_es->id == NULL )
        {
            p_es->b_error = true;
            msg_Err( p_stream, "couldn't create chain for id %d",
                     p_es->fmt.i_id );
        }
    }

    if ( !p_es->b_error )
        p_stream->p_next->pf_send( p_stream->p_next, p_es->id, p_buffer );
    else
        block_ChainRelease( p_buffer );

    for ( i = 0; i < p_sys->i_es_num; i++ )
    {
        if ( p_sys->pp_es[i]->id != NULL
              && (p_sys->pp_es[i]->fmt.i_cat == VIDEO_ES
                   || p_sys->pp_es[i]->fmt.i_cat == AUDIO_ES)
              && p_sys->pp_es[i]->i_last < i_current )
        {
            p_stream->p_next->pf_del( p_stream->p_next, p_sys->pp_es[i]->id );
            p_sys->pp_es[i]->id = NULL;
        }
    }

    return VLC_SUCCESS;
}
