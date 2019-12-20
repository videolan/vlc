/*****************************************************************************
 * spu.c: transcoding stream output module (spu)
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
#include <vlc_meta.h>
#include <vlc_spu.h>
#include <vlc_modules.h>
#include <vlc_sout.h>

#include "transcode.h"

#include <assert.h>

static subpicture_t *spu_new_buffer( decoder_t *p_dec,
                                     const subpicture_updater_t *p_upd )
{
    VLC_UNUSED( p_dec );
    subpicture_t *p_subpicture = subpicture_New( p_upd );
    if( likely(p_subpicture != NULL) )
        p_subpicture->b_subtitle = true;
    return p_subpicture;
}

static void decoder_queue_sub( decoder_t *p_dec, subpicture_t *p_spu )
{
    struct decoder_owner *p_owner = dec_get_owner( p_dec );
    sout_stream_id_sys_t *id = p_owner->id;

    vlc_mutex_lock(&id->fifo.lock);
    *id->fifo.spu.last = p_spu;
    id->fifo.spu.last = &p_spu->p_next;
    vlc_mutex_unlock(&id->fifo.lock);
}

static subpicture_t *transcode_dequeue_all_subs( sout_stream_id_sys_t *id )
{
    vlc_mutex_lock(&id->fifo.lock);
    subpicture_t *p_subpics = id->fifo.spu.first;
    id->fifo.spu.first = NULL;
    id->fifo.spu.last = &id->fifo.spu.first;
    vlc_mutex_unlock(&id->fifo.lock);

    return p_subpics;
}

int transcode_spu_init( sout_stream_t *p_stream, const es_format_t *p_fmt,
                        sout_stream_id_sys_t *id )
{
    if( id->p_enccfg->i_codec )
        msg_Dbg( p_stream, "creating subtitle transcoding from fcc=`%4.4s' "
                 "to fcc=`%4.4s'", (char*)&p_fmt->i_codec,
                 (char*)&id->p_enccfg->i_codec );
    else
        msg_Dbg( p_stream, "subtitle (fcc=`%4.4s') overlaying",
                 (char*)&p_fmt->i_codec );

    id->fifo.spu.first = NULL;
    id->fifo.spu.last = &id->fifo.spu.first;
    id->b_transcode = true;

    /*
     * Open decoder
     */
    dec_get_owner( id->p_decoder )->id = id;

    static const struct decoder_owner_callbacks dec_cbs =
    {
        .spu = {
            .buffer_new = spu_new_buffer,
            .queue = decoder_queue_sub,
        },
    };
    id->p_decoder->cbs = &dec_cbs;
    id->p_decoder->pf_decode = NULL;
    id->p_decoder->p_module =
        module_need_var( id->p_decoder, "spu decoder", "codec" );

    if( !id->p_decoder->p_module )
    {
        msg_Err( p_stream, "cannot find spu decoder" );
        return VLC_EGENERIC;
    }

    if( id->p_enccfg->i_codec /* !overlay */ )
    {
        /* Open encoder */
        /* Initialization of encoder format structures */
        assert(!id->encoder);
        id->encoder = transcode_encoder_new( sout_EncoderCreate(p_stream, sizeof(encoder_t)), &id->p_decoder->fmt_in );
        if( !id->encoder )
        {
            module_unneed( id->p_decoder, id->p_decoder->p_module );
            id->p_decoder->p_module = NULL;
            return VLC_EGENERIC;
        }

        if( transcode_encoder_open( id->encoder, id->p_enccfg ) )
        {
            msg_Err( p_stream, "cannot find spu encoder (%s)", id->p_enccfg->psz_name );
            transcode_encoder_delete( id->encoder );
            module_unneed( id->p_decoder, id->p_decoder->p_module );
            id->p_decoder->p_module = NULL;
            return VLC_EGENERIC;
        }

        /* open output stream */
        id->downstream_id =
                id->pf_transcode_downstream_add( p_stream,
                                                 &id->p_decoder->fmt_in,
                                                 transcode_encoder_format_out( id->encoder ) );
        if( !id->downstream_id )
        {
            msg_Err( p_stream, "cannot output transcoded stream %4.4s",
                               (char *) &id->p_enccfg->i_codec );
            transcode_encoder_close( id->encoder );
            transcode_encoder_delete( id->encoder );
            module_unneed( id->p_decoder, id->p_decoder->p_module );
            id->p_decoder->p_module = NULL;
            return VLC_EGENERIC;
        }
    }
    else
    {
        assert( id->p_enccfg->i_codec == 0 /* !overlay */ );
    }

    return VLC_SUCCESS;
}

void transcode_spu_clean( sout_stream_t *p_stream, sout_stream_id_sys_t *id)
{
    VLC_UNUSED(p_stream);

    /* Close encoder */
    if( id->encoder )
    {
        transcode_encoder_close( id->encoder );
        transcode_encoder_delete( id->encoder );
    }
}

int transcode_spu_process( sout_stream_t *p_stream,
                                  sout_stream_id_sys_t *id,
                                  block_t *in, block_t **out )
{
    VLC_UNUSED(p_stream);
    *out = NULL;
    bool b_error = false;

    int ret = id->p_decoder->pf_decode( id->p_decoder, in );
    if( ret != VLCDEC_SUCCESS )
        return VLC_EGENERIC;

    subpicture_t *p_subpics = transcode_dequeue_all_subs( id );

    do
    {
        subpicture_t *p_subpic = p_subpics;
        if( p_subpic == NULL )
            break;
        p_subpics = p_subpic->p_next;
        p_subpic->p_next = NULL;

        if( b_error )
        {
            subpicture_Delete( p_subpic );
            continue;
        }

        vlc_tick_t drift;
        if( id->pf_get_master_drift &&
            (drift = id->pf_get_master_drift( id->callback_data )) )
        {
            p_subpic->i_start -= drift;
            if( p_subpic->i_stop )
                p_subpic->i_stop -= drift;
        }

        if( id->p_enccfg->i_codec == 0 /* overlay */ )
        {
            if( !id->pf_send_subpicture )
            {
                subpicture_Delete( p_subpic );
                b_error = true;
            }
            else id->pf_send_subpicture( id->callback_data, p_subpic );
        }
        else
        {
            block_t *p_block;

            es_format_t fmt;
            es_format_Init( &fmt, VIDEO_ES, VLC_CODEC_TEXT );

            unsigned w, h;
            if( id->pf_get_output_dimensions == NULL ||
                id->pf_get_output_dimensions( id->callback_data,
                                              &w, &h ) != VLC_SUCCESS )
            {
                w = id->p_enccfg->spu.i_width;
                h = id->p_enccfg->spu.i_height;
            }

            fmt.video.i_sar_num =
            fmt.video.i_visible_width =
            fmt.video.i_width = w;

            fmt.video.i_sar_den =
            fmt.video.i_visible_height =
            fmt.video.i_height = h;

            subpicture_Update( p_subpic, &fmt.video, &fmt.video, p_subpic->i_start );
            es_format_Clean( &fmt );

            p_block = transcode_encoder_encode( id->encoder, p_subpic );
            subpicture_Delete( p_subpic );
            if( p_block )
                block_ChainAppend( out, p_block );
            else
                b_error = true;
        }
    } while( p_subpics );

    return b_error ? VLC_EGENERIC : VLC_SUCCESS;
}
