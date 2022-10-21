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
#include <vlc_list.h>

typedef struct sout_stream_id_sys_t sout_stream_id_sys_t;
struct sout_stream_id_sys_t
{
    sout_stream_id_sys_t *id;
    es_format_t fmt;
    vlc_tick_t i_last;
    bool b_error;
    struct vlc_list node;
};

typedef struct
{
    vlc_tick_t last_pcr;
    vlc_tick_t drop_delay;
    struct vlc_list ids;
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
    vlc_list_append(&p_es->node, &p_sys->ids);
    return p_es;
}

static void Del( sout_stream_t *p_stream, void *_p_es )
{
    sout_stream_id_sys_t *p_es = (sout_stream_id_sys_t *)_p_es;

    if( p_es->id != NULL )
        sout_StreamIdDel( p_stream->p_next, p_es->id );

    vlc_list_remove(&p_es->node);
    es_format_Clean( &p_es->fmt );
    free( p_es );
}

static int Send( sout_stream_t *p_stream, void *_p_es, block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;
    sout_stream_id_sys_t *p_es = (sout_stream_id_sys_t *)_p_es;

    p_es->i_last = ( p_buffer->i_dts != VLC_TICK_INVALID ) ? p_buffer->i_dts
                                                           : p_sys->last_pcr;
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

    vlc_list_foreach (p_es, &p_sys->ids, node)
        if (p_es->id != NULL
         && (p_es->fmt.i_cat == VIDEO_ES || p_es->fmt.i_cat == AUDIO_ES)
         && p_es->i_last + p_sys->drop_delay < p_sys->last_pcr )
        {
            sout_StreamIdDel(p_stream->p_next, p_es->id);
            p_es->id = NULL;
        }

    return VLC_SUCCESS;
}

static void SetPCR( sout_stream_t *stream, vlc_tick_t pcr )
{
    sout_stream_sys_t *sys = stream->p_sys;
    sys->last_pcr = pcr;

    sout_StreamSetPCR( stream->p_next, pcr );
}

#define SOUT_CFG_PREFIX "sout-autodel-"

static const struct sout_stream_operations ops = {
    Add, Del, Send, NULL, NULL, SetPCR,
};

static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = vlc_obj_malloc(p_this, sizeof (*p_sys));

    static const char *sout_options[] = {"drop-delay", NULL};
    config_ChainParse(p_stream, SOUT_CFG_PREFIX, sout_options, p_stream->p_cfg);

    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    p_sys->last_pcr = VLC_TICK_INVALID;
    p_sys->drop_delay = VLC_TICK_FROM_MS(
        var_GetInteger(p_stream, SOUT_CFG_PREFIX "drop-delay"));
    vlc_list_init(&p_sys->ids);
    p_stream->ops = &ops;
    p_stream->p_sys = p_sys;

    return VLC_SUCCESS;
}

#define DROP_DELAY_TEXT N_("Delay (ms) before track deletion")
#define DROP_DELAY_LONGTEXT                                                    \
    N_("Specify a delay (ms) applied to incoming frame timestamps when we "    \
       "choose whether they should be dropped or not. Tweak this parameter "   \
       "if you believe your tracks are deleted too early.")


vlc_module_begin()
    set_shortname(N_("Autodel"))
    set_description(N_("Automatically add/delete input streams"))
    set_capability("sout filter", 50)
    add_shortcut("autodel")
    set_callback(Open)

    add_integer(SOUT_CFG_PREFIX "drop-delay", 0, DROP_DELAY_TEXT, DROP_DELAY_LONGTEXT)
vlc_module_end()
