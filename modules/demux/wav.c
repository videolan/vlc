/*****************************************************************************
 * wav.c : wav file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2008 VLC authors and VideoLAN
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

#include <assert.h>
#include <limits.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_aout.h>
#include <vlc_codecs.h>

#include "windows_audio_commons.h"

#define WAV_CHAN_MAX 32
static_assert( INPUT_CHAN_MAX >= WAV_CHAN_MAX, "channel count mismatch" );

typedef struct
{
    es_format_t     fmt;
    es_out_id_t     *p_es;

    uint64_t        i_data_pos;
    uint64_t        i_data_size;

    unsigned int    i_frame_size;
    int             i_frame_samples;

    date_t          pts;

    uint32_t i_channel_mask;
    uint8_t i_chans_to_reorder;            /* do we need channel reordering */
    uint8_t pi_chan_table[AOUT_CHAN_MAX];
} demux_sys_t;

enum wav_chunk_id {
    wav_chunk_id_data,
    wav_chunk_id_ds64,
    wav_chunk_id_fmt,
};

static const struct wav_chunk_id_key
{
    enum wav_chunk_id id;
    char key[5];
} wav_chunk_id_key_list[] =  {
    /* Alphabetical order */
    { wav_chunk_id_data, "data" },
    { wav_chunk_id_ds64, "ds64" },
    { wav_chunk_id_fmt,  "fmt " },
};
static const size_t wav_chunk_id_key_count = ARRAY_SIZE(wav_chunk_id_key_list);

static int
wav_chunk_CompareCb(const void *a, const void *b)
{
    const struct wav_chunk_id_key *id = b;
    return memcmp(a, id->key, 4);
}

static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_block;
    const int64_t i_pos = vlc_stream_Tell( p_demux->s );
    unsigned int i_read_size = p_sys->i_frame_size;
    uint32_t i_read_samples = p_sys->i_frame_samples;


    if( p_sys->i_data_size > 0 )
    {
        int64_t i_end = p_sys->i_data_pos + p_sys->i_data_size;
        if ( i_pos >= i_end )
            return VLC_DEMUXER_EOF;  /* EOF */

        /* Don't read past data chunk boundary */
        if ( i_end < i_pos + i_read_size )
        {
            i_read_size = i_end - i_pos;
            i_read_samples = ( p_sys->i_frame_size - i_read_size )
                           * p_sys->i_frame_samples / p_sys->i_frame_size;
        }
    }

    if( ( p_block = vlc_stream_Block( p_demux->s, i_read_size ) ) == NULL )
    {
        msg_Warn( p_demux, "cannot read data" );
        return VLC_DEMUXER_EOF;
    }

    p_block->i_dts =
    p_block->i_pts = date_Get( &p_sys->pts );

    /* set PCR */
    es_out_SetPCR( p_demux->out, p_block->i_pts );

    /* Do the channel reordering */
    if( p_sys->i_chans_to_reorder )
        aout_ChannelReorder( p_block->p_buffer, p_block->i_buffer,
                             p_sys->fmt.audio.i_channels,
                             p_sys->pi_chan_table, p_sys->fmt.i_codec );

    es_out_Send( p_demux->out, p_sys->p_es, p_block );

    date_Increment( &p_sys->pts, i_read_samples );

    return VLC_DEMUXER_SUCCESS;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;
    int64_t i_end = -1;

    if( p_sys->i_data_size > 0 )
        i_end = p_sys->i_data_pos + p_sys->i_data_size;

    int ret = demux_vaControlHelper( p_demux->s, p_sys->i_data_pos, i_end,
                                     p_sys->fmt.i_bitrate,
                                     p_sys->fmt.audio.i_blockalign,
                                     i_query, args );
    if( ret != VLC_SUCCESS )
        return ret;

    /* Update the date to the new seek point */
    switch( i_query )
    {
        case DEMUX_SET_POSITION:
        case DEMUX_SET_TIME:
        {
            uint64_t ofs = vlc_stream_Tell( p_demux->s );
            if( unlikely( ofs < p_sys->i_data_pos ) )
                return VLC_SUCCESS;

            ofs -= p_sys->i_data_pos;
            vlc_tick_t pts =
                vlc_tick_from_samples( ofs * 8, p_sys->fmt.i_bitrate );
            date_Set( &p_sys->pts, pts );
            break;
        }
        default:
            break;
    }
    return VLC_SUCCESS;
}

static int ChunkSkip( demux_t *p_demux, uint32_t i_size )
{
    i_size += i_size & 1;

    if( unlikely( i_size >= 65536 ) )
    {
        /* Arbitrary size where a seek should be performed instead of skipping
         * by reading NULL. Non data chunks are generally smaller than this.
         * This seek may be used to skip the data chunk if there is an other
         * chunk after it (unlikely). */
        return vlc_stream_Seek( p_demux->s,
                                vlc_stream_Tell( p_demux->s ) + i_size );
    }

    ssize_t i_ret = vlc_stream_Read( p_demux->s, NULL, i_size );
    return i_ret < 0 || (size_t) i_ret != i_size ? VLC_EGENERIC : VLC_SUCCESS;
}

static int ChunkGetNext( demux_t *p_demux, enum wav_chunk_id *p_id,
                         uint32_t *pi_size )
{
#ifndef NDEBUG
    /* assert that keys are in alphabetical order */
    for( size_t i = 0; i < wav_chunk_id_key_count - 1; ++i )
        assert( strcmp( wav_chunk_id_key_list[i].key,
                        wav_chunk_id_key_list[i + 1].key ) < 0 );
#endif

    for( ;; )
    {
        const uint8_t *p_peek;
        if( vlc_stream_Peek( p_demux->s, &p_peek, 8 ) < 8 )
            return VLC_EGENERIC;

        const struct wav_chunk_id_key *id =
            bsearch( p_peek, wav_chunk_id_key_list, wav_chunk_id_key_count,
                     sizeof(*wav_chunk_id_key_list), wav_chunk_CompareCb );
        uint32_t i_size = GetDWLE( p_peek + 4 );

        if( id == NULL )
        {
            msg_Warn( p_demux, "unknown chunk '%4.4s' of size: %u",
                      p_peek, i_size );

            if( vlc_stream_Read( p_demux->s, NULL, 8 ) != 8 )
                return VLC_EGENERIC;

            if( ChunkSkip( p_demux, i_size ) != VLC_SUCCESS )
                return VLC_EGENERIC;
            continue;
        }

        if( vlc_stream_Read( p_demux->s, NULL, 8 ) != 8 )
            return VLC_EGENERIC;

        *p_id = id->id;
        *pi_size = i_size;

        return VLC_SUCCESS;
    }
}

static int FrameInfo_PCM( unsigned int *pi_size, int *pi_samples,
                          const es_format_t *p_fmt )
{
    int i_bytes;

    if( p_fmt->audio.i_rate > 352800
     || p_fmt->audio.i_bitspersample > 64
     || p_fmt->audio.i_channels > WAV_CHAN_MAX )
        return VLC_EGENERIC;

    /* read samples for 50ms of */
    *pi_samples = __MAX( p_fmt->audio.i_rate / 20, 1 );

    i_bytes = *pi_samples * p_fmt->audio.i_channels *
        ( (p_fmt->audio.i_bitspersample + 7) / 8 );

    if( p_fmt->audio.i_blockalign > 0 )
    {
        const int i_modulo = i_bytes % p_fmt->audio.i_blockalign;
        if( i_modulo > 0 )
            i_bytes += p_fmt->audio.i_blockalign - i_modulo;
    }

    *pi_size = i_bytes;
    return VLC_SUCCESS;
}

static int FrameInfo_MS_ADPCM( unsigned int *pi_size, int *pi_samples,
                               const es_format_t *p_fmt )
{
    if( p_fmt->audio.i_channels == 0 )
        return VLC_EGENERIC;

    *pi_samples = 2 + 2 * ( p_fmt->audio.i_blockalign -
        7 * p_fmt->audio.i_channels ) / p_fmt->audio.i_channels;
    *pi_size = p_fmt->audio.i_blockalign;

    return VLC_SUCCESS;
}

static int FrameInfo_IMA_ADPCM( unsigned int *pi_size, int *pi_samples,
                                const es_format_t *p_fmt )
{
    if( p_fmt->audio.i_channels == 0 )
        return VLC_EGENERIC;

    *pi_samples = 2 * ( p_fmt->audio.i_blockalign -
        4 * p_fmt->audio.i_channels ) / p_fmt->audio.i_channels;
    *pi_size = p_fmt->audio.i_blockalign;

    return VLC_SUCCESS;
}

static int FrameInfo_Creative_ADPCM( unsigned int *pi_size, int *pi_samples,
                                     const es_format_t *p_fmt )
{
    if( p_fmt->audio.i_channels == 0 )
        return VLC_EGENERIC;

    /* 4 bits / sample */
    *pi_samples = p_fmt->audio.i_blockalign * 2 / p_fmt->audio.i_channels;
    *pi_size = p_fmt->audio.i_blockalign;

    return VLC_SUCCESS;
}

static int FrameInfo_MSGSM( unsigned int *pi_size, int *pi_samples,
                            const es_format_t *p_fmt )
{
    if( p_fmt->i_bitrate <= 0 )
        return VLC_EGENERIC;

    *pi_samples = ( p_fmt->audio.i_blockalign * p_fmt->audio.i_rate * 8)
                    / p_fmt->i_bitrate;
    *pi_size = p_fmt->audio.i_blockalign;

    return VLC_SUCCESS;
}
static void Close ( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys   = p_demux->p_sys;

    es_format_Clean( &p_sys->fmt );
    free( p_sys );
}

static int ChunkParseDS64( demux_t *p_demux, uint32_t i_size )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const uint8_t *p_peek;

    if( i_size < 24 )
    {
        msg_Err( p_demux, "invalid 'ds64' chunk" );
        return VLC_EGENERIC;
    }

    if( vlc_stream_Peek( p_demux->s, &p_peek, 24 ) < 24 )
        return VLC_EGENERIC;

    p_sys->i_data_size = GetQWLE( &p_peek[8] );

    return ChunkSkip( p_demux, i_size );
}

static int ChunkParseFmt( demux_t *p_demux, uint32_t i_size )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    WAVEFORMATEXTENSIBLE *p_wf_ext = NULL;
    WAVEFORMATEX         *p_wf = NULL;
    const char *psz_name;
    unsigned int i_extended;

    i_size += 2;
    if( i_size < sizeof( WAVEFORMATEX ) )
    {
        msg_Err( p_demux, "invalid 'fmt ' chunk" );
        goto error;
    }

    /* load waveformatex */
    p_wf_ext = malloc( i_size );
    if( unlikely( !p_wf_ext ) )
         goto error;

    p_wf         = &p_wf_ext->Format;
    p_wf->cbSize = 0;
    i_size      -= 2;
    if( vlc_stream_Read( p_demux->s, p_wf, i_size ) != (int)i_size ||
        ( ( i_size & 1 ) && vlc_stream_Read( p_demux->s, NULL, 1 ) != 1 ) )
    {
        msg_Err( p_demux, "cannot load 'fmt ' chunk" );
        goto error;
    }

    wf_tag_to_fourcc( GetWLE( &p_wf->wFormatTag ), &p_sys->fmt.i_codec,
                      &psz_name );
    p_sys->fmt.audio.i_channels      = GetWLE ( &p_wf->nChannels );
    p_sys->fmt.audio.i_rate          = GetDWLE( &p_wf->nSamplesPerSec );
    p_sys->fmt.audio.i_blockalign    = GetWLE( &p_wf->nBlockAlign );
    p_sys->fmt.i_bitrate             = GetDWLE( &p_wf->nAvgBytesPerSec ) * 8;
    p_sys->fmt.audio.i_bitspersample = GetWLE( &p_wf->wBitsPerSample );
    if( i_size >= sizeof(WAVEFORMATEX) )
        p_sys->fmt.i_extra = __MIN( GetWLE( &p_wf->cbSize ), i_size - sizeof(WAVEFORMATEX) );
    i_extended = 0;

    /* Handle new WAVE_FORMAT_EXTENSIBLE wav files */
    /* see the following link for more information:
     * http://www.microsoft.com/whdc/device/audio/multichaud.mspx#EFAA */
    if( GetWLE( &p_wf->wFormatTag ) == WAVE_FORMAT_EXTENSIBLE &&
        i_size >= sizeof( WAVEFORMATEXTENSIBLE ) &&
        ( p_sys->fmt.i_extra + sizeof( WAVEFORMATEX )
            >= sizeof( WAVEFORMATEXTENSIBLE ) ) )
    {
        unsigned i_channel_mask;
        GUID guid_subformat;

        guid_subformat = p_wf_ext->SubFormat;
        guid_subformat.Data1 = GetDWLE( &p_wf_ext->SubFormat.Data1 );
        guid_subformat.Data2 = GetWLE( &p_wf_ext->SubFormat.Data2 );
        guid_subformat.Data3 = GetWLE( &p_wf_ext->SubFormat.Data3 );

        sf_tag_to_fourcc( &guid_subformat, &p_sys->fmt.i_codec, &psz_name );

        msg_Dbg( p_demux, "extensible format guid " GUID_FMT, GUID_PRINT(guid_subformat) );

        i_extended = sizeof( WAVEFORMATEXTENSIBLE ) - sizeof( WAVEFORMATEX );
        p_sys->fmt.i_extra -= i_extended;

        i_channel_mask = GetDWLE( &p_wf_ext->dwChannelMask );
        if( i_channel_mask )
        {
            int i_match = 0;
            p_sys->i_channel_mask = getChannelMask( &i_channel_mask, p_sys->fmt.audio.i_channels, &i_match );
            if( i_channel_mask )
                msg_Warn( p_demux, "Some channels are unrecognized or uselessly specified (0x%x)", i_channel_mask );
            if( i_match < p_sys->fmt.audio.i_channels )
            {
                int i_missing = p_sys->fmt.audio.i_channels - i_match;
                msg_Warn( p_demux, "Trying to fill up unspecified position for %d channels", p_sys->fmt.audio.i_channels - i_match );

                static const uint32_t pi_pair[] = { AOUT_CHAN_REARLEFT|AOUT_CHAN_REARRIGHT,
                                                    AOUT_CHAN_MIDDLELEFT|AOUT_CHAN_MIDDLERIGHT,
                                                    AOUT_CHAN_LEFT|AOUT_CHAN_RIGHT };
                /* FIXME: Unused yet
                static const uint32_t pi_center[] = { AOUT_CHAN_REARCENTER,
                                                      0,
                                                      AOUT_CHAN_CENTER }; */

                /* Try to complete with pair */
                for( unsigned i = 0; i < sizeof(pi_pair)/sizeof(*pi_pair); i++ )
                {
                    if( i_missing >= 2 && !(p_sys->i_channel_mask & pi_pair[i] ) )
                    {
                        i_missing -= 2;
                        p_sys->i_channel_mask |= pi_pair[i];
                    }
                }
                /* Well fill up with what we can */
                for( unsigned i = 0; i < sizeof(pi_channels_aout)/sizeof(*pi_channels_aout) && i_missing > 0; i++ )
                {
                    if( !( p_sys->i_channel_mask & pi_channels_aout[i] ) )
                    {
                        p_sys->i_channel_mask |= pi_channels_aout[i];
                        i_missing--;

                        if( i_missing <= 0 )
                            break;
                    }
                }

                i_match = p_sys->fmt.audio.i_channels - i_missing;
            }
            if( i_match < p_sys->fmt.audio.i_channels )
            {
                msg_Err( p_demux, "Invalid/unsupported channel mask" );
                p_sys->i_channel_mask = 0;
            }
        }
    }
    if( p_sys->i_channel_mask == 0 && p_sys->fmt.audio.i_channels > 2
     && p_sys->fmt.audio.i_channels <= AOUT_CHAN_MAX )
    {
        /* A dwChannelMask of 0 tells the audio device to render the first
         * channel to the first port on the device, the second channel to the
         * second port on the device, and so on. pi_default_channels is
         * different than pi_channels_aout. Indeed FLC/FRC must be treated a
         * SL/SR in that case. See "Default Channel Ordering" and "Details
         * about dwChannelMask" from msdn */

        static const uint32_t pi_default_channels[] = {
            AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER,
            AOUT_CHAN_LFE, AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,
            AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT, AOUT_CHAN_REARCENTER };

        for( unsigned i = 0; i < p_sys->fmt.audio.i_channels &&
             i < (sizeof(pi_default_channels) / sizeof(*pi_default_channels));
             i++ )
            p_sys->i_channel_mask |= pi_default_channels[i];
    }

    if( p_sys->i_channel_mask )
    {
        if( p_sys->fmt.i_codec == VLC_FOURCC('a','r','a','w') ||
            p_sys->fmt.i_codec == VLC_FOURCC('a','f','l','t') )
            p_sys->i_chans_to_reorder =
                aout_CheckChannelReorder( pi_channels_aout, NULL,
                                          p_sys->i_channel_mask,
                                          p_sys->pi_chan_table );

        msg_Dbg( p_demux, "channel mask: %x, reordering: %u",
                 p_sys->i_channel_mask, p_sys->i_chans_to_reorder );
    }

    p_sys->fmt.audio.i_physical_channels = p_sys->i_channel_mask;

    if( p_sys->fmt.i_extra > 0 )
    {
        p_sys->fmt.p_extra = malloc( p_sys->fmt.i_extra );
        if( unlikely(!p_sys->fmt.p_extra) )
        {
            p_sys->fmt.i_extra = 0;
            goto error;
        }
        memcpy( p_sys->fmt.p_extra, (uint8_t *)p_wf + sizeof( WAVEFORMATEX ) + i_extended,
                p_sys->fmt.i_extra );
    }

    msg_Dbg( p_demux, "format: 0x%4.4x, fourcc: %4.4s, channels: %d, "
             "freq: %u Hz, bitrate: %uKo/s, blockalign: %d, bits/samples: %d, "
             "extra size: %d",
             GetWLE( &p_wf->wFormatTag ), (char *)&p_sys->fmt.i_codec,
             p_sys->fmt.audio.i_channels, p_sys->fmt.audio.i_rate,
             p_sys->fmt.i_bitrate / 8 / 1024, p_sys->fmt.audio.i_blockalign,
             p_sys->fmt.audio.i_bitspersample, p_sys->fmt.i_extra );

    free( p_wf );
    p_wf = NULL;

    switch( p_sys->fmt.i_codec )
    {
    case VLC_FOURCC( 'a', 'r', 'a', 'w' ):
    case VLC_FOURCC( 'a', 'f', 'l', 't' ):
    case VLC_FOURCC( 'u', 'l', 'a', 'w' ):
    case VLC_CODEC_ALAW:
    case VLC_CODEC_MULAW:
        if( FrameInfo_PCM( &p_sys->i_frame_size, &p_sys->i_frame_samples,
                           &p_sys->fmt ) )
            goto error;
        p_sys->fmt.i_codec =
            vlc_fourcc_GetCodecAudio( p_sys->fmt.i_codec,
                                      p_sys->fmt.audio.i_bitspersample );
        if( p_sys->fmt.i_codec == 0 ) {
            msg_Err( p_demux, "Unrecognized codec" );
            goto error;
        }
        break;
    case VLC_CODEC_ADPCM_MS:
    /* FIXME not sure at all FIXME */
    case VLC_FOURCC( 'm', 's', 0x00, 0x61 ):
    case VLC_FOURCC( 'm', 's', 0x00, 0x62 ):
        if( FrameInfo_MS_ADPCM( &p_sys->i_frame_size, &p_sys->i_frame_samples,
                                &p_sys->fmt ) )
            goto error;
        break;
    case VLC_CODEC_ADPCM_IMA_WAV:
        if( FrameInfo_IMA_ADPCM( &p_sys->i_frame_size, &p_sys->i_frame_samples,
                                 &p_sys->fmt ) )
            goto error;
        break;
    case VLC_CODEC_ADPCM_CREATIVE:
        if( FrameInfo_Creative_ADPCM( &p_sys->i_frame_size, &p_sys->i_frame_samples,
                                      &p_sys->fmt ) )
            goto error;
        break;
    case VLC_CODEC_MPGA:
    case VLC_CODEC_A52:
        /* FIXME set end of area FIXME */
        goto error;
    case VLC_CODEC_GSM_MS:
    case VLC_CODEC_ADPCM_G726:
    case VLC_CODEC_TRUESPEECH:
    case VLC_CODEC_ATRAC3P:
    case VLC_CODEC_ATRAC3:
    case VLC_CODEC_G723_1:
    case VLC_CODEC_WMA2:
        if( FrameInfo_MSGSM( &p_sys->i_frame_size, &p_sys->i_frame_samples,
                             &p_sys->fmt ) )
            goto error;
        break;
    default:
        msg_Err( p_demux, "unsupported codec (%4.4s)",
                 (char*)&p_sys->fmt.i_codec );
        goto error;
    }

    if( p_sys->i_frame_size <= 0 || p_sys->i_frame_samples <= 0 )
    {
        msg_Dbg( p_demux, "invalid frame size: %i %i", p_sys->i_frame_size,
                                                       p_sys->i_frame_samples );
        goto error;
    }
    if( p_sys->fmt.audio.i_rate == 0 )
    {
        msg_Dbg( p_demux, "invalid sample rate: %i", p_sys->fmt.audio.i_rate );
        goto error;
    }

    msg_Dbg( p_demux, "found %s audio format", psz_name );

    return VLC_SUCCESS;

error:
    free( p_wf );
    return VLC_EGENERIC;
}

static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    const uint8_t *p_peek;
    bool           b_is_rf64;
    uint32_t       i_size;

    /* Is it a wav file ? */
    if( vlc_stream_Peek( p_demux->s, &p_peek, 12 ) < 12 )
        return VLC_EGENERIC;

    b_is_rf64 = ( memcmp( p_peek, "RF64", 4 ) == 0 );
    if( ( !b_is_rf64 && memcmp( p_peek, "RIFF", 4 ) ) ||
          memcmp( &p_peek[8], "WAVE", 4 ) )
    {
        return VLC_EGENERIC;
    }

    p_demux->pf_demux     = Demux;
    p_demux->pf_control   = Control;
    p_demux->p_sys        = p_sys = malloc( sizeof( *p_sys ) );
    if( unlikely(!p_sys) )
        return VLC_ENOMEM;

    es_format_Init( &p_sys->fmt, AUDIO_ES, 0 );
    p_sys->p_es           = NULL;
    p_sys->i_data_pos = p_sys->i_data_size = 0;
    p_sys->i_chans_to_reorder = 0;
    p_sys->i_channel_mask = 0;

    /* skip riff header */
    if( vlc_stream_Read( p_demux->s, NULL, 12 ) != 12 )
        goto error;

    bool eof = false;
    enum wav_chunk_id id;
    while( !eof && ( ChunkGetNext( p_demux, &id, &i_size ) ) == VLC_SUCCESS )
    {
        if( i_size == 0 )
        {
            msg_Err( p_demux, "invalid chunk with a size 0");
            goto error;
        }

        switch( id )
        {
            case wav_chunk_id_data:
            {
                uint64_t i_stream_size;
                if( vlc_stream_GetSize( p_demux->s, &i_stream_size ) != VLC_SUCCESS )
                    goto error;
                p_sys->i_data_pos = vlc_stream_Tell( p_demux->s );

                if( !b_is_rf64 && i_stream_size >= i_size + p_sys->i_data_pos )
                    p_sys->i_data_size = i_size;

                if( likely( b_is_rf64
                 || p_sys->i_data_pos + i_size == i_stream_size ) )
                {
                    /* Bypass the final ChunkGetNext() to avoid a read+seek
                     * since this chunk is the last one */
                    eof = true;
                }   /* Unlikely case where there is a chunk after 'data' */
                else if( ChunkSkip( p_demux, i_size ) != VLC_SUCCESS )
                    goto error;
                break;
            }
            case wav_chunk_id_ds64:
                if( b_is_rf64 )
                {
                    if( ChunkParseDS64( p_demux, i_size ) != VLC_SUCCESS )
                        goto error;
                }
                else
                {
                    msg_Err( p_demux, "'ds64' chunk found but format not RF64" );
                    goto error;
                }
                break;
            case wav_chunk_id_fmt:
                if( ChunkParseFmt( p_demux, i_size ) != VLC_SUCCESS )
                    goto error;
                break;
        }
    }

    if( p_sys->i_data_pos == 0 || p_sys->i_data_size == 0
     || p_sys->i_frame_samples <= 0 )
    {
        msg_Err( p_demux, "'%s' chunk not found",
                 p_sys->i_data_pos == 0 ? "data" :
                 p_sys->i_frame_samples <= 0 ? "fmt " :
                 b_is_rf64 ? "ds64" : "data" );
        goto error;
    }

    /* Seek back to data position if needed */
    if( unlikely( vlc_stream_Tell( p_demux->s ) != p_sys->i_data_pos )
     && vlc_stream_Seek( p_demux->s, p_sys->i_data_pos ) != VLC_SUCCESS )
        goto error;

    if( p_sys->fmt.i_bitrate <= 0 )
    {
        p_sys->fmt.i_bitrate = (int64_t)p_sys->i_frame_size *
            p_sys->fmt.audio.i_rate * 8 / p_sys->i_frame_samples;
    }

    p_sys->fmt.i_id = 0;
    p_sys->p_es = es_out_Add( p_demux->out, &p_sys->fmt );
    if( unlikely(p_sys->p_es == NULL) )
        goto error;

    date_Init( &p_sys->pts, p_sys->fmt.audio.i_rate, 1 );
    date_Set( &p_sys->pts, VLC_TICK_0 );

    return VLC_SUCCESS;

error:
    es_format_Clean( &p_sys->fmt );
    free( p_sys );
    return VLC_EGENERIC;
}

vlc_module_begin ()
    set_description( N_("WAV demuxer") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_capability( "demux", 142 )
    set_callbacks( Open, Close )
vlc_module_end ()
