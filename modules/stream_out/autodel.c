/*****************************************************************************
 * autodel.c: monitor mux inputs and automatically add/delete streams
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
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

typedef struct sout_stream_id_sys_t sout_stream_id_sys_t;
struct sout_stream_id_sys_t
{
    sout_stream_id_sys_t *id;
    es_format_t fmt;
    vlc_tick_t i_last;
    bool b_error;
};

typedef struct
{
    sout_stream_id_sys_t **pp_es;
    int i_es_num;
} sout_stream_sys_t;

static void *Add( sout_stream_t *p_stream, const es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;
    sout_stream_id_sys_t *p_es = malloc( sizeof(sout_stream_id_sys_t) );
    if( unlikely(p_es == NULL) )
        return NULL;

    es_format_Copy( &p_es->fmt, p_fmt );

    p_es->id = NULL;
    p_es->i_last = VLC_TICK_INVALID;
    p_es->b_error = false;
    TAB_APPEND( p_sys->i_es_num, p_sys->pp_es, p_es );

    return p_es;
}

static void Del( sout_stream_t *p_stream, void *_p_es )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;
    sout_stream_id_sys_t *p_es = (sout_stream_id_sys_t *)_p_es;

    if( p_es->id != NULL )
        sout_StreamIdDel( p_stream->p_next, p_es->id );

    TAB_REMOVE( p_sys->i_es_num, p_sys->pp_es, p_es );
    es_format_Clean( &p_es->fmt );
    free( p_es );
}

static int Send( sout_stream_t *p_stream, void *_p_es, block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;
    sout_stream_id_sys_t *p_es = (sout_stream_id_sys_t *)_p_es;
    vlc_tick_t i_current = vlc_tick_now();
    int i;

    p_es->i_last = p_buffer->i_dts;
    if ( !p_es->id && !p_es->b_error )
    {
        p_es->id = sout_StreamIdAdd( p_stream->p_next, &p_es->fmt );
        if ( p_es->id == NULL )
        {
            p_es->b_error = true;
            msg_Err( p_stream, "couldn't create chain for id %d",
                     p_es->fmt.i_id );
        }
    }

    if ( !p_es->b_error )
        sout_StreamIdSend( p_stream->p_next, p_es->id, p_buffer );
    else
        block_ChainRelease( p_buffer );

    for ( i = 0; i < p_sys->i_es_num; i++ )
    {
        if ( p_sys->pp_es[i]->id != NULL
              && (p_sys->pp_es[i]->fmt.i_cat == VIDEO_ES
                   || p_sys->pp_es[i]->fmt.i_cat == AUDIO_ES)
              && p_sys->pp_es[i]->i_last < i_current )
        {
            sout_StreamIdDel( p_stream->p_next, p_sys->pp_es[i]->id );
            p_sys->pp_es[i]->id = NULL;
        }
    }

    return VLC_SUCCESS;
}

static const struct sout_stream_operations ops = {
    Add, Del, Send, NULL, NULL,
};

static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = vlc_obj_malloc(p_this, sizeof (*p_sys));

    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    p_sys->pp_es = NULL;
    p_sys->i_es_num = 0;

    p_stream->ops = &ops;
    p_stream->p_sys = p_sys;

    return VLC_SUCCESS;
}

#define SOUT_CFG_PREFIX "sout-autodel-"

vlc_module_begin()
    set_shortname(N_("Autodel"))
    set_description(N_("Automatically add/delete input streams"))
    set_capability("sout filter", 50)
    add_shortcut("autodel")
    set_callback(Open)
vlc_module_end()
