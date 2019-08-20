/*****************************************************************************
 * audio.c: transcoding audio encoder
 *****************************************************************************
 * Copyright (C) 2003-2009 VLC authors and VideoLAN
 *               2018 VideoLabs, VideoLAN and VLC authors
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
 *          Antoine Cellerier <dionoea at videolan dot org>
 *          Ilkka Ollakka <ileoo at videolan dot org>
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
 * along with this program; if not, If not, see https://www.gnu.org/licenses/
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_codec.h>
#include <vlc_aout.h>
#include <vlc_sout.h>

#include "encoder.h"
#include "encoder_priv.h"

static const int pi_channels_maps[9] =
{
    0,
    AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LFE  | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARCENTER | AOUT_CHAN_MIDDLELEFT
     | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT
     | AOUT_CHAN_LFE,
};

int transcode_encoder_audio_open( transcode_encoder_t *p_enc,
                                  const transcode_encoder_config_t *p_cfg )
{
    p_enc->p_encoder->p_cfg = p_cfg->p_config_chain;
    p_enc->p_encoder->fmt_out.i_codec = p_cfg->i_codec;

    p_enc->p_encoder->p_module = module_need( p_enc->p_encoder, "encoder",
                                              p_cfg->psz_name, true );

    if( p_enc->p_encoder->p_module )
    {
        p_enc->p_encoder->fmt_out.i_codec =
                vlc_fourcc_GetCodec( AUDIO_ES, p_enc->p_encoder->fmt_out.i_codec );
    }

    return ( p_enc->p_encoder->p_module ) ? VLC_SUCCESS: VLC_EGENERIC;
}

static int encoder_audio_configure( const transcode_encoder_config_t *p_cfg,
                                    const audio_format_t *p_dec_out,
                                    encoder_t *p_enc, bool b_keep_fmtin )
{
    audio_format_t *p_enc_in = &p_enc->fmt_in.audio;
    audio_format_t *p_enc_out = &p_enc->fmt_out.audio;

    p_enc->p_cfg = p_cfg->p_config_chain;

    if ( p_cfg->psz_lang )
    {
        free( p_enc->fmt_in.psz_language );
        free( p_enc->fmt_out.psz_language );
        p_enc->fmt_in.psz_language = strdup( p_cfg->psz_lang );
        p_enc->fmt_out.psz_language = strdup( p_cfg->psz_lang );
    }

    /* Complete destination format */
    p_enc->fmt_out.i_codec = p_cfg->i_codec;
    p_enc->fmt_out.audio.i_format = p_cfg->i_codec;
    p_enc->fmt_out.i_bitrate = p_cfg->audio.i_bitrate;
    p_enc_out->i_rate = p_cfg->audio.i_sample_rate ? p_cfg->audio.i_sample_rate
                                                   : p_dec_out->i_rate;
    p_enc_out->i_bitspersample = p_dec_out->i_bitspersample;
    p_enc_out->i_channels = p_cfg->audio.i_channels ? p_cfg->audio.i_channels
                                                    : p_dec_out->i_channels;
    aout_FormatPrepare( p_enc_out );
    assert(p_enc_out->i_channels > 0);
    if( p_enc_out->i_channels >= ARRAY_SIZE(pi_channels_maps) )
        p_enc_out->i_channels = ARRAY_SIZE(pi_channels_maps) - 1;

    p_enc_out->i_physical_channels = pi_channels_maps[p_enc_out->i_channels];

    if( b_keep_fmtin ) /* This is tested/wanted decoder fmtin */
        return VLC_SUCCESS;

    p_enc_in->i_physical_channels = p_enc_out->i_physical_channels;

    /* Initialization of encoder format structures */
    p_enc->fmt_in.i_codec = p_dec_out->i_format;
    p_enc_in->i_format = p_dec_out->i_format;
    p_enc_in->i_rate = p_enc_out->i_rate;
    p_enc_in->i_physical_channels = p_enc_out->i_physical_channels;
    aout_FormatPrepare( p_enc_in );

    /* Fix input format */
    p_enc_in->i_format = p_enc->fmt_in.i_codec;
    if( !p_enc_in->i_physical_channels )
    {
        if( p_enc_in->i_channels < ARRAY_SIZE(pi_channels_maps) )
            p_enc_in->i_physical_channels = pi_channels_maps[p_enc_in->i_channels];
    }
    aout_FormatPrepare( p_enc_in );

    return VLC_SUCCESS;
}

int transcode_encoder_audio_configure( const transcode_encoder_config_t *p_cfg,
                                       const audio_format_t *p_dec_out,
                                       transcode_encoder_t *p_enc,
                                       bool b_keep_fmtin )
{
    return encoder_audio_configure( p_cfg, p_dec_out, p_enc->p_encoder, b_keep_fmtin );
}

int transcode_encoder_audio_test( encoder_t *p_encoder,
                                  const transcode_encoder_config_t *p_cfg,
                                  const es_format_t *p_dec_out,
                                  vlc_fourcc_t i_codec_in,
                                  es_format_t *p_enc_wanted_in )
{
    p_encoder->p_cfg = p_cfg->p_config_chain;

    es_format_Init( &p_encoder->fmt_in, AUDIO_ES, i_codec_in );
    p_encoder->fmt_in.audio = p_dec_out->audio;
    es_format_Init( &p_encoder->fmt_out, AUDIO_ES, p_cfg->i_codec );

    audio_format_t *p_afmt_out = &p_encoder->fmt_out.audio;

    if( encoder_audio_configure( p_cfg, &p_dec_out->audio, p_encoder, false ) )
    {
        es_format_Clean( &p_encoder->fmt_in );
        es_format_Clean( &p_encoder->fmt_out );
        vlc_object_delete(p_encoder);
        return VLC_EGENERIC;
    }

    p_encoder->fmt_in.audio.i_format = i_codec_in;

    if( p_afmt_out->i_channels == 0 )
    {
        p_afmt_out->i_channels = 2;
        p_afmt_out->i_physical_channels = AOUT_CHANS_STEREO;
    }

    module_t *p_module = module_need( p_encoder, "encoder", p_cfg->psz_name, true );
    if( !p_module )
    {
        msg_Err( p_encoder, "cannot find audio encoder (module:%s fourcc:%4.4s). "
                           "Take a look few lines earlier to see possible reason.",
                           p_cfg->psz_name ? p_cfg->psz_name : "any",
                           (char *)&p_cfg->i_codec );
    }
    else
    {
        /* Close the encoder.
         * We'll open it only when we have the first frame. */
        module_unneed( p_encoder, p_module );
    }

    p_encoder->fmt_in.audio.i_format = p_encoder->fmt_in.i_codec;

    /* copy our requested format */
    es_format_Copy( p_enc_wanted_in, &p_encoder->fmt_in );

    es_format_Clean( &p_encoder->fmt_in );
    es_format_Clean( &p_encoder->fmt_out );

    vlc_object_delete(p_encoder);

    return p_module != NULL ? VLC_SUCCESS : VLC_EGENERIC;
}

block_t * transcode_encoder_audio_encode( transcode_encoder_t *p_enc, block_t *p_block )
{
    return p_enc->p_encoder->pf_encode_audio( p_enc->p_encoder, p_block );
}

int transcode_encoder_audio_drain( transcode_encoder_t *p_enc, block_t **out )
{
    block_t *p_block;
    do {
        p_block = transcode_encoder_audio_encode( p_enc, NULL );
        block_ChainAppend( out, p_block );
    } while( p_block );
    return VLC_SUCCESS;
}
