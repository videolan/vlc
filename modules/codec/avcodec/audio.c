/*****************************************************************************
 * audio.c: audio decoder using libavcodec library
 *****************************************************************************
 * Copyright (C) 1999-2003 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_aout.h>
#include <vlc_codec.h>
#include <vlc_avcodec.h>

#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>

#include <libavutil/audioconvert.h>

#include "avcodec.h"

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    AVCODEC_COMMON_MEMBERS

    /* Temporary buffer for libavcodec */
    int     i_output_max;
    uint8_t *p_output;

    /*
     * Output properties
     */
    audio_sample_format_t aout_format;
    date_t                end_date;

    /* */
    int     i_reject_count;

    /* */
    bool    b_extract;
    int     pi_extraction[AOUT_CHAN_MAX];
    int     i_previous_channels;
    int64_t i_previous_layout;
};

#define BLOCK_FLAG_PRIVATE_REALLOCATED (1 << BLOCK_FLAG_PRIVATE_SHIFT)

static void SetupOutputFormat( decoder_t *p_dec, bool b_trust );

static void InitDecoderConfig( decoder_t *p_dec, AVCodecContext *p_context )
{
    if( p_dec->fmt_in.i_extra > 0 )
    {
        const uint8_t * const p_src = p_dec->fmt_in.p_extra;
        int i_offset;
        int i_size;

        if( p_dec->fmt_in.i_codec == VLC_CODEC_FLAC )
        {
            i_offset = 8;
            i_size = p_dec->fmt_in.i_extra - 8;
        }
        else if( p_dec->fmt_in.i_codec == VLC_CODEC_ALAC )
        {
            static const uint8_t p_pattern[] = { 0, 0, 0, 36, 'a', 'l', 'a', 'c' };
            /* Find alac atom XXX it is a bit ugly */
            for( i_offset = 0; i_offset < p_dec->fmt_in.i_extra - sizeof(p_pattern); i_offset++ )
            {
                if( !memcmp( &p_src[i_offset], p_pattern, sizeof(p_pattern) ) )
                    break;
            }
            i_size = __MIN( p_dec->fmt_in.i_extra - i_offset, 36 );
            if( i_size < 36 )
                i_size = 0;
        }
        else
        {
            i_offset = 0;
            i_size = p_dec->fmt_in.i_extra;
        }

        if( i_size > 0 )
        {
            p_context->extradata =
                malloc( i_size + FF_INPUT_BUFFER_PADDING_SIZE );
            if( p_context->extradata )
            {
                uint8_t *p_dst = p_context->extradata;

                p_context->extradata_size = i_size;

                memcpy( &p_dst[0],            &p_src[i_offset], i_size );
                memset( &p_dst[i_size], 0, FF_INPUT_BUFFER_PADDING_SIZE );
            }
        }
    }
    else
    {
        p_context->extradata_size = 0;
        p_context->extradata = NULL;
    }
}

/*****************************************************************************
 * InitAudioDec: initialize audio decoder
 *****************************************************************************
 * The avcodec codec will be opened, some memory allocated.
 *****************************************************************************/
int InitAudioDec( decoder_t *p_dec, AVCodecContext *p_context,
                      AVCodec *p_codec, int i_codec_id, const char *psz_namecodec )
{
    decoder_sys_t *p_sys;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc(sizeof(*p_sys)) ) == NULL )
    {
        return VLC_ENOMEM;
    }

    p_codec->type = AVMEDIA_TYPE_AUDIO;
    p_context->codec_type = AVMEDIA_TYPE_AUDIO;
    p_context->codec_id = i_codec_id;
    p_sys->p_context = p_context;
    p_sys->p_codec = p_codec;
    p_sys->i_codec_id = i_codec_id;
    p_sys->psz_namecodec = psz_namecodec;
    p_sys->b_delayed_open = true;

    // Initialize decoder extradata
    InitDecoderConfig( p_dec, p_context);

    /* ***** Open the codec ***** */
    if( ffmpeg_OpenCodec( p_dec ) < 0 )
    {
        msg_Err( p_dec, "cannot open codec (%s)", p_sys->psz_namecodec );
        free( p_sys->p_context->extradata );
        free( p_sys );
        return VLC_EGENERIC;
    }

    switch( i_codec_id )
    {
    case CODEC_ID_WAVPACK:
        p_sys->i_output_max = 8 * sizeof(int32_t) * 131072;
        break;
    case CODEC_ID_TTA:
        p_sys->i_output_max = p_sys->p_context->channels * sizeof(int32_t) * p_sys->p_context->sample_rate * 2;
        break;
    case CODEC_ID_FLAC:
        p_sys->i_output_max = 8 * sizeof(int32_t) * 65535;
        break;
    case CODEC_ID_WMAPRO:
        p_sys->i_output_max = 8 * sizeof(float) * 6144; /* (1 << 12) * 3/2 */
        break;
    default:
        p_sys->i_output_max = 0;
        break;
    }
    if( p_sys->i_output_max < AVCODEC_MAX_AUDIO_FRAME_SIZE )
        p_sys->i_output_max = AVCODEC_MAX_AUDIO_FRAME_SIZE;
    msg_Dbg( p_dec, "Using %d bytes output buffer", p_sys->i_output_max );
    p_sys->p_output = av_malloc( p_sys->i_output_max );

    p_sys->i_reject_count = 0;
    p_sys->b_extract = false;
    p_sys->i_previous_channels = 0;
    p_sys->i_previous_layout = 0;

    /* */
    p_dec->fmt_out.i_cat = AUDIO_ES;
    /* Try to set as much information as possible but do not trust it */
    SetupOutputFormat( p_dec, false );

    date_Set( &p_sys->end_date, 0 );
    if( p_dec->fmt_out.audio.i_rate )
        date_Init( &p_sys->end_date, p_dec->fmt_out.audio.i_rate, 1 );
    else if( p_dec->fmt_in.audio.i_rate )
        date_Init( &p_sys->end_date, p_dec->fmt_in.audio.i_rate, 1 );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DecodeAudio: Called to decode one frame
 *****************************************************************************/
block_t * DecodeAudio ( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_used, i_output;
    AVPacket pkt;

    if( !pp_block || !*pp_block )
        return NULL;

    block_t *p_block = *pp_block;
    *pp_block = NULL;

    if( !p_sys->p_context->extradata_size && p_dec->fmt_in.i_extra &&
        p_sys->b_delayed_open)
    {
        InitDecoderConfig( p_dec, p_sys->p_context);
        if( ffmpeg_OpenCodec( p_dec ) )
            msg_Err( p_dec, "Cannot open decoder %s", p_sys->psz_namecodec );
    }

    if( p_sys->b_delayed_open )
        goto end;

    if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        avcodec_flush_buffers( p_sys->p_context );
        date_Set( &p_sys->end_date, 0 );

        if( p_sys->i_codec_id == CODEC_ID_MP2 || p_sys->i_codec_id == CODEC_ID_MP3 )
            p_sys->i_reject_count = 3;

        goto end;
    }

    /* We've just started the stream, wait for the first PTS. */
    if( !date_Get( &p_sys->end_date ) && p_block->i_pts <= VLC_TS_INVALID )
        goto end;

    if( p_block->i_buffer <= 0 )
        goto end;

    if( (p_block->i_flags & BLOCK_FLAG_PRIVATE_REALLOCATED) == 0 )
    {
        p_block = block_Realloc( p_block, 0, p_block->i_buffer + FF_INPUT_BUFFER_PADDING_SIZE );
        if( !p_block )
            return NULL;
        p_block->i_buffer -= FF_INPUT_BUFFER_PADDING_SIZE;
        memset( &p_block->p_buffer[p_block->i_buffer], 0, FF_INPUT_BUFFER_PADDING_SIZE );

        p_block->i_flags |= BLOCK_FLAG_PRIVATE_REALLOCATED;
    }

    do
    {
        i_output = __MAX( p_block->i_buffer, p_sys->i_output_max );
        if( i_output > p_sys->i_output_max )
        {
            /* Grow output buffer if necessary (eg. for PCM data) */
            p_sys->p_output = av_realloc( p_sys->p_output, i_output );
        }

        av_init_packet( &pkt );
        pkt.data = p_block->p_buffer;
        pkt.size = p_block->i_buffer;
        i_used = avcodec_decode_audio3( p_sys->p_context,
                                       (int16_t*)p_sys->p_output, &i_output,
                                       &pkt );

        if( i_used < 0 || i_output < 0 )
        {
            if( i_used < 0 )
                msg_Warn( p_dec, "cannot decode one frame (%zu bytes)",
                          p_block->i_buffer );

            goto end;
        }
        else if( (size_t)i_used > p_block->i_buffer )
        {
            i_used = p_block->i_buffer;
        }

        p_block->i_buffer -= i_used;
        p_block->p_buffer += i_used;

    } while( p_block->i_buffer > 0 && i_output <= 0 );

    if( p_sys->p_context->channels <= 0 || p_sys->p_context->channels > 8 ||
        p_sys->p_context->sample_rate <= 0 )
    {
        msg_Warn( p_dec, "invalid audio properties channels count %d, sample rate %d",
                  p_sys->p_context->channels, p_sys->p_context->sample_rate );
        goto end;
    }

    if( p_dec->fmt_out.audio.i_rate != (unsigned int)p_sys->p_context->sample_rate )
        date_Init( &p_sys->end_date, p_sys->p_context->sample_rate, 1 );

    if( p_block->i_pts > VLC_TS_INVALID &&
        p_block->i_pts != date_Get( &p_sys->end_date ) )
    {
        date_Set( &p_sys->end_date, p_block->i_pts );
    }

    block_Release( p_block );

    SetupOutputFormat( p_dec, true );

    /* Silent unwanted samples */
    if( p_sys->i_reject_count > 0 )
    {
        memset( p_sys->p_output, 0, i_output );
        p_sys->i_reject_count--;
    }

    int i_samples = i_output / (p_dec->fmt_out.audio.i_bitspersample / 8) / p_sys->p_context->channels;
    if (i_samples == 0)
        return NULL;

    block_t *p_buffer = decoder_NewAudioBuffer( p_dec, i_samples );
    if (!p_buffer)
        return NULL;

    p_buffer->i_pts = date_Get( &p_sys->end_date );
    p_buffer->i_length = date_Increment( &p_sys->end_date, i_samples ) - p_buffer->i_pts;

    int sample_planar = av_sample_fmt_is_planar( p_sys->p_context->sample_fmt );
    if( sample_planar )
        aout_Interleave( p_buffer->p_buffer, p_sys->p_output, i_samples, p_sys->p_context->channels, p_dec->fmt_out.audio.i_format );

    if( p_sys->b_extract == !!sample_planar )
        memcpy( p_sys->p_output, p_buffer->p_buffer, p_buffer->i_buffer );

    if (p_sys->b_extract)
        aout_ChannelExtract( p_buffer->p_buffer, p_dec->fmt_out.audio.i_channels,
                             p_sys->p_output, p_sys->p_context->channels, i_samples,
                             p_sys->pi_extraction, p_dec->fmt_out.audio.i_bitspersample );

    return p_buffer;

end:
    block_Release(p_block);
    return NULL;
}

/*****************************************************************************
 * EndAudioDec: audio decoder destruction
 *****************************************************************************/
void EndAudioDec( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    av_free( p_sys->p_output );
}

/*****************************************************************************
 *
 *****************************************************************************/

vlc_fourcc_t GetVlcAudioFormat( int fmt )
{
    static const vlc_fourcc_t fcc[] = {
        [AV_SAMPLE_FMT_U8]    = VLC_CODEC_U8,
        [AV_SAMPLE_FMT_S16]   = VLC_CODEC_S16N,
        [AV_SAMPLE_FMT_S32]   = VLC_CODEC_S32N,
        [AV_SAMPLE_FMT_FLT]   = VLC_CODEC_FL32,
        [AV_SAMPLE_FMT_DBL]   = VLC_CODEC_FL64,
        [AV_SAMPLE_FMT_U8P]   = VLC_CODEC_U8,
        [AV_SAMPLE_FMT_S16P]  = VLC_CODEC_S16N,
        [AV_SAMPLE_FMT_S32P]  = VLC_CODEC_S32N,
        [AV_SAMPLE_FMT_FLTP]  = VLC_CODEC_FL32,
        [AV_SAMPLE_FMT_DBLP]  = VLC_CODEC_FL64,
    };
    if( (sizeof(fcc) / sizeof(fcc[0])) > (unsigned)fmt )
        return fcc[fmt];
    return VLC_CODEC_S16N;
}

static const uint64_t pi_channels_map[][2] =
{
    { AV_CH_FRONT_LEFT,        AOUT_CHAN_LEFT },
    { AV_CH_FRONT_RIGHT,       AOUT_CHAN_RIGHT },
    { AV_CH_FRONT_CENTER,      AOUT_CHAN_CENTER },
    { AV_CH_LOW_FREQUENCY,     AOUT_CHAN_LFE },
    { AV_CH_BACK_LEFT,         AOUT_CHAN_REARLEFT },
    { AV_CH_BACK_RIGHT,        AOUT_CHAN_REARRIGHT },
    { AV_CH_FRONT_LEFT_OF_CENTER, 0 },
    { AV_CH_FRONT_RIGHT_OF_CENTER, 0 },
    { AV_CH_BACK_CENTER,       AOUT_CHAN_REARCENTER },
    { AV_CH_SIDE_LEFT,         AOUT_CHAN_MIDDLELEFT },
    { AV_CH_SIDE_RIGHT,        AOUT_CHAN_MIDDLERIGHT },
    { AV_CH_TOP_CENTER,        0 },
    { AV_CH_TOP_FRONT_LEFT,    0 },
    { AV_CH_TOP_FRONT_CENTER,  0 },
    { AV_CH_TOP_FRONT_RIGHT,   0 },
    { AV_CH_TOP_BACK_LEFT,     0 },
    { AV_CH_TOP_BACK_CENTER,   0 },
    { AV_CH_TOP_BACK_RIGHT,    0 },
    { AV_CH_STEREO_LEFT,       0 },
    { AV_CH_STEREO_RIGHT,      0 },
};

static void SetupOutputFormat( decoder_t *p_dec, bool b_trust )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    p_dec->fmt_out.i_codec = GetVlcAudioFormat( p_sys->p_context->sample_fmt );
    p_dec->fmt_out.audio.i_format = p_dec->fmt_out.i_codec;
    p_dec->fmt_out.audio.i_rate = p_sys->p_context->sample_rate;

    /* */
    if( p_sys->i_previous_channels == p_sys->p_context->channels &&
        p_sys->i_previous_layout == p_sys->p_context->channel_layout )
        return;
    if( b_trust )
    {
        p_sys->i_previous_channels = p_sys->p_context->channels;
        p_sys->i_previous_layout = p_sys->p_context->channel_layout;
    }

    /* Specified order
     * FIXME should we use fmt_in.audio.i_physical_channels or not ?
     */
    const unsigned i_order_max = 8 * sizeof(p_sys->p_context->channel_layout);
    uint32_t pi_order_src[i_order_max];
    int i_channels_src = 0;

    if( p_sys->p_context->channel_layout )
    {
        for( unsigned i = 0; i < sizeof(pi_channels_map)/sizeof(*pi_channels_map); i++ )
        {
            if( p_sys->p_context->channel_layout & pi_channels_map[i][0] )
                pi_order_src[i_channels_src++] = pi_channels_map[i][1];
        }
    }
    else
    {
        /* Create default order  */
        if( b_trust )
            msg_Warn( p_dec, "Physical channel configuration not set : guessing" );
        for( unsigned int i = 0; i < __MIN( i_order_max, (unsigned)p_sys->p_context->channels ); i++ )
        {
            if( i < sizeof(pi_channels_map)/sizeof(*pi_channels_map) )
                pi_order_src[i_channels_src++] = pi_channels_map[i][1];
        }
    }
    if( i_channels_src != p_sys->p_context->channels && b_trust )
        msg_Err( p_dec, "Channel layout not understood" );

    uint32_t i_layout_dst;
    int      i_channels_dst;
    p_sys->b_extract = aout_CheckChannelExtraction( p_sys->pi_extraction,
                                                    &i_layout_dst, &i_channels_dst,
                                                    NULL, pi_order_src, i_channels_src );
    if( i_channels_dst != i_channels_src && b_trust )
        msg_Warn( p_dec, "%d channels are dropped", i_channels_src - i_channels_dst );

    p_dec->fmt_out.audio.i_physical_channels =
    p_dec->fmt_out.audio.i_original_channels = i_layout_dst;
    aout_FormatPrepare( &p_dec->fmt_out.audio );
}

