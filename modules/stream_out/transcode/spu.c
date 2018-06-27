/*****************************************************************************
 * spu.c: transcoding stream output module (spu)
 *****************************************************************************
 * Copyright (C) 2003-2009 VLC authors and VideoLAN
 * $Id$
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

static int transcode_spu_new( sout_stream_t *p_stream, sout_stream_id_sys_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    /*
     * Open decoder
     */
    dec_get_owner( id->p_decoder )->id = id;

    static const struct decoder_owner_callbacks dec_cbs =
    {
        .spu = {
            spu_new_buffer,
            decoder_queue_sub,
        },
    };
    id->p_decoder->cbs = &dec_cbs;

    id->p_decoder->pf_decode = NULL;
    /* id->p_decoder->p_cfg = p_sys->p_spu_cfg; */

    id->p_decoder->p_module =
        module_need_var( id->p_decoder, "spu decoder", "codec" );

    if( !id->p_decoder->p_module )
    {
        msg_Err( p_stream, "cannot find spu decoder" );
        return VLC_EGENERIC;
    }

    if( !p_sys->b_soverlay )
    {
        /* Open encoder */
        /* Initialization of encoder format structures */
        es_format_Init( &id->p_encoder->fmt_in, id->p_decoder->fmt_in.i_cat,
                        id->p_decoder->fmt_in.i_codec );

        id->p_encoder->p_cfg = p_sys->p_spu_cfg;

        id->p_encoder->p_module =
            module_need( id->p_encoder, "encoder", p_sys->psz_senc, true );

        if( !id->p_encoder->p_module )
        {
            module_unneed( id->p_decoder, id->p_decoder->p_module );
            msg_Err( p_stream, "cannot find spu encoder (%s)", p_sys->psz_senc );
            return VLC_EGENERIC;
        }
    }

    if( !p_sys->p_spu )
        p_sys->p_spu = spu_Create( p_stream, NULL );

    return VLC_SUCCESS;
}

void transcode_spu_close( sout_stream_t *p_stream, sout_stream_id_sys_t *id)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    /* Close decoder */
    if( id->p_decoder->p_module )
        module_unneed( id->p_decoder, id->p_decoder->p_module );
    if( id->p_decoder->p_description )
        vlc_meta_Delete( id->p_decoder->p_description );

    /* Close encoder */
    if( id->p_encoder->p_module )
        module_unneed( id->p_encoder, id->p_encoder->p_module );

    if( p_sys->p_spu )
    {
        spu_Destroy( p_sys->p_spu );
        p_sys->p_spu = NULL;
    }
}

int transcode_spu_process( sout_stream_t *p_stream,
                                  sout_stream_id_sys_t *id,
                                  block_t *in, block_t **out )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
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

        if( p_sys->b_master_sync && p_sys->i_master_drift )
        {
            p_subpic->i_start -= p_sys->i_master_drift;
            if( p_subpic->i_stop ) p_subpic->i_stop -= p_sys->i_master_drift;
        }

        if( p_sys->b_soverlay )
            spu_PutSubpicture( p_sys->p_spu, p_subpic );
        else
        {
            block_t *p_block;

            es_format_t fmt;
            es_format_Init( &fmt, VIDEO_ES, VLC_CODEC_TEXT );

            fmt.video.i_sar_num =
            fmt.video.i_visible_width =
            fmt.video.i_width = p_sys->i_spu_width;

            fmt.video.i_sar_den =
            fmt.video.i_visible_height =
            fmt.video.i_height = p_sys->i_spu_height;

            subpicture_Update( p_subpic, &fmt.video, &fmt.video, p_subpic->i_start );
            es_format_Clean( &fmt );

            p_block = id->p_encoder->pf_encode_sub( id->p_encoder, p_subpic );
            subpicture_Delete( p_subpic );
            if( p_block )
                block_ChainAppend( out, p_block );
            else
                b_error = true;
        }
    } while( p_subpics );

    return b_error ? VLC_EGENERIC : VLC_SUCCESS;
}

bool transcode_spu_add( sout_stream_t *p_stream, const es_format_t *p_fmt,
                        sout_stream_id_sys_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    id->fifo.spu.first = NULL;
    id->fifo.spu.last = &id->fifo.spu.first;

    if( p_sys->i_scodec )
    {
        msg_Dbg( p_stream, "creating subtitle transcoding from fcc=`%4.4s' "
                 "to fcc=`%4.4s'", (char*)&p_fmt->i_codec,
                 (char*)&p_sys->i_scodec );

        /* Complete destination format */
        id->p_encoder->fmt_out.i_codec = p_sys->i_scodec;

        /* build decoder -> filter -> encoder */
        if( transcode_spu_new( p_stream, id ) )
        {
            msg_Err( p_stream, "cannot create subtitle chain" );
            return false;
        }

        /* open output stream */
        id->downstream_id = sout_StreamIdAdd( p_stream->p_next, &id->p_encoder->fmt_out );
        id->b_transcode = true;

        if( !id->downstream_id )
        {
            transcode_spu_close( p_stream, id );
            return false;
        }
    }
    else
    {
        assert( p_sys->b_soverlay );
        msg_Dbg( p_stream, "subtitle (fcc=`%4.4s') overlaying",
                 (char*)&p_fmt->i_codec );

        id->b_transcode = true;

        /* Build decoder -> filter -> overlaying chain */
        if( transcode_spu_new( p_stream, id ) )
        {
            msg_Err( p_stream, "cannot create subtitle chain" );
            return false;
        }
    }

    return true;
}
