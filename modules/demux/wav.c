/*****************************************************************************
 * wav.c : wav file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc_aout.h>
#include <vlc_codecs.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("WAV demuxer") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_capability( "demux", 142 )
    set_callbacks( Open, Close )
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

    int64_t         i_data_pos;
    unsigned int    i_data_size;

    unsigned int    i_frame_size;
    int             i_frame_samples;

    date_t          pts;

    uint32_t i_channel_mask;
    bool b_chan_reorder;              /* do we need channel reordering */
    int pi_chan_table[AOUT_CHAN_MAX];
};

static int ChunkFind( demux_t *, const char *, unsigned int * );

static int FrameInfo_IMA_ADPCM( unsigned int *, int *, const es_format_t * );
static int FrameInfo_MS_ADPCM ( unsigned int *, int *, const es_format_t * );
static int FrameInfo_PCM      ( unsigned int *, int *, const es_format_t * );
static int FrameInfo_MSGSM    ( unsigned int *, int *, const es_format_t * );

static const uint32_t pi_channels_src[] =
    { WAVE_SPEAKER_FRONT_LEFT, WAVE_SPEAKER_FRONT_RIGHT,
      WAVE_SPEAKER_FRONT_CENTER, WAVE_SPEAKER_LOW_FREQUENCY,
      WAVE_SPEAKER_BACK_LEFT, WAVE_SPEAKER_BACK_RIGHT, WAVE_SPEAKER_BACK_CENTER,
      WAVE_SPEAKER_SIDE_LEFT, WAVE_SPEAKER_SIDE_RIGHT, 0 };
static const uint32_t pi_channels_in[] =
    { AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT,
      AOUT_CHAN_CENTER, AOUT_CHAN_LFE,
      AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, AOUT_CHAN_REARCENTER,
      AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT, 0 };

/*****************************************************************************
 * Open: check file and initializes structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    const uint8_t *p_peek;
    unsigned int   i_size;
    unsigned int   i_extended;
    const char    *psz_name;

    WAVEFORMATEXTENSIBLE *p_wf_ext = NULL;
    WAVEFORMATEX         *p_wf = NULL;

    /* Is it a wav file ? */
    if( stream_Peek( p_demux->s, &p_peek, 12 ) < 12 )
        return VLC_EGENERIC;

    if( memcmp( p_peek, "RIFF", 4 ) || memcmp( &p_peek[8], "WAVE", 4 ) )
    {
        return VLC_EGENERIC;
    }

    p_demux->pf_demux     = Demux;
    p_demux->pf_control   = Control;
    p_demux->p_sys        = p_sys = malloc( sizeof( *p_sys ) );
    if( unlikely(!p_sys) )
        return VLC_ENOMEM;

    p_sys->p_es           = NULL;
    p_sys->b_chan_reorder = false;
    p_sys->i_channel_mask = 0;

    /* skip riff header */
    if( stream_Read( p_demux->s, NULL, 12 ) != 12 )
        goto error;

    /* search fmt chunk */
    if( ChunkFind( p_demux, "fmt ", &i_size ) )
    {
        msg_Err( p_demux, "cannot find 'fmt ' chunk" );
        goto error;
    }
    i_size += 2;
    if( i_size < sizeof( WAVEFORMATEX ) )
    {
        msg_Err( p_demux, "invalid 'fmt ' chunk" );
        goto error;
    }
    if( stream_Read( p_demux->s, NULL, 8 ) != 8 )
        goto error;


    /* load waveformatex */
    p_wf_ext = malloc( i_size );
    if( unlikely( !p_wf_ext ) )
         goto error;

    p_wf         = &p_wf_ext->Format;
    p_wf->cbSize = 0;
    i_size      -= 2;
    if( stream_Read( p_demux->s, p_wf, i_size ) != (int)i_size ||
        ( ( i_size & 1 ) && stream_Read( p_demux->s, NULL, 1 ) != 1 ) )
    {
        msg_Err( p_demux, "cannot load 'fmt ' chunk" );
        goto error;
    }

    es_format_Init( &p_sys->fmt, AUDIO_ES, 0 );
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

        i_extended = sizeof( WAVEFORMATEXTENSIBLE ) - sizeof( WAVEFORMATEX );
        p_sys->fmt.i_extra -= i_extended;

        i_channel_mask = GetDWLE( &p_wf_ext->dwChannelMask );
        if( i_channel_mask )
        {
            int i_match = 0;
            for( unsigned i = 0; i < sizeof(pi_channels_src)/sizeof(*pi_channels_src); i++ )
            {
                if( i_channel_mask & pi_channels_src[i] )
                {
                    if( !( p_sys->i_channel_mask & pi_channels_in[i] ) )
                        i_match++;

                    i_channel_mask &= ~pi_channels_src[i];
                    p_sys->i_channel_mask |= pi_channels_in[i];

                    if( i_match >= p_sys->fmt.audio.i_channels )
                        break;
                }
            }
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
                for( unsigned i = 0; i < sizeof(pi_channels_in)/sizeof(*pi_channels_in) && i_missing > 0; i++ )
                {
                    if( !( p_sys->i_channel_mask & pi_channels_in[i] ) )
                    {
                        p_sys->i_channel_mask |= pi_channels_in[i];
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
    else if( GetWLE( &p_wf->wFormatTag ) == WAVE_FORMAT_PCM &&
             p_sys->fmt.audio.i_channels > 2 && p_sys->fmt.audio.i_channels <= 9 )
    {
        for( int i = 0; i < p_sys->fmt.audio.i_channels; i++ )
            p_sys->i_channel_mask |= pi_channels_in[i];
    }

    if( p_sys->i_channel_mask )
    {
        if( p_sys->fmt.i_codec == VLC_FOURCC('a','r','a','w') ||
            p_sys->fmt.i_codec == VLC_FOURCC('p','c','m',' ') ||
            p_sys->fmt.i_codec == VLC_FOURCC('a','f','l','t') )
            p_sys->b_chan_reorder =
                aout_CheckChannelReorder( pi_channels_in, NULL,
                                          p_sys->i_channel_mask,
                                          p_sys->pi_chan_table );

        msg_Dbg( p_demux, "channel mask: %x, reordering: %i",
                 p_sys->i_channel_mask, (int)p_sys->b_chan_reorder );
    }

    p_sys->fmt.audio.i_physical_channels =
    p_sys->fmt.audio.i_original_channels = p_sys->i_channel_mask;

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
    case VLC_FOURCC( 'p', 'c', 'm', ' ' ):
        if( FrameInfo_PCM( &p_sys->i_frame_size, &p_sys->i_frame_samples,
                           &p_sys->fmt ) )
            goto error;
        break;
    case VLC_CODEC_ADPCM_MS:
        if( FrameInfo_MS_ADPCM( &p_sys->i_frame_size, &p_sys->i_frame_samples,
                                &p_sys->fmt ) )
            goto error;
        break;
    case VLC_CODEC_ADPCM_IMA_WAV:
        if( FrameInfo_IMA_ADPCM( &p_sys->i_frame_size, &p_sys->i_frame_samples,
                                 &p_sys->fmt ) )
            goto error;
        break;
    case VLC_FOURCC( 'm', 's', 0x00, 0x61 ):
    case VLC_FOURCC( 'm', 's', 0x00, 0x62 ):
        /* FIXME not sure at all FIXME */
        if( FrameInfo_MS_ADPCM( &p_sys->i_frame_size, &p_sys->i_frame_samples,
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
    case VLC_CODEC_ATRAC3:
    case VLC_CODEC_G723_1:
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
    if( p_sys->fmt.audio.i_rate <= 0 )
    {
        msg_Dbg( p_demux, "invalid sample rate: %i", p_sys->fmt.audio.i_rate );
        goto error;
    }

    msg_Dbg( p_demux, "found %s audio format", psz_name );

    if( ChunkFind( p_demux, "data", &p_sys->i_data_size ) )
    {
        msg_Err( p_demux, "cannot find 'data' chunk" );
        goto error;
    }
    if( stream_Read( p_demux->s, NULL, 8 ) != 8 )
        goto error;
    p_sys->i_data_pos = stream_Tell( p_demux->s );

    if( p_sys->fmt.i_bitrate <= 0 )
    {
        p_sys->fmt.i_bitrate = (int64_t)p_sys->i_frame_size *
            p_sys->fmt.audio.i_rate * 8 / p_sys->i_frame_samples;
    }

    p_sys->p_es = es_out_Add( p_demux->out, &p_sys->fmt );

    date_Init( &p_sys->pts, p_sys->fmt.audio.i_rate, 1 );
    date_Set( &p_sys->pts, 1 );

    return VLC_SUCCESS;

error:
    msg_Err( p_demux, "An error occurred during wav demuxing" );
    free( p_wf );
    free( p_sys );
    return VLC_EGENERIC;
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
    const int64_t i_pos = stream_Tell( p_demux->s );

    if( p_sys->i_data_size > 0 &&
        i_pos >= p_sys->i_data_pos + p_sys->i_data_size )
    {
        /* EOF */
        return 0;
    }

    if( ( p_block = stream_Block( p_demux->s, p_sys->i_frame_size ) ) == NULL )
    {
        msg_Warn( p_demux, "cannot read data" );
        return 0;
    }

    p_block->i_dts =
    p_block->i_pts = VLC_TS_0 + date_Get( &p_sys->pts );

    /* set PCR */
    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block->i_pts );

    /* Do the channel reordering */
    if( p_sys->b_chan_reorder )
        aout_ChannelReorder( p_block->p_buffer, p_block->i_buffer,
                             p_sys->fmt.audio.i_channels,
                             p_sys->pi_chan_table,
                             p_sys->fmt.audio.i_bitspersample );

    es_out_Send( p_demux->out, p_sys->p_es, p_block );

    date_Increment( &p_sys->pts, p_sys->i_frame_samples );

    return 1;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close ( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys   = p_demux->p_sys;

    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;
    int64_t i_end = -1;

    if( p_sys->i_data_size > 0 )
        i_end = p_sys->i_data_pos + p_sys->i_data_size;

    return demux_vaControlHelper( p_demux->s, p_sys->i_data_pos, i_end,
                                   p_sys->fmt.i_bitrate,
                                   p_sys->fmt.audio.i_blockalign,
                                   i_query, args );
}

/*****************************************************************************
 * Local functions
 *****************************************************************************/
static int ChunkFind( demux_t *p_demux, const char *fcc, unsigned int *pi_size )
{
    const uint8_t *p_peek;

    for( ;; )
    {
        uint32_t i_size;

        if( stream_Peek( p_demux->s, &p_peek, 8 ) < 8 )
        {
            msg_Err( p_demux, "cannot peek" );
            return VLC_EGENERIC;
        }

        i_size = GetDWLE( p_peek + 4 );

        msg_Dbg( p_demux, "chunk: fcc=`%4.4s` size=%"PRIu32, p_peek, i_size );

        if( !memcmp( p_peek, fcc, 4 ) )
        {
            if( pi_size )
            {
                *pi_size = i_size;
            }
            return VLC_SUCCESS;
        }

        /* Skip chunk */
        if( stream_Read( p_demux->s, NULL, 8 ) != 8 ||
            stream_Read( p_demux->s, NULL, i_size ) != (int)i_size ||
            ( (i_size & 1) && stream_Read( p_demux->s, NULL, 1 ) != 1 ) )
            return VLC_EGENERIC;
    }
}

static int FrameInfo_PCM( unsigned int *pi_size, int *pi_samples,
                          const es_format_t *p_fmt )
{
    int i_bytes;

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
    if( p_fmt->audio.i_channels <= 0 )
        return VLC_EGENERIC;

    *pi_samples = 2 + 2 * ( p_fmt->audio.i_blockalign -
        7 * p_fmt->audio.i_channels ) / p_fmt->audio.i_channels;
    *pi_size = p_fmt->audio.i_blockalign;

    return VLC_SUCCESS;
}

static int FrameInfo_IMA_ADPCM( unsigned int *pi_size, int *pi_samples,
                                const es_format_t *p_fmt )
{
    if( p_fmt->audio.i_channels <= 0 )
        return VLC_EGENERIC;

    *pi_samples = 2 * ( p_fmt->audio.i_blockalign -
        4 * p_fmt->audio.i_channels ) / p_fmt->audio.i_channels;
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

