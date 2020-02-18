/*****************************************************************************
 * aiff.c: Audio Interchange File Format demuxer
 *****************************************************************************
 * Copyright (C) 2004-2007 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <limits.h>

/* TODO:
 *  - ...
 */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    ( vlc_object_t * );

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_("AIFF demuxer" ) )
    set_capability( "demux", 10 )
    set_callback( Open )
    add_shortcut( "aiff" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct
{
    es_format_t  fmt;
    es_out_id_t *es;

    int64_t     i_ssnd_pos;
    int64_t     i_ssnd_size;
    int         i_ssnd_offset;
    int         i_ssnd_blocksize;

    /* real data start */
    int64_t     i_ssnd_start;
    int64_t     i_ssnd_end;

    int         i_ssnd_fsize;

    vlc_tick_t  i_time;
} demux_sys_t;

static int Demux  ( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );

/* GetF80BE: read a 80 bits float in big endian */
static unsigned int GetF80BE( const uint8_t p[10] )
{
    unsigned int i_mantissa = GetDWBE( &p[2] );
    int          i_exp = 30 - p[1];
    unsigned int i_last = 0;

    while( i_exp-- > 0 )
    {
        i_last = i_mantissa;
        i_mantissa >>= 1;
    }
    if( i_last&0x01 )
    {
        i_mantissa++;
    }
    return i_mantissa;
}

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;

    const uint8_t *p_peek;

    if( vlc_stream_Peek( p_demux->s, &p_peek, 12 ) < 12 )
        return VLC_EGENERIC;
    if( memcmp( p_peek, "FORM", 4 ) || memcmp( &p_peek[8], "AIFF", 4 ) )
        return VLC_EGENERIC;

    /* skip aiff header */
    if( vlc_stream_Read( p_demux->s, NULL, 12 ) < 12 )
        return VLC_EGENERIC;

    /* Fill p_demux field */
    demux_sys_t *p_sys = vlc_obj_calloc( p_this, 1, sizeof (*p_sys) );
    es_format_Init( &p_sys->fmt, AUDIO_ES, VLC_FOURCC( 't', 'w', 'o', 's' ) );
    p_sys->i_time = 0;
    p_sys->i_ssnd_pos = -1;

    for( ;; )
    {
        if( vlc_stream_Peek( p_demux->s, &p_peek, 8 ) < 8 )
            return VLC_EGENERIC;

        uint32_t i_data_size = GetDWBE( &p_peek[4] );
        uint64_t i_chunk_size = UINT64_C( 8 ) + i_data_size + ( i_data_size & 1 );

        msg_Dbg( p_demux, "chunk fcc=%4.4s size=%" PRIu64 " data_size=%" PRIu32,
            p_peek, i_chunk_size, i_data_size );

        if( !memcmp( p_peek, "COMM", 4 ) )
        {
            if( vlc_stream_Peek( p_demux->s, &p_peek, 18+8 ) < 18+8 )
                return VLC_EGENERIC;

            p_sys->fmt.audio.i_channels = GetWBE( &p_peek[8] );
            p_sys->fmt.audio.i_bitspersample = GetWBE( &p_peek[14] );
            p_sys->fmt.audio.i_rate     = GetF80BE( &p_peek[16] );

            msg_Dbg( p_demux, "COMM: channels=%d samples_frames=%d bits=%d rate=%d",
                     GetWBE( &p_peek[8] ), GetDWBE( &p_peek[10] ), GetWBE( &p_peek[14] ),
                     GetF80BE( &p_peek[16] ) );
        }
        else if( !memcmp( p_peek, "SSND", 4 ) )
        {
            if( vlc_stream_Peek( p_demux->s, &p_peek, 8+8 ) < 8+8 )
                return VLC_EGENERIC;

            p_sys->i_ssnd_pos = vlc_stream_Tell( p_demux->s );
            p_sys->i_ssnd_size = i_data_size;
            p_sys->i_ssnd_offset = GetDWBE( &p_peek[8] );
            p_sys->i_ssnd_blocksize = GetDWBE( &p_peek[12] );

            msg_Dbg( p_demux, "SSND: (offset=%d blocksize=%d)",
                     p_sys->i_ssnd_offset, p_sys->i_ssnd_blocksize );
        }
        if( p_sys->i_ssnd_pos >= 12 && p_sys->fmt.audio.i_channels != 0 )
        {
            /* We have found the 2 needed chunks */
            break;
        }

        /* consume chunk data */
        for( ssize_t i_req; i_chunk_size; i_chunk_size -= i_req )
        {
#if SSIZE_MAX < UINT64_MAX
            i_req = __MIN( SSIZE_MAX, i_chunk_size );
#else
            i_req = i_chunk_size;
#endif
            if( vlc_stream_Read( p_demux->s, NULL, i_req ) != i_req )
            {
                msg_Warn( p_demux, "incomplete file" );
                return VLC_EGENERIC;
            }
        }
    }

    p_sys->i_ssnd_start = p_sys->i_ssnd_pos + 16 + p_sys->i_ssnd_offset;
    p_sys->i_ssnd_end   = p_sys->i_ssnd_start + p_sys->i_ssnd_size;

    p_sys->i_ssnd_fsize = p_sys->fmt.audio.i_channels *
                          ((p_sys->fmt.audio.i_bitspersample + 7) / 8);

    if( p_sys->i_ssnd_fsize <= 0 || p_sys->fmt.audio.i_rate == 0 )
    {
        msg_Err( p_demux, "invalid audio parameters" );
        return VLC_EGENERIC;
    }

    if( p_sys->i_ssnd_size <= 0 )
    {
        /* unknown */
        p_sys->i_ssnd_end = 0;
    }

    /* seek into SSND chunk */
    if( vlc_stream_Seek( p_demux->s, p_sys->i_ssnd_start ) )
    {
        msg_Err( p_demux, "cannot seek to data chunk" );
        return VLC_EGENERIC;
    }

    /* */
    p_sys->fmt.i_id = 0;
    p_sys->es = es_out_Add( p_demux->out, &p_sys->fmt );
    if( unlikely(p_sys->es == NULL) )
        return VLC_ENOMEM;

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t     i_tell = vlc_stream_Tell( p_demux->s );

    block_t     *p_block;
    int         i_read;

    if( p_sys->i_ssnd_end > 0 && i_tell >= p_sys->i_ssnd_end )
    {
        /* EOF */
        return VLC_DEMUXER_EOF;
    }

    /* Set PCR */
    es_out_SetPCR( p_demux->out, VLC_TICK_0 + p_sys->i_time);

    /* we will read 100ms at once */
    i_read = p_sys->i_ssnd_fsize * ( p_sys->fmt.audio.i_rate / 10 );
    if( p_sys->i_ssnd_end > 0 && p_sys->i_ssnd_end - i_tell < i_read )
    {
        i_read = p_sys->i_ssnd_end - i_tell;
    }
    if( ( p_block = vlc_stream_Block( p_demux->s, i_read ) ) == NULL )
    {
        return VLC_DEMUXER_EOF;
    }

    p_block->i_dts =
    p_block->i_pts = VLC_TICK_0 + p_sys->i_time;

    p_sys->i_time += vlc_tick_from_samples(p_block->i_buffer,
                                           p_sys->i_ssnd_fsize) /
                     p_sys->fmt.audio.i_rate;

    /* */
    es_out_Send( p_demux->out, p_sys->es, p_block );
    return VLC_DEMUXER_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double f, *pf;

    switch( i_query )
    {
        case DEMUX_CAN_SEEK:
            return vlc_stream_vaControl( p_demux->s, i_query, args );

        case DEMUX_GET_POSITION:
        {
            int64_t i_start = p_sys->i_ssnd_start;
            int64_t i_end   = p_sys->i_ssnd_end > 0 ? p_sys->i_ssnd_end : stream_Size( p_demux->s );
            int64_t i_tell  = vlc_stream_Tell( p_demux->s );

            pf = va_arg( args, double * );

            if( i_start < i_end )
            {
                *pf = (double)(i_tell - i_start)/(double)(i_end - i_start);
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        }

        case DEMUX_SET_POSITION:
        {
            int64_t i_start = p_sys->i_ssnd_start;
            int64_t i_end  = p_sys->i_ssnd_end > 0 ? p_sys->i_ssnd_end : stream_Size( p_demux->s );

            f = va_arg( args, double );

            if( i_start < i_end )
            {
                int     i_frame = (f * ( i_end - i_start )) / p_sys->i_ssnd_fsize;
                int64_t i_new   = i_start + i_frame * p_sys->i_ssnd_fsize;

                if( vlc_stream_Seek( p_demux->s, i_new ) )
                {
                    return VLC_EGENERIC;
                }
                p_sys->i_time = vlc_tick_from_samples( i_frame, p_sys->fmt.audio.i_rate );
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        }

        case DEMUX_GET_TIME:
            *va_arg( args, vlc_tick_t * ) = p_sys->i_time;
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
        {
            int64_t i_end  = p_sys->i_ssnd_end > 0 ? p_sys->i_ssnd_end : stream_Size( p_demux->s );

            if( p_sys->i_ssnd_start < i_end )
            {
                *va_arg( args, vlc_tick_t * ) =
                    vlc_tick_from_samples( i_end - p_sys->i_ssnd_start, p_sys->i_ssnd_fsize) / p_sys->fmt.audio.i_rate;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        }
        case DEMUX_SET_TIME:
        case DEMUX_GET_FPS:
            return VLC_EGENERIC;

        case DEMUX_CAN_PAUSE:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_GET_PTS_DELAY:
             return demux_vaControlHelper( p_demux->s, 0, -1, 0, 1, i_query, args );

        default:
            return VLC_EGENERIC;
    }
}
