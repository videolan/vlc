/*****************************************************************************
 * osd.c: transcoding stream output module (osd)
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

#include <vlc_spu.h>
#include <vlc_modules.h>

/*
 * OSD menu
 */
int transcode_osd_new( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    id->p_decoder->fmt_in.i_cat = SPU_ES;
    id->p_encoder->fmt_out.psz_language = strdup( "osd" );

    if( p_sys->i_osdcodec != 0 || p_sys->psz_osdenc )
    {
        msg_Dbg( p_stream, "creating osdmenu transcoding from fcc=`%4.4s' "
                 "to fcc=`%4.4s'", (char*)&id->p_encoder->fmt_out.i_codec,
                 (char*)&p_sys->i_osdcodec );

        /* Complete destination format */
        id->p_encoder->fmt_out.i_codec = p_sys->i_osdcodec;

        /* Open encoder */
        es_format_Init( &id->p_encoder->fmt_in, id->p_decoder->fmt_in.i_cat,
                        VLC_CODEC_YUVA );
        id->p_encoder->fmt_in.psz_language = strdup( "osd" );

        id->p_encoder->p_cfg = p_sys->p_osd_cfg;

        id->p_encoder->p_module =
            module_need( id->p_encoder, "encoder", p_sys->psz_osdenc, true );

        if( !id->p_encoder->p_module )
        {
            msg_Err( p_stream, "cannot find spu encoder (%s)", p_sys->psz_osdenc );
            goto error;
        }

        /* open output stream */
        id->id = sout_StreamIdAdd( p_stream->p_next, &id->p_encoder->fmt_out );
        id->b_transcode = true;

        if( !id->id ) goto error;
    }
    else
    {
        msg_Dbg( p_stream, "not transcoding a stream (fcc=`%4.4s')",
                 (char*)&id->p_decoder->fmt_out.i_codec );
        id->id = sout_StreamIdAdd( p_stream->p_next, &id->p_decoder->fmt_out );
        id->b_transcode = false;

        if( !id->id ) goto error;
    }

    if( !p_sys->p_spu )
        p_sys->p_spu = spu_Create( p_stream );

    return VLC_SUCCESS;

 error:
    msg_Err( p_stream, "starting osd encoding thread failed" );
    if( id->p_encoder->p_module )
            module_unneed( id->p_encoder, id->p_encoder->p_module );
    p_sys->b_osd = false;
    return VLC_EGENERIC;
}

void transcode_osd_close( sout_stream_t *p_stream, sout_stream_id_t *id)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    /* Close encoder */
    if( id )
    {
        if( id->p_encoder->p_module )
            module_unneed( id->p_encoder, id->p_encoder->p_module );
    }
    p_sys->b_osd = false;
}

int transcode_osd_process( sout_stream_t *p_stream, sout_stream_id_t *id,
                                  block_t *in, block_t **out )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    subpicture_t *p_subpic = NULL;

    /* Check if we have a subpicture to send */
    if( p_sys->p_spu && in->i_dts > VLC_TS_INVALID )
    {
        video_format_t fmt;
        video_format_Init( &fmt, 0 );
        video_format_Setup( &fmt, 0, 720, 576, 1, 1 );
        p_subpic = spu_Render( p_sys->p_spu, NULL, &fmt, &fmt, in->i_dts, in->i_dts, false );
    }
    else
    {
        msg_Warn( p_stream, "spu channel not initialized, doing it now" );
        if( !p_sys->p_spu )
            p_sys->p_spu = spu_Create( p_stream );
    }

    if( p_subpic )
    {
        block_t *p_block = NULL;

        if( p_sys->b_master_sync && p_sys->i_master_drift )
        {
            p_subpic->i_start -= p_sys->i_master_drift;
            if( p_subpic->i_stop ) p_subpic->i_stop -= p_sys->i_master_drift;
        }

        p_block = id->p_encoder->pf_encode_sub( id->p_encoder, p_subpic );
        subpicture_Delete( p_subpic );
        if( p_block )
        {
            p_block->i_dts = p_block->i_pts = in->i_dts;
            block_ChainAppend( out, p_block );
            return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}

bool transcode_osd_add( sout_stream_t *p_stream, es_format_t *p_fmt,
                                sout_stream_id_t *id)
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    msg_Dbg( p_stream, "creating osd transcoding from fcc=`%4.4s' "
             "to fcc=`%4.4s'", (char*)&p_fmt->i_codec,
             (char*)&p_sys->i_scodec );

    id->b_transcode = true;

    /* Create a fake OSD menu elementary stream */
    if( transcode_osd_new( p_stream, id ) )
    {
        msg_Err( p_stream, "cannot create osd chain" );
        return false;
    }
    p_sys->b_osd = true;

    return true;
}
