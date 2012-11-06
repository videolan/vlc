/*****************************************************************************
 * cdg.c : cdg file demux module for vlc
 *****************************************************************************
 * Copyright (C) 2007 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir # via.ecp.fr>
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
#include <vlc_demux.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("CDG demuxer") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_capability( "demux", 3 )
    set_callbacks( Open, Close )
    add_shortcut( "cdg", "subtitle" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int i_query, va_list args );

struct demux_sys_t
{
    es_format_t     fmt;
    es_out_id_t     *p_es;

    date_t          pts;
};

#define CDG_FRAME_SIZE (96)
#define CDG_FRAME_RATE (75)

/*****************************************************************************
 * Open: check file and initializes structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    /* Identify cdg file by extension, as there is no simple way to
     * detect it */
    if( !demux_IsPathExtension( p_demux, ".cdg" ) && !demux_IsForced( p_demux, "cdg" ) )
        return VLC_EGENERIC;

    /* CDG file size has to be multiple of CDG_FRAME_SIZE (it works even
     * if size is unknown ie 0) */
//    if( (stream_Size( p_demux->s ) % CDG_FRAME_SIZE) != 0 )
//    {
//        msg_Err( p_demux, "Reject CDG file based on its size" );
//        return VLC_EGENERIC;
//    }

    p_sys = malloc( sizeof( demux_sys_t ) );
    if( unlikely(p_sys == NULL) )
        return VLC_ENOMEM;

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys      = p_sys;

    /* */
    es_format_Init( &p_sys->fmt, VIDEO_ES, VLC_CODEC_CDG );
    p_sys->fmt.video.i_width  = 300-2*6;
    p_sys->fmt.video.i_height = 216-2*12 ;

    p_sys->p_es = es_out_Add( p_demux->out, &p_sys->fmt );

    /* There is CDG_FRAME_RATE frames per second */
    date_Init( &p_sys->pts, CDG_FRAME_RATE, 1 );
    date_Set( &p_sys->pts, 0 );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Demux: read packet and send them to decoders
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_block;

    p_block = stream_Block( p_demux->s, CDG_FRAME_SIZE );
    if( p_block == NULL )
    {
        msg_Dbg( p_demux, "cannot read data, eof" );
        return 0;
    }

    p_block->i_dts =
    p_block->i_pts = VLC_TS_0 + date_Get( &p_sys->pts );

    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block->i_pts );

    es_out_Send( p_demux->out, p_sys->p_es, p_block );

    date_Increment( &p_sys->pts, 1 );

    return 1;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close ( vlc_object_t * p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    int i_ret = demux_vaControlHelper( p_demux->s, 0, -1,
                                       8*CDG_FRAME_SIZE*CDG_FRAME_RATE, CDG_FRAME_SIZE,
                                       i_query, args );
    if( !i_ret && ( i_query == DEMUX_SET_POSITION || i_query == DEMUX_SET_TIME ) )
        date_Set( &p_demux->p_sys->pts,
                  stream_Tell( p_demux->s ) / CDG_FRAME_SIZE *
                    INT64_C(1000000) / CDG_FRAME_RATE );

    return i_ret;
}

