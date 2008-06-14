/*****************************************************************************
 * demux.c: stats demux plugin
 *****************************************************************************
 * Copyright (C) 2001-2008 the VideoLAN team
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Pierre d'Herbemont <pdherbemont@videolan.org>
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
#include <vlc_interface.h>
#include <vlc_access.h>
#include <vlc_demux.h>

#include "stats.h"


/*****************************************************************************
 * Demux
 *****************************************************************************/


struct demux_sys_t
{
    es_format_t     fmt;
    es_out_id_t     *p_es;

    date_t          pts;
};

static int Demux( demux_t * );
static int DemuxControl( demux_t *, int, va_list );


/*****************************************************************************
 * OpenDemux
 *****************************************************************************/
int OpenDemux ( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    p_demux->p_sys = NULL;

    /* Only when selected */
    if( *p_demux->psz_demux == '\0' )
        return VLC_EGENERIC;

    msg_Dbg( p_demux, "Init Stat demux" );

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = DemuxControl;

    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    if( !p_demux->p_sys )
        return VLC_ENOMEM;

    date_Init( &p_sys->pts, 1, 1 );
    date_Set( &p_sys->pts, 1 );

    es_format_Init( &p_sys->fmt, VIDEO_ES, VLC_FOURCC('s','t','a','t') );
    p_sys->fmt.video.i_width = 720;
    p_sys->fmt.video.i_height= 480;

    p_sys->p_es = es_out_Add( p_demux->out, &p_sys->fmt );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseDemux
 *****************************************************************************/
void CloseDemux ( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;

    msg_Dbg( p_demux, "Closing Stat demux" );

    free( p_demux->p_sys );
}

/*****************************************************************************
 * Demux
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    block_t * p_block = stream_Block( p_demux->s, kBufferSize );

    if( !p_block ) return 1;

    p_block->i_dts = p_block->i_pts =
        date_Increment( &p_sys->pts, kBufferSize );

    msg_Dbg( p_demux, "demux got %d ms offset", (int)(mdate() - *(mtime_t *)p_block->p_buffer) / 1000 );

    //es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block->i_pts );

    es_out_Send( p_demux->out, p_sys->p_es, p_block );

    return 1;
}

static int DemuxControl( demux_t *p_demux, int i_query, va_list args )
{
    return demux_vaControlHelper( p_demux->s,
                                   0, 0, 0, 1,
                                   i_query, args );
}
