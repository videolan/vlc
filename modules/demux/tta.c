/*****************************************************************************
 * tta.c : The Lossless True Audio parser
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
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
#include <math.h>
#include <limits.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );

vlc_module_begin ()
    set_shortname( "TTA" )
    set_description( N_("TTA demuxer") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_capability( "demux", 145 )

    set_callbacks( Open, Close )
    add_shortcut( "tta" )
vlc_module_end ()

#define TTA_FRAMETIME 1.04489795918367346939

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );

typedef struct
{
    /* */
    es_out_id_t *p_es;

    /* */
    uint32_t i_totalframes;
    uint32_t i_currentframe;
    uint32_t *pi_seektable;
    uint32_t i_datalength;
    int      i_framelength;

    /* */
    vlc_meta_t     *p_meta;
    int64_t        i_start;
} demux_sys_t;

/*****************************************************************************
 * Open: initializes ES structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    es_format_t fmt;
    const uint8_t *p_peek;
    uint8_t     p_header[22];
    uint8_t     *p_fullheader;
    int         i_seektable_size = 0;
    //char        psz_info[4096];
    //module_t    *p_id3;

    if( vlc_stream_Peek( p_demux->s, &p_peek, 4 ) < 4 )
        return VLC_EGENERIC;

    if( memcmp( p_peek, "TTA1", 4 ) )
    {
        if( !p_demux->obj.force )
            return VLC_EGENERIC;

        /* User forced */
        msg_Err( p_demux, "this doesn't look like a true-audio stream, "
                 "continuing anyway" );
    }

    if( vlc_stream_Read( p_demux->s, p_header, 22 ) < 22 )
        return VLC_EGENERIC;

    /* Fill p_demux fields */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->pi_seektable = NULL;

    /* Read the metadata */
    es_format_Init( &fmt, AUDIO_ES, VLC_CODEC_TTA );
    fmt.audio.i_channels = GetWLE( &p_header[6] );
    fmt.audio.i_bitspersample = GetWLE( &p_header[8] );
    fmt.audio.i_rate = GetDWLE( &p_header[10] );
    if( fmt.audio.i_rate == 0 || /* Avoid divide by 0 */
        fmt.audio.i_rate > ( 1 << 20 ) /* Avoid i_framelength overflow */ )
    {
        msg_Warn( p_demux, "Wrong sample rate" );
        goto error;
    }

    p_sys->i_datalength = GetDWLE( &p_header[14] );
    p_sys->i_framelength = TTA_FRAMETIME * fmt.audio.i_rate;

    p_sys->i_totalframes = p_sys->i_datalength / p_sys->i_framelength +
                          ((p_sys->i_datalength % p_sys->i_framelength) != 0);
    p_sys->i_currentframe = 0;
    if( (INT_MAX - 22 - 4) / sizeof(uint32_t) < p_sys->i_totalframes )
        goto error;

    i_seektable_size = sizeof(uint32_t)*p_sys->i_totalframes;

    /* Store the header and Seektable for avcodec */
    fmt.i_extra = 22 + i_seektable_size + 4;
    fmt.p_extra = p_fullheader = malloc( fmt.i_extra );
    if( !p_fullheader )
    {
        fmt.i_extra = 0;
        goto error;
    }

    memcpy( p_fullheader, p_header, 22 );
    p_fullheader += 22;
    if( vlc_stream_Read( p_demux->s, p_fullheader, i_seektable_size )
             != i_seektable_size )
        goto error;

    p_sys->pi_seektable = calloc( p_sys->i_totalframes, sizeof(uint32_t) );
    if( !p_sys->pi_seektable )
        goto error;
    for( uint32_t i = 0; i < p_sys->i_totalframes; i++ )
    {
        p_sys->pi_seektable[i] = GetDWLE( p_fullheader );
        p_fullheader += 4;
    }

    if( 4 != vlc_stream_Read( p_demux->s, p_fullheader, 4 ) ) /* CRC */
        goto error;
    p_fullheader += 4;

    p_sys->p_es = es_out_Add( p_demux->out, &fmt );
    p_sys->i_start = p_fullheader - (uint8_t *)fmt.p_extra;
    es_format_Clean( &fmt );

    return VLC_SUCCESS;
error:
    es_format_Clean( &fmt );
    Close( p_this );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t        *p_demux = (demux_t*)p_this;
    demux_sys_t    *p_sys = p_demux->p_sys;

    free( p_sys->pi_seektable );
    free( p_sys );
}

/*****************************************************************************
 * Demux:
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_data;

    if( p_sys->i_currentframe >= p_sys->i_totalframes )
        return VLC_DEMUXER_EOF;

    p_data = vlc_stream_Block( p_demux->s,
                               p_sys->pi_seektable[p_sys->i_currentframe] );
    if( p_data == NULL )
        return VLC_DEMUXER_EOF;
    p_data->i_dts = p_data->i_pts = VLC_TICK_0 + vlc_tick_from_sec( p_sys->i_currentframe * TTA_FRAMETIME );

    p_sys->i_currentframe++;

    es_out_SetPCR( p_demux->out, p_data->i_dts );
    if( p_sys->p_es )
        es_out_Send( p_demux->out, p_sys->p_es, p_data );

    return VLC_DEMUXER_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double   f, *pf;
    int64_t i64;

    switch( i_query )
    {
        case DEMUX_CAN_SEEK:
            return vlc_stream_vaControl( p_demux->s, i_query, args );

        case DEMUX_GET_POSITION:
            pf = va_arg( args, double * );
            i64 = stream_Size( p_demux->s ) - p_sys->i_start;
            if( i64 > 0 )
            {
                *pf = (double)(vlc_stream_Tell( p_demux->s ) - p_sys->i_start )/ (double)i64;
            }
            else
            {
                *pf = 0.0;
            }
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            f = va_arg( args, double );
            i64 = (int64_t)(f * (stream_Size( p_demux->s ) - p_sys->i_start));
            if( i64 > 0 )
            {
                int64_t tmp = 0;
                uint32_t i;
                for( i=0; i < p_sys->i_totalframes && tmp+p_sys->pi_seektable[i] < i64; i++)
                {
                    tmp += p_sys->pi_seektable[i];
                }
                if( vlc_stream_Seek( p_demux->s, tmp+p_sys->i_start ) )
                    return VLC_EGENERIC;
                p_sys->i_currentframe = i;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_LENGTH:
            *va_arg( args, vlc_tick_t * ) =
                vlc_tick_from_sec( p_sys->i_totalframes * TTA_FRAMETIME );
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            *va_arg( args, vlc_tick_t * ) = vlc_tick_from_sec( p_sys->i_currentframe * TTA_FRAMETIME );
            return VLC_SUCCESS;

        case DEMUX_CAN_PAUSE:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_GET_PTS_DELAY:
            return demux_vaControlHelper( p_demux->s, 0, p_sys->i_datalength,
                                          0, p_sys->i_framelength, i_query, args );

        default:
            return VLC_EGENERIC;
    }
}

