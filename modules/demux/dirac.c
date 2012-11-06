/*****************************************************************************
 * dirac.c : Dirac Video demuxer
 *****************************************************************************
 * Copyright (C) 2002-2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: David Flynn <davidf@rd.bbc.co.uk>
 * Based on vc1.c by: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc_codec.h>

#define DEMUX_CFG_PREFIX "dirac-"

#define DEMUX_DTSOFFSET "dts-offset"
#define DEMUX_DTSOFFSET_TEXT N_("Value to adjust dts by")
#define DEMUX_DTSOFFSET_LONGTEXT DEMUX_DTSOFFSET_TEXT

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_shortname( "Dirac");
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_description( N_("Dirac video demuxer" ) );
    set_capability( "demux", 50 );
    add_integer( DEMUX_CFG_PREFIX DEMUX_DTSOFFSET, 0,
                 DEMUX_DTSOFFSET_TEXT, DEMUX_DTSOFFSET_LONGTEXT, false )
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct demux_sys_t
{
    mtime_t i_dts;
    mtime_t i_dtsoffset;
    mtime_t i_pts_offset_lowtide;
    es_out_id_t *p_es;

    enum {
        /* demuxer states, do not reorder (++ is used) */
        DIRAC_DEMUX_DISCONT = 0, /* signal a discontinuity to packetizer */
        DIRAC_DEMUX_FIRST, /* provide an origin timestamp for the packetizer */
        DIRAC_DEMUX_STEADY, /* normal operation */
    } i_state;

    decoder_t *p_packetizer;
};

static int Demux( demux_t * );
static int Control( demux_t *, int, va_list );

#define DIRAC_PACKET_SIZE 4096

/*****************************************************************************
 * Open: initializes demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    const uint8_t *p_peek;
    es_format_t fmt;

    if( stream_Peek( p_demux->s, &p_peek, 5 ) < 5 ) return VLC_EGENERIC;

    if( p_peek[0] != 'B' || p_peek[1] != 'B' ||
        p_peek[2] != 'C' || p_peek[3] != 'D') /* start of ParseInfo */
    {
        if( !p_demux->b_force ) return VLC_EGENERIC;

        msg_Err( p_demux, "This doesn't look like a Dirac stream (incorrect parsecode)" );
        msg_Warn( p_demux, "continuing anyway" );
    }

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = calloc( 1, sizeof( demux_sys_t ) );
    if( !p_sys ) return VLC_ENOMEM;

    p_sys->i_pts_offset_lowtide = INT64_MAX;
    p_sys->i_state = DIRAC_DEMUX_FIRST;

    p_sys->i_dtsoffset = var_CreateGetInteger( p_demux, DEMUX_CFG_PREFIX DEMUX_DTSOFFSET );

    /* Load the packetizer */
    es_format_Init( &fmt, VIDEO_ES, VLC_CODEC_DIRAC );
    p_sys->p_packetizer = demux_PacketizerNew( p_demux, &fmt, "dirac" );
    if( !p_sys->p_packetizer )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    demux_PacketizerDestroy( p_sys->p_packetizer );

    if( p_sys->i_pts_offset_lowtide < INT64_MAX &&
        p_sys->i_pts_offset_lowtide > 0 )
    {
        msg_Warn( p_demux, "For all packets seen, pts-dts (%"PRId64") could be reduced to 0",
                  p_sys->i_pts_offset_lowtide );
    }
    free( p_sys );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_block_in, *p_block_out;

    if( p_sys->i_state == DIRAC_DEMUX_DISCONT )
    {
        p_sys->i_state++;
        p_block_in = block_Alloc( 128 );
        if( p_block_in )
        {
            p_block_in->i_flags = BLOCK_FLAG_DISCONTINUITY | BLOCK_FLAG_CORRUPTED;
        }
    }
    else
    {
        p_block_in = stream_Block( p_demux->s, DIRAC_PACKET_SIZE );
        if( !p_block_in )
        {
            return 0;
        }
        if ( p_sys->i_state == DIRAC_DEMUX_FIRST)
        {
            p_sys->i_state++;
            /* by default, timestamps are invalid.
             * Except when we need an anchor point */
            p_block_in->i_dts = VLC_TS_0;
        }
    }

    while( (p_block_out = p_sys->p_packetizer->pf_packetize( p_sys->p_packetizer, &p_block_in )) )
    {
        while( p_block_out )
        {
            block_t *p_next = p_block_out->p_next;
            p_block_out->p_next = NULL;

            if( p_sys->p_es == NULL )
                p_sys->p_es = es_out_Add( p_demux->out, &p_sys->p_packetizer->fmt_out);

            p_block_out->i_dts += p_sys->i_dtsoffset;
            p_sys->i_dts = p_block_out->i_dts;

            /* track low watermark for pts_offset -- can be used to show
             * when it is too large */
            mtime_t i_delay = p_block_out->i_pts - p_block_out->i_dts;
            if( p_sys->i_pts_offset_lowtide > i_delay )
            {
                p_sys->i_pts_offset_lowtide = i_delay;
            }

            es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block_out->i_dts );
            es_out_Send( p_demux->out, p_sys->p_es, p_block_out );

            p_block_out = p_next;
        }
    }
    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;
    if( DEMUX_GET_TIME == i_query )
    {
        int64_t *pi64 = (int64_t*)va_arg( args, int64_t * );
        *pi64 = p_sys->i_dts;
        return VLC_SUCCESS;
    }
    else if( DEMUX_GET_FPS == i_query )
    {
        if( !p_sys->p_packetizer->fmt_out.video.i_frame_rate )
        {
            return VLC_EGENERIC;
        }
        double *pd = (double*)va_arg( args, double * );
        *pd = (float) p_sys->p_packetizer->fmt_out.video.i_frame_rate
            / p_sys->p_packetizer->fmt_out.video.i_frame_rate_base;
        return VLC_SUCCESS;
    }
    else
    {
        if( DEMUX_SET_POSITION == i_query || DEMUX_SET_TIME == i_query )
        {
            p_sys->i_state = DIRAC_DEMUX_DISCONT;
        }
        return demux_vaControlHelper( p_demux->s, 0, -1, 0, 1, i_query, args );
    }
}

