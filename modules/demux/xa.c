/*****************************************************************************
 * xa.c : xa file demux module for vlc
 *****************************************************************************
 * Copyright (C) 2005 Rémi Denis-Courmont
 *
 * Authors: Rémi Denis-Courmont
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

#include <assert.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("XA demuxer") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_capability( "demux", 10 )
    set_callback( Open )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int i_query, va_list args );

typedef struct
{
    es_out_id_t     *p_es;

    unsigned int    i_data_size;
    unsigned int    i_block_frames;
    unsigned int    i_frame_size;
    unsigned int    i_bitrate;

    date_t          pts;
} demux_sys_t;

typedef struct xa_header_t
{
    char     xa_id[4];
    uint32_t iSize;

    uint16_t wFormatTag;
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
} xa_header_t;

#define HEADER_LENGTH 24

static_assert(offsetof(xa_header_t, wBitsPerSample) == 22, "Bad padding");

#define FRAME_LENGTH 28 /* samples per frame */

/*****************************************************************************
 * Open: check file and initializes structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    const uint8_t *peek;

    /* XA file heuristic */
    if( vlc_stream_Peek( p_demux->s, &peek, 10 ) < 10 )
        return VLC_EGENERIC;
    if( memcmp( peek, "XAI", 4 ) && memcmp( peek, "XAJ", 4 ) &&
            memcmp( peek, "XA\0", 4 ) )
        return VLC_EGENERIC;
    if( GetWLE( peek + 8 ) != 1 ) /* format tag */
        return VLC_EGENERIC;

    demux_sys_t *p_sys = vlc_obj_malloc( p_this, sizeof (*p_sys) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    /* read XA header*/
    xa_header_t xa;

    if( vlc_stream_Read( p_demux->s, &xa, HEADER_LENGTH ) < HEADER_LENGTH )
        return VLC_EGENERIC;

    es_format_t fmt;
    es_format_Init( &fmt, AUDIO_ES, VLC_CODEC_ADPCM_XA_EA );

    msg_Dbg( p_demux, "assuming EA ADPCM audio codec" );
    fmt.audio.i_rate = GetDWLE( &xa.nSamplesPerSec );
    fmt.audio.i_bytes_per_frame = 15 * GetWLE( &xa.nChannels );
    fmt.audio.i_frame_length = FRAME_LENGTH;

    fmt.audio.i_channels = GetWLE ( &xa.nChannels );
    fmt.audio.i_blockalign = fmt.audio.i_bytes_per_frame;
    fmt.audio.i_bitspersample = GetWLE( &xa.wBitsPerSample );
    fmt.i_bitrate = (fmt.audio.i_rate * fmt.audio.i_bytes_per_frame * 8)
                    / fmt.audio.i_frame_length;

    /* FIXME: better computation */
    p_sys->i_data_size = xa.iSize * 15 / 56;
    /* How many frames per block (1:1 is too CPU intensive) */
    p_sys->i_block_frames = fmt.audio.i_rate / (FRAME_LENGTH * 20) + 1;
    p_sys->i_frame_size = fmt.audio.i_bytes_per_frame;
    p_sys->i_bitrate = fmt.i_bitrate;

    msg_Dbg( p_demux, "fourcc: %4.4s, channels: %d, "
             "freq: %d Hz, bitrate: %dKo/s, blockalign: %d",
             (char *)&fmt.i_codec, fmt.audio.i_channels, fmt.audio.i_rate,
             fmt.i_bitrate / 8192, fmt.audio.i_blockalign );

    if( fmt.audio.i_rate == 0 || fmt.audio.i_channels == 0
     || fmt.audio.i_bitspersample != 16 )
        return VLC_EGENERIC;

    p_sys->p_es = es_out_Add( p_demux->out, &fmt );
    if( unlikely(p_sys->p_es == NULL) )
        return VLC_ENOMEM;

    date_Init( &p_sys->pts, fmt.audio.i_rate, 1 );
    date_Set( &p_sys->pts, VLC_TICK_0 );

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys      = p_sys;
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
    int64_t     i_offset = vlc_stream_Tell( p_demux->s );
    unsigned    i_frames = p_sys->i_block_frames;

    if( p_sys->i_data_size > 0 &&
        (i_offset - HEADER_LENGTH) >= p_sys->i_data_size )
    {
        return VLC_DEMUXER_EOF;
    }

    p_block = vlc_stream_Block( p_demux->s, p_sys->i_frame_size * i_frames );
    if( p_block == NULL )
    {
        msg_Warn( p_demux, "cannot read data" );
        return VLC_DEMUXER_EOF;
    }

    i_frames = p_block->i_buffer / p_sys->i_frame_size;
    p_block->i_dts = p_block->i_pts = date_Get( &p_sys->pts );
    es_out_SetPCR( p_demux->out, p_block->i_pts );
    es_out_Send( p_demux->out, p_sys->p_es, p_block );

    date_Increment( &p_sys->pts, i_frames * FRAME_LENGTH );

    return VLC_DEMUXER_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;

    return demux_vaControlHelper( p_demux->s, HEADER_LENGTH,
        p_sys->i_data_size ? (int64_t)HEADER_LENGTH + p_sys->i_data_size : -1,
                                  p_sys->i_bitrate, p_sys->i_frame_size,
                                  i_query, args );
}
