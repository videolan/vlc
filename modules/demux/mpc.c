/*****************************************************************************
 * mpc.c : MPC stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 the VideoLAN team
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr.com>
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
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_codec.h>
#include <math.h>

#include <mpc/mpcdec.h>

/* TODO:
 *  - test stream version 4..6
 *  - test fixed float version
 *  - ...
 *
 *  XXX:
 *  It is done the ugly way (the demux does the decode stage... but it works
 */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open  ( vlc_object_t * );

vlc_module_begin ()
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_("MusePack demuxer") )
    set_capability( "demux", 145 )

    set_callback( Open )
    add_shortcut( "mpc" )
    add_file_extension("mpc")
    add_file_extension("mp+")
    add_file_extension("mpp")
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );

typedef struct
{
    /* */
    es_out_id_t   *p_es;

    /* */
    mpc_demux      *decoder;
    mpc_reader     reader;
    mpc_streaminfo info;

    /* */
    mpc_uint64_t   i_position;
} demux_sys_t;

static mpc_int32_t ReaderRead(mpc_reader *, void *dst, mpc_int32_t i_size);
static mpc_bool_t  ReaderSeek(mpc_reader *, mpc_int32_t i_offset);
static mpc_int32_t ReaderTell(mpc_reader *);
static mpc_int32_t ReaderGetSize(mpc_reader *);
static mpc_bool_t  ReaderCanSeek(mpc_reader *);

/*****************************************************************************
 * Open: initializes ES structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    es_format_t fmt;
    const uint8_t *p_peek;

    if( vlc_stream_Peek( p_demux->s, &p_peek, 4 ) < 4 )
        return VLC_EGENERIC;

    if( memcmp( p_peek, "MP+", 3 ) )
    {
        /* for v4..6 we check extension file */
        const int i_version = (GetDWLE( p_peek ) >> 11)&0x3ff;

        if( i_version  < 4 || i_version > 6 )
            return VLC_EGENERIC;

        if( !p_demux->obj.force )
            return VLC_EGENERIC;
    }

    /* */
    p_sys = vlc_obj_calloc( p_this, 1, sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->i_position = 0;

    p_sys->reader.read = ReaderRead;
    p_sys->reader.seek = ReaderSeek;
    p_sys->reader.tell = ReaderTell;
    p_sys->reader.get_size = ReaderGetSize;
    p_sys->reader.canseek = ReaderCanSeek;
    p_sys->reader.data = p_demux->s;

    /* */
    p_sys->decoder = mpc_demux_init( &p_sys->reader );
    if( !p_sys->decoder )
        return VLC_EGENERIC;

    /* Load info */
    mpc_demux_get_info( p_sys->decoder, &p_sys->info );

    /* Fill p_demux fields */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys;

    /* */
#ifndef MPC_FIXED_POINT
    es_format_Init( &fmt, AUDIO_ES, VLC_CODEC_FL32 );
#else
#   ifdef WORDS_BIGENDIAN
    es_format_Init( &fmt, AUDIO_ES, VLC_CODEC_S32B );
#   else
    es_format_Init( &fmt, AUDIO_ES, VLC_CODEC_S32L );
#   endif
#endif
    fmt.audio.i_channels = p_sys->info.channels;
    fmt.audio.i_rate = p_sys->info.sample_freq;
    fmt.audio.i_blockalign = 4*fmt.audio.i_channels;
    fmt.audio.i_bitspersample = 32;
    fmt.i_bitrate = fmt.i_bitrate * fmt.audio.i_channels *
                    fmt.audio.i_bitspersample;

#ifdef HAVE_MPC_MPCDEC_H
#   define CONVERT_PEAK( mpc_peak ) (pow( 10, (mpc_peak) / 256.0 / 20.0 ) / 32767.0)
#   define CONVERT_GAIN( mpc_gain ) (MPC_OLD_GAIN_REF - (mpc_gain) / 256.0)
#else
#   define CONVERT_PEAK( mpc_peak ) ((mpc_peak) / 32767.0)
#   define CONVERT_GAIN( mpc_gain ) ((mpc_gain) / 100.0)
#endif

    if( p_sys->info.peak_title > 0 )
    {
        fmt.audio_replay_gain.pb_peak[AUDIO_REPLAY_GAIN_TRACK] = true;
        fmt.audio_replay_gain.pf_peak[AUDIO_REPLAY_GAIN_TRACK] = (float) CONVERT_PEAK( p_sys->info.peak_title );
        fmt.audio_replay_gain.pb_gain[AUDIO_REPLAY_GAIN_TRACK] = true;
        fmt.audio_replay_gain.pf_gain[AUDIO_REPLAY_GAIN_TRACK] = (float) CONVERT_GAIN( p_sys->info.gain_title );
    }
    if( p_sys->info.peak_album > 0 )
    {
        fmt.audio_replay_gain.pb_peak[AUDIO_REPLAY_GAIN_ALBUM] = true;
        fmt.audio_replay_gain.pf_peak[AUDIO_REPLAY_GAIN_ALBUM] = (float) CONVERT_PEAK( p_sys->info.peak_album );
        fmt.audio_replay_gain.pb_gain[AUDIO_REPLAY_GAIN_ALBUM] = true;
        fmt.audio_replay_gain.pf_gain[AUDIO_REPLAY_GAIN_ALBUM] = (float) CONVERT_GAIN( p_sys->info.gain_album );
    }

#undef CONVERT_GAIN
#undef CONVERT_PEAK

    fmt.i_id = 0;
    p_sys->p_es = es_out_Add( p_demux->out, &fmt );
    if( !p_sys->p_es )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
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
    mpc_frame_info frame;
    mpc_status err;

    p_data = block_Alloc( MPC_DECODER_BUFFER_LENGTH*sizeof(MPC_SAMPLE_FORMAT) );
    if( unlikely(!p_data) )
        return VLC_DEMUXER_EGENERIC;

    frame.buffer = (MPC_SAMPLE_FORMAT*)p_data->p_buffer;
    err = mpc_demux_decode( p_sys->decoder, &frame );
    if( err != MPC_STATUS_OK )
    {
        block_Release( p_data );
        return VLC_DEMUXER_EGENERIC;
    }
    else if( frame.bits == -1 || frame.samples == 0 )
    {
        block_Release( p_data );
        return VLC_DEMUXER_EOF;
    }

    /* */
    p_data->i_buffer = frame.samples * sizeof(MPC_SAMPLE_FORMAT) * p_sys->info.channels;
    p_data->i_dts = p_data->i_pts =
            VLC_TICK_0 + vlc_tick_from_samples(p_sys->i_position, p_sys->info.sample_freq);

    es_out_SetPCR( p_demux->out, p_data->i_dts );

    es_out_Send( p_demux->out, p_sys->p_es, p_data );

    /* */
    p_sys->i_position += frame.samples;

    return VLC_DEMUXER_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    switch( i_query )
    {
        case DEMUX_CAN_SEEK:
            return vlc_stream_vaControl( p_demux->s, i_query, args );

        case DEMUX_HAS_UNSUPPORTED_META:
            *va_arg( args, bool* ) = true;
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            *va_arg( args, vlc_tick_t * ) =
                vlc_tick_from_samples(p_sys->info.samples, p_sys->info.sample_freq);
            return VLC_SUCCESS;

        case DEMUX_GET_POSITION:
            *va_arg( args, double * ) = (double)p_sys->i_position /
                                                p_sys->info.samples;
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            *va_arg( args, vlc_tick_t * ) =
                vlc_tick_from_samples(p_sys->i_position, p_sys->info.sample_freq);
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
        {
            mpc_uint64_t i64 = va_arg( args, double ) * p_sys->info.samples;
            if( mpc_demux_seek_sample( p_sys->decoder, i64 ) )
            {
                p_sys->i_position = i64;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        }

        case DEMUX_SET_TIME:
        {
            vlc_tick_t i64 = va_arg( args, vlc_tick_t );
            if( mpc_demux_seek_sample( p_sys->decoder, i64 ) )
            {
                p_sys->i_position = i64;
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        }

        case DEMUX_CAN_PAUSE:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_GET_PTS_DELAY:
            return demux_vaControlHelper( p_demux->s, 0, -1, 0, 1, i_query, args );

        default:
            return VLC_EGENERIC;
    }
}

mpc_int32_t ReaderRead(mpc_reader *p_private, void *dst, mpc_int32_t i_size)
{
    stream_t *stream = p_private->data;
    return vlc_stream_Read( stream, dst, i_size );
}

mpc_bool_t ReaderSeek(mpc_reader *p_private, mpc_int32_t i_offset)
{
    stream_t *stream = p_private->data;
    return !vlc_stream_Seek( stream, i_offset );
}

mpc_int32_t ReaderTell(mpc_reader *p_private)
{
    stream_t *stream = p_private->data;
    return vlc_stream_Tell( stream );
}

mpc_int32_t ReaderGetSize(mpc_reader *p_private)
{
    stream_t *stream = p_private->data;
    return stream_Size( stream );
}

mpc_bool_t ReaderCanSeek(mpc_reader *p_private)
{
    stream_t *stream = p_private->data;
    bool b_canseek;

    vlc_stream_Control( stream, STREAM_CAN_SEEK, &b_canseek );
    return b_canseek;
}

