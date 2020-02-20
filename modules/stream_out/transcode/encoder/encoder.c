/*****************************************************************************
 * encoder.c: transcoding encoders
 *****************************************************************************
 * Copyright (C) 2018 VideoLabs, VideoLAN and VLC authors
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
#include <vlc_picture_fifo.h>
#include <vlc_aout.h>
#include <vlc_sout.h>

#include "encoder.h"
#include "encoder_priv.h"

void transcode_encoder_config_init( transcode_encoder_config_t *p_cfg )
{
    memset( p_cfg, 0, sizeof(*p_cfg) );
}

void transcode_encoder_config_clean( transcode_encoder_config_t *p_cfg )
{
    free( p_cfg->psz_name );
    free( p_cfg->psz_lang );
    config_ChainDestroy( p_cfg->p_config_chain );
}

void transcode_encoder_delete( transcode_encoder_t *p_enc )
{
    if( p_enc->p_encoder )
    {
        if( p_enc->p_encoder->fmt_in.i_cat == VIDEO_ES )
        {
            block_ChainRelease( p_enc->p_buffers );
            picture_fifo_Delete( p_enc->pp_pics );
        }
        es_format_Clean( &p_enc->p_encoder->fmt_in );
        es_format_Clean( &p_enc->p_encoder->fmt_out );
        vlc_object_delete(p_enc->p_encoder);
    }
    free( p_enc );
}

transcode_encoder_t * transcode_encoder_new( encoder_t *p_encoder,
                                             const es_format_t *p_fmt )
{
    if( !p_encoder )
        return NULL;

    switch( p_fmt->i_cat )
    {
        case VIDEO_ES:
        case AUDIO_ES:
        case SPU_ES:
            break;
        default:
            return NULL;
    }

    transcode_encoder_t *p_enc = calloc( 1, sizeof(*p_enc) );
    if( !p_enc )
    {
        vlc_object_delete(p_encoder);
        return NULL;
    }

    p_enc->p_encoder = p_encoder;
    p_enc->p_encoder->p_module = NULL;

    /* Create destination format */
    es_format_Init( &p_enc->p_encoder->fmt_in, p_fmt->i_cat, 0 );
    es_format_Copy( &p_enc->p_encoder->fmt_in, p_fmt );
    es_format_Init( &p_enc->p_encoder->fmt_out, p_fmt->i_cat, 0 );
    p_enc->p_encoder->fmt_out.i_id    = p_fmt->i_id;
    p_enc->p_encoder->fmt_out.i_group = p_fmt->i_group;
    if( p_enc->p_encoder->fmt_in.psz_language )
        p_enc->p_encoder->fmt_out.psz_language = strdup( p_enc->p_encoder->fmt_in.psz_language );

    switch( p_fmt->i_cat )
    {
        case VIDEO_ES:
            p_enc->pp_pics = picture_fifo_New();
            if( !p_enc->pp_pics )
            {
                es_format_Clean( &p_enc->p_encoder->fmt_in );
                es_format_Clean( &p_enc->p_encoder->fmt_out );
                vlc_object_delete(p_enc->p_encoder);
                free( p_enc );
                return NULL;
            }
            vlc_mutex_init( &p_enc->lock_out );
            break;
        default:
            break;
    }

    return p_enc;
}

const es_format_t *transcode_encoder_format_in( const transcode_encoder_t *p_enc )
{
    return &p_enc->p_encoder->fmt_in;
}

const es_format_t *transcode_encoder_format_out( const transcode_encoder_t *p_enc )
{
    return &p_enc->p_encoder->fmt_out;
}

void transcode_encoder_update_format_in( transcode_encoder_t *p_enc, const es_format_t *fmt )
{
    es_format_Clean( &p_enc->p_encoder->fmt_in );
    es_format_Copy( &p_enc->p_encoder->fmt_in, fmt );
}

void transcode_encoder_update_format_out( transcode_encoder_t *p_enc, const es_format_t *fmt )
{
    es_format_Clean( &p_enc->p_encoder->fmt_out );
    es_format_Copy( &p_enc->p_encoder->fmt_out, fmt );
}

bool transcode_encoder_opened( const transcode_encoder_t *p_enc )
{
    return p_enc->p_encoder && p_enc->p_encoder->p_module;
}

block_t * transcode_encoder_encode( transcode_encoder_t *p_enc, void *in )
{
    switch( p_enc->p_encoder->fmt_in.i_cat )
    {
        case VIDEO_ES:
            return transcode_encoder_video_encode( p_enc, in );
        case AUDIO_ES:
            return transcode_encoder_audio_encode( p_enc, in );
        case SPU_ES:
            return transcode_encoder_spu_encode( p_enc, in );
        default:
            vlc_assert_unreachable();
            return NULL;
    }
}

block_t * transcode_encoder_get_output_async( transcode_encoder_t *p_enc )
{
    vlc_mutex_lock( &p_enc->lock_out );
    block_t *p_data = p_enc->p_buffers;
    p_enc->p_buffers = NULL;
    vlc_mutex_unlock( &p_enc->lock_out );
    return p_data;
}

void transcode_encoder_close( transcode_encoder_t *p_enc )
{
    if( !p_enc->p_encoder->p_module )
        return;

    switch( p_enc->p_encoder->fmt_in.i_cat )
    {
        case VIDEO_ES:
            transcode_encoder_video_close( p_enc );
            break;
        default:
            module_unneed( p_enc->p_encoder, p_enc->p_encoder->p_module );
            break;
    }

    p_enc->p_encoder->p_module = NULL;
}


int transcode_encoder_open( transcode_encoder_t *p_enc,
                            const transcode_encoder_config_t *p_cfg )
{
    switch( p_enc->p_encoder->fmt_in.i_cat )
    {
        case SPU_ES:
            return transcode_encoder_spu_open( p_enc, p_cfg );
        case AUDIO_ES:
            return transcode_encoder_audio_open( p_enc, p_cfg );
        case VIDEO_ES:
            return transcode_encoder_video_open( p_enc, p_cfg );
        default:
            return VLC_EGENERIC;
    }
}

int transcode_encoder_drain( transcode_encoder_t *p_enc, block_t **out )
{
    if( !transcode_encoder_opened( p_enc ) )
        return VLC_EGENERIC;

    switch( p_enc->p_encoder->fmt_in.i_cat )
    {
        case VIDEO_ES:
            return transcode_encoder_video_drain( p_enc, out );
        case AUDIO_ES:
            return transcode_encoder_audio_drain( p_enc, out );
        case SPU_ES:
            return VLC_SUCCESS;
        default:
            return VLC_EGENERIC;
    }
}

int transcode_encoder_test( encoder_t *p_encoder,
                            const transcode_encoder_config_t *p_cfg,
                            const es_format_t *p_dec_fmtin,
                            vlc_fourcc_t i_codec_in,
                            es_format_t *p_enc_wanted_in )
{
    if( !p_encoder )
        return VLC_EGENERIC;

    switch ( p_dec_fmtin->i_cat )
    {
        case VIDEO_ES:
            return transcode_encoder_video_test( p_encoder, p_cfg, p_dec_fmtin,
                                                 i_codec_in, p_enc_wanted_in );
        case AUDIO_ES:
            return transcode_encoder_audio_test( p_encoder, p_cfg, p_dec_fmtin,
                                                 i_codec_in, p_enc_wanted_in );
        default:
            return VLC_EGENERIC;
    }
}
