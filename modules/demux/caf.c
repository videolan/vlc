/*****************************************************************************
 * caf.c: Core Audio File Format demuxer
 *****************************************************************************
 * Copyright (C) 2013 VLC authors and VideoLAN
 *
 * Authors: Matthias Keiser <matthias@tristan-inc.com>
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
#include <math.h>
#include <limits.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_codecs.h>

/* TODO:
 *
 * - handle channel layout
 * - 64 bit float LPCM is broken (sound has artifacts with little endian, only silence plays for big endian).
 */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin ()
set_category( CAT_INPUT )
set_subcategory( SUBCAT_INPUT_DEMUX )
set_description( N_( "CAF demuxer" ))
set_capability( "demux", 140 )
set_callbacks( Open, Close )
add_shortcut( "caf" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int i_query, va_list args );

typedef struct frame_span_t
{
    uint64_t i_frames;
    uint64_t i_samples;
    uint64_t i_bytes;
    uint64_t i_desc_bytes;
} frame_span_t;

typedef struct packet_table_t
{
    uint64_t i_num_packets;
    uint64_t i_num_valid_frames;
    uint32_t i_num_priming_frames;
    uint32_t i_num_remainder_frames;
    uint64_t i_descriptions_start;
} packet_table_t;

typedef struct
{
    es_format_t  fmt;
    es_out_id_t *es;
    unsigned i_max_frames;

    uint64_t i_data_offset;
    uint64_t i_data_size;

    frame_span_t position;
    packet_table_t packet_table;
} demux_sys_t;

/*
 We use this value to indicate that the data section extends until the end of the file.
*/
static const uint64_t kCHUNK_SIZE_EOF = UINT64_C( 0xffffffffffffffff );

/*****************************************************************************
 * Various Utility Functions
 *****************************************************************************/

/* ParseVarLenInteger parses a var length integer as found in the packet descriptions
 (and in the aac magic cookie). In theorie a var length integer could be bigger than
 an uint64_t, but I think it's unlikely to ever be a problem... */

static int ParseVarLenInteger( const uint8_t *p_buff, size_t i_buff_len, uint64_t *pi_value_out, uint32_t *i_len_out )
{
    *i_len_out = 0;

    uint64_t i_value = 0;
    bool finished = false;

    for( uint32_t i = 0; i < i_buff_len; i++ )
    {
        if( (( i_value >> 32 ) << 7 ) > UINT32_MAX )
        {
            return VLC_EGENERIC; /* overflow */
        }
        uint8_t i_byte = p_buff[i];
        i_value = ( i_value << 7 ) | ( i_byte & 0x7f );

        ( *i_len_out )++;

        if( !( i_byte & 0x80 ))
        {
            finished = true;
            break;
        }
    }

    if( !finished )
    {
        return VLC_EGENERIC;
    }

    *pi_value_out = i_value;

    return VLC_SUCCESS;
}

/* Utility function that reads a big endian double from the input buffer. */

static inline double GetDBLBE( const uint8_t *p )
{
    union
    {
        uint64_t uint64;
        double dbl;
    } u_64;

    u_64.uint64 = GetQWBE( p );
    return u_64.dbl;
}

/* Utility function that reads a big endian signed 32 bit integer into an unsigned 32 bit variable.
   If the read value is negative, this function returns an error.
 */

static inline int ReadBEInt32ToUInt32( const uint8_t *p, uint32_t *i_out )
{
    uint32_t i_value = GetDWBE( p );

    if( i_value > INT32_MAX ) return VLC_EGENERIC;

    *i_out = i_value;
    return VLC_SUCCESS;
}

/* Utility function that reads a big endian signed 64 bit integer into an unsigned 64 bit variable.
   If the read value is negative, this function returns an error.
*/

static inline int ReadBEInt64ToUInt64( const uint8_t *p, uint64_t *i_out )
{
    uint64_t i_value = GetQWBE( p );

    if( i_value > INT64_MAX ) return VLC_EGENERIC;

    *i_out = i_value;
    return VLC_SUCCESS;
}

static inline bool NeedsPacketTable( demux_sys_t *p_sys )
{
    return ( !p_sys->fmt.audio.i_bytes_per_frame || !p_sys->fmt.audio.i_frame_length );
}

static uint64_t TotalNumFrames( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( !NeedsPacketTable( p_sys ))
    {
        uint64_t i_data_size;

        if( p_sys->i_data_size != kCHUNK_SIZE_EOF)
        {
            i_data_size = p_sys->i_data_size;
        }
        else
        {
            int64_t i_stream_size = stream_Size( p_demux->s );
            if(i_stream_size >= 0 && (uint64_t)i_stream_size >= p_sys->i_data_offset)
                i_data_size = i_stream_size - p_sys->i_data_offset;
            else
                i_data_size = 0;
        }

        return i_data_size / p_sys->fmt.audio.i_bytes_per_frame;
    }
    else
    {
        return p_sys->packet_table.i_num_packets;
    }
}

static uint64_t TotalNumSamples( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( !NeedsPacketTable( p_sys ))
    {
        return TotalNumFrames( p_demux ) * p_sys->fmt.audio.i_frame_length;
    }
    else
    {
        return p_sys->packet_table.i_num_valid_frames + p_sys->packet_table.i_num_priming_frames +
                p_sys->packet_table.i_num_remainder_frames;
    }
}

static inline vlc_fourcc_t ReadFOURCC( const uint8_t *p )
{
    return VLC_FOURCC( p[0], p[1], p[2], p[3] );
}

/*****************************************************************************
 * FrameSpan Functions
 *****************************************************************************
 *  A FrameSpan structure contains the relationship between a number of
 frames, the number of samples they contain, the amount of data they
 use, and the length of their packet descriptions (if any).
 *****************************************************************************/

/* FrameSpanAddSpan adds span2 to span1 */

static inline void FrameSpanAddSpan( frame_span_t *span1, frame_span_t *span2 )
{
    span1->i_frames += span2->i_frames;
    span1->i_samples += span2->i_samples;
    span1->i_bytes += span2->i_bytes;
    span1->i_desc_bytes += span2->i_desc_bytes;
}

/* Adds the frame that is described at i_desc_offset to span */

static int FrameSpanAddDescription( demux_t *p_demux, uint64_t i_desc_offset, frame_span_t *span )
{
    demux_sys_t *p_sys  = p_demux->p_sys;

    /* Avoid seeking + peeking for simple case (PCM) */
    if( p_sys->fmt.audio.i_bytes_per_frame && p_sys->fmt.audio.i_frame_length )
    {
        span->i_bytes += p_sys->fmt.audio.i_bytes_per_frame;
        span->i_samples += p_sys->fmt.audio.i_frame_length;
        span->i_frames++;
        return VLC_SUCCESS;
    }

    uint32_t i_desc_size = 0;

    if( vlc_stream_Seek( p_demux->s, p_sys->packet_table.i_descriptions_start + i_desc_offset ))
    {
        msg_Err( p_demux, "Couldn't seek packet description." );
        return VLC_EGENERIC;
    }

    const uint8_t *p_peek;
    int i_peek_len = vlc_stream_Peek( p_demux->s, &p_peek, 2 * 10 );
    /* Peeking the maximum number of bytes that two 64 bit numbers could use
     * (( 64 + 6 ) / 7 = 10). */
    if( i_peek_len < 0 )
        i_peek_len = 0;

    if( p_sys->fmt.audio.i_bytes_per_frame )
    {
        span->i_bytes += p_sys->fmt.audio.i_bytes_per_frame;
    }
    else
    {
        uint64_t i_size;
        uint32_t i_this_int;
        if( ParseVarLenInteger( p_peek, i_peek_len, &i_size, &i_this_int ))
        {
            return VLC_EGENERIC;
        }

        i_desc_size += i_this_int;
        span->i_bytes += i_size;
    }

    if( p_sys->fmt.audio.i_frame_length )
    {
        span->i_samples += p_sys->fmt.audio.i_frame_length;
    }
    else
    {
        if( i_desc_size >= (unsigned)i_peek_len )
        {
            return VLC_EGENERIC;
        }

        uint64_t i_num_samples;
        uint32_t i_this_int;
        if( ParseVarLenInteger( p_peek + i_desc_size, i_peek_len - i_desc_size, &i_num_samples, &i_this_int ))
        {
            return VLC_EGENERIC;
        }

        i_desc_size += i_this_int;
        span->i_samples += i_num_samples;
    }
    span->i_desc_bytes += i_desc_size;
    span->i_frames++;

    return VLC_SUCCESS;
}

/* FrameSpanGetTime returns the time span represented by the frame span. */

static inline vlc_tick_t FrameSpanGetTime( frame_span_t *span, uint32_t i_sample_rate )
{
    if( !i_sample_rate )
        return VLC_TICK_INVALID;

    return vlc_tick_from_samples( span->i_samples, i_sample_rate) + VLC_TICK_0;
}

/* SetSpanWithSample returns the span from the beginning of the file up to and
 including the specified sample. This works at frame granularity, so the
 returned span may not represent the exact target sample.
 Since we might have to lineraly search the packet descriptions, this is a
 potentially slow operation! */

static int SetSpanWithSample( demux_t *p_demux, frame_span_t *p_span, uint64_t i_target_sample )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    uint64_t i_num_frames = TotalNumFrames( p_demux );

    if( !NeedsPacketTable( p_sys ))
    {
        uint64_t i_frame = i_target_sample / p_sys->fmt.audio.i_frame_length;
        uint64_t i_remaining = i_target_sample - i_frame * p_sys->fmt.audio.i_frame_length;
        if( i_remaining > ( p_sys->fmt.audio.i_frame_length / 2 ))
            i_frame++;

        if( i_frame > i_num_frames )
            i_frame = i_num_frames;

        p_span->i_frames = i_frame;
        p_span->i_samples = i_frame * p_sys->fmt.audio.i_frame_length;
        p_span->i_bytes = i_frame * p_sys->fmt.audio.i_bytes_per_frame;
        p_span->i_desc_bytes = 0;
    }
    else
    {
        *p_span = (frame_span_t){0};
        frame_span_t prev_span;

        while( p_span->i_samples < i_target_sample && p_span->i_frames < i_num_frames )
        {
            prev_span = *p_span;

            if( FrameSpanAddDescription( p_demux, p_span->i_desc_bytes, p_span ))
                return VLC_EGENERIC;

            if( p_span->i_samples >= i_target_sample )
            {
                uint64_t i_this_samples = p_span->i_samples - prev_span.i_samples;

                if( i_target_sample - prev_span.i_samples < i_this_samples / 2 )
                    *p_span = prev_span;

                break;
            }
        }
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CAF Parsing Functions
 *****************************************************************************/

/* The NextChunk function returns the four char code and the (sanitized) size at the current file position.
   Upon return the file position points to the start of the chunk payload.
   Note that the ReadXXXChunk functions expect the chunk to size to be sanitized
   (i.e. they don't check if it is bigger than INT64_MAX, but note the special case of the 'data' chunk, which can be kCHUNK_SIZE_EOF).
 */

static int NextChunk( demux_t *p_demux, vlc_fourcc_t *p_fcc, uint64_t *pi_size )
{
    uint8_t p_read[12];

    if( vlc_stream_Read( p_demux->s, p_read, 12 ) < 12 )
        return VLC_EGENERIC;

    *p_fcc = ReadFOURCC( p_read );
    uint64_t i_size = GetQWBE( p_read + 4 );

    /* We accept no negativ sizes for chunks, except -1 for the data chunk. */

    if( i_size > INT64_MAX )
    {
        if( *p_fcc == VLC_FOURCC( 'd', 'a', 't', 'a' ) && i_size == UINT64_C( -1 ))
            i_size = kCHUNK_SIZE_EOF;
        else
            return VLC_EGENERIC;
    }

    *pi_size = i_size;

    return VLC_SUCCESS;
}

static int ReadDescChunk( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    const uint8_t *p_peek;

    if ( vlc_stream_Peek( p_demux->s, &p_peek, 8 + 6 * 4 ) < ( 8 + 6 * 4 ))
    {
        return VLC_EGENERIC;
    }

    vlc_fourcc_t i_fmt = ReadFOURCC( p_peek + 8 );
    uint32_t i_fmt_flags = GetDWBE( p_peek + 12 );

    uint32_t i_bits_per_channel = GetDWBE( p_peek + 28 );
    uint32_t i_bytes_per_packet = GetDWBE( p_peek + 16 );
    uint32_t i_frames_per_packet = GetDWBE( p_peek + 20 );
    uint32_t i_channels_per_frame = GetDWBE( p_peek + 24 );

    if( i_fmt == VLC_CODEC_DVD_LPCM )
    {
        if( !i_frames_per_packet || !i_channels_per_frame )
        {
            msg_Err( p_demux, "Absurd LPCM parameters (frames_per_packet: %u, channels_per_frame: %u).", i_frames_per_packet, i_channels_per_frame );
            return VLC_EGENERIC;
        }

        uint32_t i_unpacked_bits_per_sample = ( i_bytes_per_packet / i_frames_per_packet / i_channels_per_frame ) * 8;

        bool b_is_float = !!( i_fmt_flags & ( 1 << 0 ) );
        bool b_is_be = !( i_fmt_flags & ( 1 << 1 ) );

        vlc_fourcc_t i_basic_codec = 0;

        if( !b_is_float )
        {
            i_basic_codec = b_is_be ? VLC_FOURCC( 't', 'w', 'o', 's' ) : VLC_FOURCC( 's', 'o', 'w', 't' );
            es_format_Init( &p_sys->fmt, AUDIO_ES, vlc_fourcc_GetCodecAudio( i_basic_codec, i_unpacked_bits_per_sample ));
        }
        else
        {
            if( i_bits_per_channel == 32 )
                i_basic_codec = b_is_be ? VLC_CODEC_F32B : VLC_CODEC_F32L;
            else if( i_bits_per_channel == 64 )
                i_basic_codec = b_is_be ? VLC_CODEC_F64B : VLC_CODEC_F64L;

            if( i_basic_codec )
                es_format_Init( &p_sys->fmt, AUDIO_ES, vlc_fourcc_GetCodecAudio( i_basic_codec, i_bits_per_channel ));
        }
    }
    else if( i_fmt == VLC_FOURCC( 'a', 'a', 'c', ' ' ))
    {
        const uint32_t kMP4Audio_AAC_LC_ObjectType = 2;

        if( i_fmt_flags != kMP4Audio_AAC_LC_ObjectType )
        {
            msg_Warn( p_demux, "The only documented format flag for aac is 2 (kMP4Audio_AAC_LC_ObjectType), but is %i. Continuing anyways.", i_fmt_flags );
        }

        es_format_Init( &p_sys->fmt, AUDIO_ES, vlc_fourcc_GetCodecAudio( VLC_CODEC_MP4A, 0 ));

    }
    else
    {
        es_format_Init( &p_sys->fmt, AUDIO_ES, vlc_fourcc_GetCodecAudio( i_fmt, 0 ));
    }

    if( !p_sys->fmt.i_codec )
    {
        msg_Err( p_demux, "could not determine codec" );
        return VLC_EGENERIC;
    }

    double d_rate = round( GetDBLBE( p_peek ));

    if( d_rate <= 0 || d_rate > UINT_MAX )
        return VLC_EGENERIC;

    p_sys->fmt.audio.i_rate = (unsigned int)lround( d_rate );
    if( !p_sys->fmt.audio.i_rate )
    {
        msg_Err( p_demux, "Sample rate must be non-zero" );
        return VLC_EGENERIC;
    }
    p_sys->fmt.audio.i_channels = i_channels_per_frame;
    p_sys->fmt.audio.i_bytes_per_frame = i_bytes_per_packet; /* "mBytesPerPacket" in Apple parlance */
    p_sys->fmt.audio.i_frame_length = i_frames_per_packet; /* "mFramesPerPacket" in Apple parlance */
    p_sys->fmt.audio.i_bitspersample = i_bits_per_channel; /* mBitsPerChannel */
    p_sys->fmt.audio.i_blockalign = i_bytes_per_packet;
    p_sys->fmt.i_bitrate = i_bits_per_channel * p_sys->fmt.audio.i_rate * i_channels_per_frame;

    if( p_sys->fmt.i_codec == VLC_CODEC_OPUS )
    {
        p_sys->i_max_frames = 1;
    }
    else p_sys->i_max_frames = UINT_MAX;

    return VLC_SUCCESS;
}

/*  This is lifted from cafdec.c in libavformat (function read_kuki_chunk). Appearantly the
    alac library expects the cookie to be of length 36, but current alac files
    have a cookie length of 24.
 */
static int ProcessALACCookie( demux_t *p_demux, const uint8_t *p, uint64_t i_size )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    const unsigned int kALAC_NEW_KUKI_SIZE = 24;
    const unsigned int kALAC_LIB_REQ_KUKI_SIZE = 36;
    int i_extra;

    if( i_size == kALAC_NEW_KUKI_SIZE || i_size == kALAC_LIB_REQ_KUKI_SIZE )
    {
        i_extra = kALAC_LIB_REQ_KUKI_SIZE;
    }
    else
    {
        msg_Warn( p_demux, "Unknown alac magic cookie. Passing it on to the decoder as is and hoping for the best." );
        i_extra = ( int )i_size;
    }

    p_sys->fmt.i_extra = i_extra;
    p_sys->fmt.p_extra = malloc( i_extra );

    if( !p_sys->fmt.p_extra )
        return VLC_ENOMEM;

    uint8_t *p_extra = ( uint8_t * )p_sys->fmt.p_extra;

    if( i_size == kALAC_NEW_KUKI_SIZE )
    {
        SetDWBE( p_extra, 36 );
        memcpy( p_extra + 4, "alac", 4 );
        SetDWBE( p_extra + 8, 0 );
        memcpy( p_extra + 12, p, 24 );
    }
    else
    {
        memcpy( p_sys->fmt.p_extra, p, i_size );
    }

    return VLC_SUCCESS;
}

/* Helper functions for AAC cookie processing. */

static inline bool AACCookieGetTag( uint8_t *p_tag, const uint8_t *p, uint64_t *p_offset, uint64_t i_size )
{
    if( *p_offset + 1 > i_size )
        return false;

    *p_tag = *( p + *p_offset );
    *p_offset += 1;

    return true;
}

static inline bool AACCookieTagLen( uint64_t *p_tag_len, const uint8_t *p, uint64_t *p_offset, uint64_t i_size )
{
    uint32_t i_int_size;

    if( ParseVarLenInteger( p + *p_offset, i_size - *p_offset, p_tag_len, &i_int_size ))
        return false;

    *p_offset += i_int_size;

    return true;
}

static inline bool AACCookieChkLen( int64_t i_length, uint64_t i_size, uint64_t i_offset )
{
    return ( i_offset + i_length <= i_size );
}

/*  This is lifted from libmp4.c in the mp4 demuxer module (function MP4_ReadBox_esds). The aac
    library only want a subset of the magic cookie ("decoder specific info"), so we have to parse it.
 */
static int ProcessAACCookie( demux_t *p_demux, const uint8_t *p, uint64_t i_size )
{
    const uint8_t kAAC_ES_DESCR_TAG = 3;
    const uint8_t kAAC_DEC_CONFIG_DESCR_TAG = 4;
    const uint8_t kAAC_DEC_SPEC_INFO_TAG = 5;

    demux_sys_t *p_sys = p_demux->p_sys;

    uint64_t i_offset = 0;
    uint64_t i_kuki_size = 0;
    uint64_t i_tag_len;
    uint8_t i_tag;

    if( !AACCookieGetTag( &i_tag, p, &i_offset, i_size )) goto aac_kuki_finish;

    if( i_tag == kAAC_ES_DESCR_TAG )
    {

        if( !AACCookieTagLen( &i_tag_len, p, &i_offset, i_size )) goto aac_kuki_finish;

        if( !AACCookieChkLen( 3, i_size, i_offset )) goto aac_kuki_finish;
        i_offset += 2; /* don't care (ES ID) */
        uint8_t i_flags = *( p + i_offset++ );

        if( i_flags&0x80 )
        {
            if( !AACCookieChkLen( 2, i_size, i_offset )) goto aac_kuki_finish;
            i_offset += 2; /* don't care (dependance) */
        }
        if( i_flags&0x40 )
        {
            if( !AACCookieChkLen( 1, i_size, i_offset )) goto aac_kuki_finish;
            uint8_t i_url_len = *( p + i_offset++ );
            i_offset += i_url_len; /* don't care (url) */
        }
        if( i_flags&0x20 )
        {
            if( !AACCookieChkLen( 2, i_size, i_offset )) goto aac_kuki_finish;
            i_offset += 2; /* don't care (OCR) */
        }

        if( !AACCookieGetTag( &i_tag, p, &i_offset, i_size )) goto aac_kuki_finish;
    }

    if( i_tag != kAAC_DEC_CONFIG_DESCR_TAG )
        goto aac_kuki_finish;

    if( !AACCookieTagLen( &i_tag_len, p, &i_offset, i_size )) goto aac_kuki_finish;

    if( !AACCookieChkLen( 1 + 1 + 3 + 4 + 4, i_size, i_offset )) goto aac_kuki_finish;
    i_offset += ( 1 + 1 + 3 + 4 + 4 ); /* don't care */

    if( !AACCookieGetTag( &i_tag, p, &i_offset, i_size )) goto aac_kuki_finish;

    if( i_tag != kAAC_DEC_SPEC_INFO_TAG ) /* this is the one we need */
        goto aac_kuki_finish;

    if( !AACCookieTagLen( &i_tag_len, p, &i_offset, i_size )) goto aac_kuki_finish;

    if( i_offset + i_tag_len > i_size )
        goto aac_kuki_finish;

    i_kuki_size = i_tag_len;

aac_kuki_finish:

    if( !i_kuki_size )
    {
        msg_Warn( p_demux, "Error parsing aac cookie. Passing it on to the decoder as is and hoping for the best." );
        i_kuki_size = i_size;
        i_offset = 0;
    }

    p_sys->fmt.i_extra = (int)i_kuki_size;
    p_sys->fmt.p_extra = malloc( i_kuki_size );

    if( !p_sys->fmt.p_extra )
    {
        return VLC_ENOMEM;
    }

    memcpy( p_sys->fmt.p_extra, p + i_offset, i_kuki_size );

    return VLC_SUCCESS;
}

static int ReadKukiChunk( demux_t *p_demux, uint64_t i_size )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const uint8_t *p_peek;

    if( i_size > SSIZE_MAX )
    {
        msg_Err( p_demux, "Magic Cookie chunk too big" );
        return VLC_EGENERIC;
    }

    if( vlc_stream_Peek( p_demux->s, &p_peek, i_size ) < (ssize_t)i_size )
    {
        msg_Err( p_demux, "Couldn't peek extra data" );
        return VLC_EGENERIC;
    }

    if( p_sys->fmt.i_codec  == VLC_CODEC_ALAC )
    {
        int error = ProcessALACCookie( p_demux, p_peek, i_size );
        if( error ) return error;
    }
    else if( p_sys->fmt.i_codec == VLC_CODEC_MP4A )
    {
        int error = ProcessAACCookie( p_demux, p_peek, i_size );
        if( error ) return error;
    }
    else
    {
        p_sys->fmt.i_extra = (int)i_size;
        p_sys->fmt.p_extra = malloc( i_size );

        if( !p_sys->fmt.p_extra )
        {
            return VLC_ENOMEM;
        }
        memcpy( p_sys->fmt.p_extra, p_peek, p_sys->fmt.i_extra );
    }

    return VLC_SUCCESS;
}

static int ReadDataChunk( demux_t *p_demux, uint64_t i_size )
{
    if( i_size < 4 )
        return VLC_EGENERIC;

    demux_sys_t *p_sys = p_demux->p_sys;

    p_sys->i_data_offset = vlc_stream_Tell( p_demux->s ) + 4; /* skip edit count */
    p_sys->i_data_size = i_size == kCHUNK_SIZE_EOF ? kCHUNK_SIZE_EOF : ( i_size - 4 );

    return VLC_SUCCESS;
}

static int ReadPaktChunk( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    const uint8_t *p_peek;

    if ( vlc_stream_Peek( p_demux->s, &p_peek, 8 + 8 + 4 + 4 ) < ( 8 + 8 + 4 + 4 ))
    {
        msg_Err( p_demux, "Couldn't peek packet descriptions" );
        return VLC_EGENERIC;
    }

    if( ReadBEInt64ToUInt64( p_peek, &p_sys->packet_table.i_num_packets ))
    {
        msg_Err( p_demux, "Invalid packet table: i_num_packets is negative.");
        return VLC_EGENERIC;
    }
    if( ReadBEInt64ToUInt64( p_peek + 8, &p_sys->packet_table.i_num_valid_frames ))
    {
        msg_Err( p_demux, "Invalid packet table: i_num_valid_frames is negative.");
        return VLC_EGENERIC;
    }
    if( ReadBEInt32ToUInt32( p_peek + 16, &p_sys->packet_table.i_num_priming_frames ))
    {
        msg_Err( p_demux, "Invalid packet table: i_num_priming_frames is negative.");
        return VLC_EGENERIC;
    }
    if( ReadBEInt32ToUInt32( p_peek + 20, &p_sys->packet_table.i_num_remainder_frames ))
    {
        msg_Err( p_demux, "Invalid packet table: i_num_remainder_frames is negative.");
        return VLC_EGENERIC;
    }

    p_sys->packet_table.i_descriptions_start = vlc_stream_Tell( p_demux->s ) + 24;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    int i_error = VLC_SUCCESS;

    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    const uint8_t *p_peek;

    if( vlc_stream_Peek( p_demux->s, &p_peek, 8 ) < 8 )
        return VLC_EGENERIC;

    /* Is it a caf file? */
    if( memcmp( p_peek, "caff", 4 ))
        return VLC_EGENERIC;

    /* check file version (we only handle version 1) */
    uint16_t i_version = GetWBE( p_peek + 4 );
    if( i_version != 1 )
    {
        msg_Dbg( p_demux, "Unknown caf file version %d.", i_version );
        return VLC_EGENERIC;
    }

    /* check file flags (must be 0) */
    uint16_t i_flags = GetWBE( p_peek + 6 );
    if( i_flags != 0 )
    {
        msg_Dbg( p_demux, "Unknown caf file flags %d.", i_flags );
        return VLC_EGENERIC;
    }

    if( vlc_stream_Read( p_demux->s, NULL, 8 ) < 8 )
        return VLC_EGENERIC; /* This would be very strange since we justed peeked at these bytes. */

    p_demux->p_sys = calloc( 1, sizeof( demux_sys_t ));
    if( !p_demux->p_sys ) return VLC_ENOMEM;

    /* From this point on, we have to free p_sys if we return an error (e.g. "goto caf_open_end") */

    p_sys = p_demux->p_sys;
    es_format_Init( &p_sys->fmt, AUDIO_ES, 0 );

    vlc_fourcc_t i_fcc;
    uint64_t i_size;
    uint64_t i_idx = 0;

    while( NextChunk( p_demux, &i_fcc, &i_size ) == VLC_SUCCESS )
    {
        bool b_handled = true;

        switch ( i_fcc )
        {
            case VLC_FOURCC( 'd', 'e', 's', 'c' ):

                if( i_idx != 0 )
                {
                    msg_Err( p_demux, "The audio description chunk must be the first chunk in a caf file." );
                    i_error = VLC_EGENERIC;
                    goto caf_open_end;
                }

                i_error = ReadDescChunk( p_demux );
                break;

            case VLC_FOURCC( 'd', 'a', 't', 'a' ):

                i_error = ReadDataChunk( p_demux, i_size );
                break;

            case VLC_FOURCC( 'p', 'a', 'k', 't' ):

                i_error = ReadPaktChunk( p_demux );
                break;

            case VLC_FOURCC( 'k', 'u', 'k', 'i' ):

                i_error = ReadKukiChunk( p_demux, i_size );
                break;

            default:

                b_handled = false;
                break;
        }

        if( i_error )
            goto caf_open_end;

        if( b_handled )
            msg_Dbg( p_demux, "Found '%4.4s' chunk.", ( char * )&i_fcc );
        else
            msg_Dbg( p_demux, "Ignoring '%4.4s' chunk.", ( char * )&i_fcc );

        if( i_size == kCHUNK_SIZE_EOF )
            break;

        if( vlc_stream_Seek( p_demux->s, vlc_stream_Tell( p_demux->s ) + i_size ) != VLC_SUCCESS )
            break;

        i_idx++;
    }

    if ( !p_sys->i_data_offset || p_sys->fmt.i_cat != AUDIO_ES || !p_sys->fmt.audio.i_rate ||
        ( NeedsPacketTable( p_sys ) && !p_sys->packet_table.i_descriptions_start ))
    {
        msg_Err( p_demux, "Did not find all necessary chunks." );
        i_error = VLC_EGENERIC;
        goto caf_open_end;
    }

    p_sys->fmt.i_id = 0;
    p_sys->es = es_out_Add( p_demux->out, &p_sys->fmt );

    if( !p_sys->es )
    {
        msg_Err( p_demux, "Could not add elementary stream." );
        i_error = VLC_EGENERIC;
        goto caf_open_end;
    }

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;
    return VLC_SUCCESS;

caf_open_end:
    es_format_Clean( &p_sys->fmt );
    free( p_sys  );
    return i_error;
}

/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_block;

    if( p_sys->i_data_size != kCHUNK_SIZE_EOF && p_sys->position.i_bytes >= p_sys->i_data_size )
    {
        /* EOF */
        return VLC_DEMUXER_EOF;
    }

    frame_span_t advance = (frame_span_t){0};

    /* we will read 50ms at once */
    uint64_t i_req_samples = __MAX( p_sys->fmt.audio.i_rate / 20, 1 );

    if( !NeedsPacketTable( p_sys )) /* PCM/IMA4 */
    {
        int64_t i_req_frames = ( i_req_samples + ( p_sys->fmt.audio.i_frame_length - 1 )) / p_sys->fmt.audio.i_frame_length;

        if( p_sys->i_data_size != kCHUNK_SIZE_EOF && ( p_sys->position.i_bytes + i_req_frames * p_sys->fmt.audio.i_bytes_per_frame ) > p_sys->i_data_size )
        {
            i_req_frames = ( p_sys->i_data_size - p_sys->position.i_frames * p_sys->fmt.audio.i_bytes_per_frame ) / p_sys->fmt.audio.i_bytes_per_frame;
        }

        advance.i_frames = i_req_frames;
        advance.i_samples = i_req_frames * p_sys->fmt.audio.i_frame_length;
        advance.i_bytes = p_sys->fmt.audio.i_bytes_per_frame * advance.i_frames;
    }
    else /* use packet table */
    {
        uint64_t i_max_frames;
        if( p_sys->packet_table.i_num_packets > p_sys->position.i_frames )
            i_max_frames = p_sys->packet_table.i_num_packets - p_sys->position.i_frames;
        else
            i_max_frames = 1; /* will be rejected on FrameSpanAddDescription below */

        if( i_max_frames > p_sys->i_max_frames )
            i_max_frames = p_sys->i_max_frames;

        do
        {
            if( FrameSpanAddDescription( p_demux, p_sys->position.i_desc_bytes + advance.i_desc_bytes, &advance ))
                break;
        }
        while ( i_req_samples > advance.i_samples && advance.i_frames < i_max_frames );
    }

    if( !advance.i_frames )
    {
        msg_Err( p_demux, "Unexpected end of file" );
        return VLC_DEMUXER_EGENERIC;
    }

    if( vlc_stream_Seek( p_demux->s, p_sys->i_data_offset + p_sys->position.i_bytes ))
    {
        if( p_sys->i_data_size == kCHUNK_SIZE_EOF)
            return VLC_DEMUXER_EOF;

        msg_Err( p_demux, "cannot seek data" );
        return VLC_DEMUXER_EGENERIC;
    }

    p_block = vlc_stream_Block( p_demux->s, (int)advance.i_bytes );
    if( p_block == NULL )
    {
        msg_Err( p_demux, "cannot read data" );
        return VLC_DEMUXER_EGENERIC;
    }

    p_block->i_dts =
    p_block->i_pts = FrameSpanGetTime( &p_sys->position, p_sys->fmt.audio.i_rate );

    FrameSpanAddSpan( &p_sys->position, &advance );

    es_out_SetPCR( p_demux->out, p_block->i_pts );
    es_out_Send( p_demux->out, p_sys->es, p_block );

    return VLC_DEMUXER_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    int64_t i_sample;
    double f, *pf;
    frame_span_t position;

    demux_sys_t *p_sys  = p_demux->p_sys;
    uint64_t i_num_samples = TotalNumSamples( p_demux );

    switch( i_query )
    {
        case DEMUX_CAN_SEEK:
            *va_arg( args, bool * ) = true;
            return VLC_SUCCESS;

        case DEMUX_GET_LENGTH:
            *va_arg( args, vlc_tick_t * ) =
                vlc_tick_from_samples( i_num_samples, p_sys->fmt.audio.i_rate );
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            *va_arg( args, vlc_tick_t * ) =
                vlc_tick_from_samples( p_sys->position.i_samples, p_sys->fmt.audio.i_rate );
            return VLC_SUCCESS;

        case DEMUX_GET_POSITION:
            pf = va_arg( args, double * );
            *pf = i_num_samples ? (double)p_sys->position.i_samples / (double)i_num_samples : 0.0;
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            f = va_arg( args, double );
            i_sample = f * i_num_samples;
            if( SetSpanWithSample( p_demux, &position, i_sample ))
                return VLC_EGENERIC;
            p_sys->position = position;
            return VLC_SUCCESS;

        case DEMUX_SET_TIME:
            i_sample =
                samples_from_vlc_tick( va_arg( args, vlc_tick_t ), p_sys->fmt.audio.i_rate );
            if( SetSpanWithSample( p_demux, &position, i_sample ))
                return VLC_EGENERIC;
            p_sys->position = position;
            return VLC_SUCCESS;

        case DEMUX_GET_META:
            return vlc_stream_Control( p_demux->s, STREAM_GET_META, args );

        case DEMUX_CAN_PAUSE:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_GET_PTS_DELAY:
            return demux_vaControlHelper( p_demux->s, p_sys->i_data_offset,
                                          p_sys->i_data_size, 0, 1, i_query, args );

        default:
            return VLC_EGENERIC;
    }

    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    es_format_Clean( &p_sys->fmt );
    free( p_sys );
}
