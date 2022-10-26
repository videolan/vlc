/*****************************************************************************
 * delay.c: delay a stream
 *****************************************************************************
 * Copyright © 2009-2011 VLC authors and VideoLAN
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

#define SOUT_CFG_PREFIX "sout-delay-"

typedef struct
{
    void *id;
    int i_id;
    vlc_tick_t i_delay;
} sout_stream_sys_t;

static void *Add( sout_stream_t *p_stream, const es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;

    if ( p_fmt->i_id == p_sys->i_id )
    {
        msg_Dbg( p_stream, "delaying ID %d by %"PRId64,
                 p_sys->i_id, p_sys->i_delay );
        p_sys->id = sout_StreamIdAdd( p_stream->p_next, p_fmt );
        return p_sys->id;
    }

    return sout_StreamIdAdd( p_stream->p_next, p_fmt );
}

static void Del( sout_stream_t *p_stream, void *id )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;

    if ( id == p_sys->id )
        p_sys->id = NULL;

    sout_StreamIdDel( p_stream->p_next, id );
}

static void block_ChainApplyDelay( block_t *chain, vlc_tick_t delay )
{
    for ( block_t *frame = chain; frame != NULL; frame = frame->p_next )
    {
        if ( frame->i_dts != VLC_TICK_INVALID )
            frame->i_dts += delay;
        if ( frame->i_pts != VLC_TICK_INVALID )
            frame->i_pts += delay;
    }
}

static int Send( sout_stream_t *p_stream, void *id, block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;

    /**
     * Positive delay is added to the selected ES timestamps while negative
     * delay is added to every other ES except the selected one. This avoids
     * any negative timestamps.
     */
    if ( p_sys->i_delay < 0 && id != p_sys->id )
        block_ChainApplyDelay( p_buffer, -p_sys->i_delay );
    else if ( p_sys->i_delay > 0 && id == p_sys->id )
        block_ChainApplyDelay( p_buffer, p_sys->i_delay );

    return sout_StreamIdSend( p_stream->p_next, id, p_buffer );
}

static void SetPCR( sout_stream_t *stream, vlc_tick_t pcr )
{
    sout_StreamSetPCR( stream->p_next, pcr );
}

static const struct sout_stream_operations ops = {
    Add, Del, Send, NULL, NULL, SetPCR,
};

static const char *ppsz_sout_options[] = {
    "id", "delay", NULL
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;

    p_sys = calloc( 1, sizeof( sout_stream_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;


    config_ChainParse( p_stream, SOUT_CFG_PREFIX, ppsz_sout_options,
                   p_stream->p_cfg );

    p_sys->i_id = var_GetInteger( p_stream, SOUT_CFG_PREFIX "id" );
    p_sys->i_delay = VLC_TICK_FROM_MS(var_GetInteger( p_stream, SOUT_CFG_PREFIX "delay" ));

    p_stream->ops = &ops;
    p_stream->p_sys = p_sys;
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

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ID_TEXT N_("Elementary Stream ID")
#define ID_LONGTEXT N_( \
    "Specify an identifier integer for this elementary stream" )

#define DELAY_TEXT N_("Delay of the ES (ms)")
#define DELAY_LONGTEXT N_( \
    "Specify a delay (in ms) for this elementary stream. " \
    "Positive means delay and negative means advance." )

vlc_module_begin()
    set_shortname(N_("Delay"))
    set_description(N_("Delay a stream"))
    set_capability("sout filter", 50)
    add_shortcut("delay")
    set_subcategory(SUBCAT_SOUT_STREAM)
    set_callbacks(Open, Close)
    add_integer(SOUT_CFG_PREFIX "id", 0, ID_TEXT, ID_LONGTEXT)
    add_integer(SOUT_CFG_PREFIX "delay", 0, DELAY_TEXT, DELAY_LONGTEXT)
vlc_module_end()
