/*****************************************************************************
 * spu.c: transcoding stream output module (spu)
 *****************************************************************************
 * Copyright (C) 2003-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
 *          Antoine Cellerier <dionoea at videolan dot org>
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

#include "transcode.h"

#include <vlc_meta.h>
#include <vlc_spu.h>
#include <vlc_modules.h>
#include <assert.h>

static subpicture_t *spu_new_buffer( decoder_t *p_dec,
                                     const subpicture_updater_t *p_upd )
{
    VLC_UNUSED( p_dec );
    subpicture_t *p_subpicture = subpicture_New( p_upd );
    p_subpicture->b_subtitle = true;
    return p_subpicture;
}

static void spu_del_buffer( decoder_t *p_dec, subpicture_t *p_subpic )
{
    VLC_UNUSED( p_dec );
    subpicture_Delete( p_subpic );
}
int transcode_spu_new( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    /*
     * Open decoder
     */

    /* Initialization of decoder structures */
    id->p_decoder->pf_decode_sub = NULL;
    id->p_decoder->pf_spu_buffer_new = spu_new_buffer;
    id->p_decoder->pf_spu_buffer_del = spu_del_buffer;
    id->p_decoder->p_owner = (decoder_owner_sys_t *)p_stream;
    /* id->p_decoder->p_cfg = p_sys->p_spu_cfg; */

    id->p_decoder->p_module =
        module_need( id->p_decoder, "decoder", "$codec", false );

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
        p_sys->p_spu = spu_Create( p_stream );

    return VLC_SUCCESS;
}

void transcode_spu_close( sout_stream_t *p_stream, sout_stream_id_t *id)
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
                                  sout_stream_id_t *id,
                                  block_t *in, block_t **out )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    subpicture_t *p_subpic;
    *out = NULL;

    p_subpic = id->p_decoder->pf_decode_sub( id->p_decoder, &in );
    if( !p_subpic )
        return VLC_EGENERIC;

    if( p_sys->b_master_sync && p_sys->i_master_drift )
    {
        p_subpic->i_start -= p_sys->i_master_drift;
        if( p_subpic->i_stop ) p_subpic->i_stop -= p_sys->i_master_drift;
    }

    if( p_sys->b_soverlay )
    {
        spu_PutSubpicture( p_sys->p_spu, p_subpic );
    }
    else
    {
        block_t *p_block;

        p_block = id->p_encoder->pf_encode_sub( id->p_encoder, p_subpic );
        spu_del_buffer( id->p_decoder, p_subpic );
        if( p_block )
        {
            block_ChainAppend( out, p_block );
            return VLC_SUCCESS;
        }
    }

    return VLC_EGENERIC;
}

bool transcode_spu_add( sout_stream_t *p_stream, es_format_t *p_fmt,
                        sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    if( p_sys->i_scodec || p_sys->psz_senc )
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
        id->id = sout_StreamIdAdd( p_stream->p_next, &id->p_encoder->fmt_out );
        id->b_transcode = true;

        if( !id->id )
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
