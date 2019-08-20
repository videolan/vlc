/*****************************************************************************
 * audio.c: transcoding stream output module (audio)
 *****************************************************************************
 * Copyright (C) 2003-2009 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
 *          Antoine Cellerier <dionoea at videolan dot org>
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
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_meta.h>
#include <vlc_modules.h>
#include <vlc_sout.h>

#include "transcode.h"

static int audio_update_format( decoder_t *p_dec )
{
    struct decoder_owner *p_owner = dec_get_owner( p_dec );
    sout_stream_id_sys_t *id = p_owner->id;

    p_dec->fmt_out.audio.i_format = p_dec->fmt_out.i_codec;
    aout_FormatPrepare( &p_dec->fmt_out.audio );

    if( !AOUT_FMT_LINEAR(&p_dec->fmt_out.audio) )
        return VLC_EGENERIC;

    vlc_mutex_lock(&id->fifo.lock);
    es_format_Clean( &id->decoder_out );
    es_format_Copy( &id->decoder_out, &p_dec->fmt_out );
    vlc_mutex_unlock(&id->fifo.lock);

    return VLC_SUCCESS;
}

static int transcode_audio_filters_init( sout_stream_t *p_stream,
                                         const sout_filters_config_t *p_cfg,
                                         const audio_format_t *p_dec_out,
                                         const audio_format_t *p_enc_in,
                                         aout_filters_t **pp_chain )
{
    /* Load user specified audio filters */
    /* XXX: These variable names come kinda out of nowhere... */
    var_Create( p_stream, "audio-time-stretch", VLC_VAR_BOOL );
    var_Create( p_stream, "audio-filter", VLC_VAR_STRING );
    if( p_cfg->psz_filters )
        var_SetString( p_stream, "audio-filter", p_cfg->psz_filters );
    *pp_chain = aout_FiltersNew( p_stream, p_dec_out, p_enc_in, NULL );
    var_Destroy( p_stream, "audio-filter" );
    var_Destroy( p_stream, "audio-time-stretch" );
    return ( *pp_chain != NULL ) ? VLC_SUCCESS : VLC_EGENERIC;
}

static void decoder_queue_audio( decoder_t *p_dec, block_t *p_audio )
{
    struct decoder_owner *p_owner = dec_get_owner( p_dec );
    sout_stream_id_sys_t *id = p_owner->id;

    vlc_mutex_lock(&id->fifo.lock);
    *id->fifo.audio.last = p_audio;
    id->fifo.audio.last = &p_audio->p_next;
    vlc_mutex_unlock(&id->fifo.lock);
}

static block_t *transcode_dequeue_all_audios( sout_stream_id_sys_t *id )
{
    vlc_mutex_lock(&id->fifo.lock);
    block_t *p_audio_bufs = id->fifo.audio.first;
    id->fifo.audio.first = NULL;
    id->fifo.audio.last = &id->fifo.audio.first;
    vlc_mutex_unlock(&id->fifo.lock);

    return p_audio_bufs;
}

int transcode_audio_init( sout_stream_t *p_stream, const es_format_t *p_fmt,
                          sout_stream_id_sys_t *id )
{
    msg_Dbg( p_stream,
             "creating audio transcoding from fcc=`%4.4s' to fcc=`%4.4s'",
             (char*)&p_fmt->i_codec, (char*)&id->p_enccfg->i_codec );

    id->fifo.audio.first = NULL;
    id->fifo.audio.last = &id->fifo.audio.first;
    id->b_transcode = true;
    es_format_Init( &id->decoder_out, AUDIO_ES, 0 );

    /*
     * Open decoder
     */
    dec_get_owner( id->p_decoder )->id = id;

    static const struct decoder_owner_callbacks dec_cbs =
    {
        .audio = {
            .format_update = audio_update_format,
            .queue = decoder_queue_audio,
        },
    };
    id->p_decoder->cbs = &dec_cbs;
    id->p_decoder->pf_decode = NULL;
    id->p_decoder->p_module = module_need_var( id->p_decoder, "audio decoder", "codec" );
    if( !id->p_decoder->p_module )
    {
        msg_Err( p_stream, "cannot find audio decoder" );
        es_format_Clean( &id->decoder_out );
        return VLC_EGENERIC;
    }

    if( id->decoder_out.i_codec == 0 ) /* format_update can happen on open() */
    {
        es_format_Clean( &id->decoder_out );
        es_format_Copy( &id->decoder_out, &id->p_decoder->fmt_out );
    }

    vlc_mutex_lock(&id->fifo.lock);

    /* The decoder fmt_out can be uninitialized here (since it can initialized
     * asynchronously). Fix decoder_out with default values in that case.
     * This should be enough to initialize the encoder for the first time (it
     * will be reloaded when all informations from the decoder are available).
     * */
    id->decoder_out.audio.i_format = FIRSTVALID( id->decoder_out.audio.i_format,
                                                 id->p_decoder->fmt_out.i_codec,
                                                 VLC_CODEC_FL32 );
    id->decoder_out.audio.i_rate = FIRSTVALID( id->decoder_out.audio.i_rate,
                                               id->p_decoder->fmt_in.audio.i_rate,
                                               48000 );
    id->decoder_out.audio.i_physical_channels =
            FIRSTVALID( id->decoder_out.audio.i_physical_channels,
                        id->p_decoder->fmt_in.audio.i_physical_channels,
                        AOUT_CHANS_STEREO );
    aout_FormatPrepare( &id->decoder_out.audio );

    /* Should be the same format until encoder loads */
    es_format_t encoder_tested_fmt_in;
    es_format_Init( &encoder_tested_fmt_in, id->decoder_out.i_cat, 0 );

    /* The decoder fmt_out can be uninitialized here (since it can initialized
     * asynchronously). Fix audio_dec_out with default values in that case.
     * This should be enough to initialize the encoder for the first time (it
     * will be reloaded when all informations from the decoder are available).
     * */
    if( transcode_encoder_test( sout_EncoderCreate(p_stream, sizeof(encoder_t)),
                                id->p_enccfg,
                                &id->decoder_out,
                                id->p_decoder->fmt_out.i_codec,
                                &encoder_tested_fmt_in ) )
    {
        vlc_mutex_unlock(&id->fifo.lock);
        module_unneed( id->p_decoder, id->p_decoder->p_module );
        id->p_decoder->p_module = NULL;
        es_format_Clean( &id->decoder_out );
        es_format_Clean( &encoder_tested_fmt_in );
        return VLC_EGENERIC;
    }

    es_format_SetMeta( &encoder_tested_fmt_in, p_fmt );

    vlc_mutex_unlock(&id->fifo.lock);

    id->encoder = transcode_encoder_new( sout_EncoderCreate(p_stream, sizeof(encoder_t)), &encoder_tested_fmt_in );
    if( !id->encoder )
    {
        module_unneed( id->p_decoder, id->p_decoder->p_module );
        id->p_decoder->p_module = NULL;
        es_format_Clean( &id->decoder_out );
        es_format_Clean( &encoder_tested_fmt_in );
        return VLC_EGENERIC;
    }

    es_format_Clean( &encoder_tested_fmt_in );

    return VLC_SUCCESS;
}

void transcode_audio_clean( sout_stream_t *p_stream, sout_stream_id_sys_t *id )
{
    /* Close encoder */
    transcode_encoder_close( id->encoder );
    transcode_encoder_delete( id->encoder );

    es_format_Clean( &id->decoder_out );

    /* Close filters */
    if( id->p_af_chain != NULL )
        aout_FiltersDelete( p_stream, id->p_af_chain );
}

static bool transcode_audio_format_IsSimilar( const audio_format_t *a,
                                              const audio_format_t *b )
{
    return
    a->i_rate == b->i_rate &&
    a->i_physical_channels == b->i_physical_channels;
}

int transcode_audio_process( sout_stream_t *p_stream,
                                    sout_stream_id_sys_t *id,
                                    block_t *in, block_t **out )
{
    *out = NULL;

    int ret = id->p_decoder->pf_decode( id->p_decoder, in );
    if( ret != VLCDEC_SUCCESS )
        return VLC_EGENERIC;

    block_t *p_audio_bufs = transcode_dequeue_all_audios( id );

    do
    {
        block_t *p_audio_buf = p_audio_bufs;
        if( p_audio_buf == NULL )
            break;
        p_audio_bufs = p_audio_buf->p_next;
        p_audio_buf->p_next = NULL;

        if( id->b_error )
        {
            block_Release( p_audio_buf );
            continue;
        }

        vlc_mutex_lock(&id->fifo.lock);

        if( p_audio_buf && ( unlikely(!transcode_encoder_opened( id->encoder )) ||
            !transcode_audio_format_IsSimilar( &id->fmt_input_audio, &id->decoder_out.audio ) ) )
        {
            if( !transcode_encoder_opened( id->encoder ) )
            {
                transcode_encoder_audio_configure( id->p_enccfg,
                                                   &id->decoder_out.audio, id->encoder, true );
                id->fmt_input_audio = id->decoder_out.audio;
            }
            else
            {
                /* Check if audio format has changed, and filters need reinit */
                msg_Info( p_stream, "Audio changed, trying to reinitialize filters" );
                if( id->p_af_chain != NULL )
                {
                    aout_FiltersDelete( p_stream, id->p_af_chain );
                    id->p_af_chain = NULL;
                }
                id->fmt_input_audio = id->decoder_out.audio;
            }

            if( !id->p_af_chain &&
                transcode_audio_filters_init( p_stream,
                                              id->p_filterscfg,
                                              &id->decoder_out.audio,
                                              &transcode_encoder_format_in( id->encoder )->audio,
                                              &id->p_af_chain ) )
            {
                vlc_mutex_unlock(&id->fifo.lock);
                goto error;
            }

            date_Init( &id->next_input_pts, id->decoder_out.audio.i_rate, 1 );
            date_Set( &id->next_input_pts, p_audio_buf->i_pts );

            if( !transcode_encoder_opened( id->encoder ) &&
                transcode_encoder_open( id->encoder, id->p_enccfg ) )
            {
                vlc_mutex_unlock(&id->fifo.lock);
                goto error;
            }

            if( !id->downstream_id )
                id->downstream_id =
                    id->pf_transcode_downstream_add( p_stream,
                                                     &id->p_decoder->fmt_in,
                                                     transcode_encoder_format_out( id->encoder ) );
            if( !id->downstream_id )
            {
                msg_Err( p_stream, "cannot output transcoded stream %4.4s",
                                   (char *) &id->p_enccfg->i_codec );
                vlc_mutex_unlock(&id->fifo.lock);
                goto error;
            }
        }

        vlc_mutex_unlock(&id->fifo.lock);

        if( id->pf_drift_validate )
        {
            vlc_tick_t i_pts = date_Get( &id->next_input_pts );
            vlc_tick_t i_drift = 0;

            if( likely( p_audio_buf->i_pts != VLC_TICK_INVALID ) )
                i_drift = p_audio_buf->i_pts - i_pts;
            if( id->pf_drift_validate( id->callback_data, i_drift ) != VLC_SUCCESS )
            {
                date_Set( &id->next_input_pts, p_audio_buf->i_pts );
                i_drift = 0;
            }

            vlc_mutex_lock(&id->fifo.lock);
            id->i_drift = i_drift;
            vlc_mutex_unlock(&id->fifo.lock);
            date_Increment( &id->next_input_pts, p_audio_buf->i_nb_samples );
        }

        p_audio_buf->i_dts = p_audio_buf->i_pts;

        /* Run filter chain */
        p_audio_buf = aout_FiltersPlay( id->p_af_chain, p_audio_buf, 1.f );
        if( p_audio_buf  )
        {
            p_audio_buf->i_dts = p_audio_buf->i_pts;

            block_t *p_block = transcode_encoder_encode( id->encoder, p_audio_buf );
            block_ChainAppend( out, p_block );
            block_Release( p_audio_buf );
        }
        continue;
error:
        if( p_audio_buf )
            block_Release( p_audio_buf );
        id->b_error = true;
    } while( p_audio_bufs );

    /* Drain encoder */
    if( unlikely( !id->b_error && in == NULL ) && transcode_encoder_opened( id->encoder ) )
    {
        transcode_encoder_drain( id->encoder, out );
    }

    return id->b_error ? VLC_EGENERIC : VLC_SUCCESS;
}
