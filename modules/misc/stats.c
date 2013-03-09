/*****************************************************************************
 * stats.c : stats plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2008 the VideoLAN team
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Pierre d'Herbemont <pdherbemont@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* Example usage:
 *  $ vlc movie.avi --sout="#transcode{aenc=dummy,venc=stats}:\
 *                          std{access=http,mux=dummy,dst=0.0.0.0:8081}"
 *  $ vlc -vvv http://127.0.0.1:8081 --demux=stats --vout=stats --codec=stats
 */

#define kBufferSize 0x500

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_demux.h>

/*** Decoder ***/
static picture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    block_t *p_block;
    picture_t * p_pic = NULL;

    if( !pp_block || !*pp_block ) return NULL;
    p_block = *pp_block;

    p_pic = decoder_NewPicture( p_dec );

    if( p_block->i_buffer == kBufferSize )
    {
        msg_Dbg( p_dec, "got %"PRIu64" ms",
                 *(mtime_t *)p_block->p_buffer  / 1000 );
        msg_Dbg( p_dec, "got %"PRIu64" ms offset",
                 (mdate() - *(mtime_t *)p_block->p_buffer) / 1000 );
        *(mtime_t *)(p_pic->p->p_pixels) = *(mtime_t *)p_block->p_buffer;
    }
    else
    {
        msg_Dbg( p_dec, "got a packet not from stats demuxer" );
        *(mtime_t *)(p_pic->p->p_pixels) = mdate();
    }

    p_pic->date = p_block->i_pts > VLC_TS_INVALID ?
            p_block->i_pts : p_block->i_dts;
    p_pic->b_force = true;

    block_Release( p_block );
    *pp_block = NULL;
    return p_pic;
}

static int OpenDecoder ( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    msg_Dbg( p_this, "opening stats decoder" );

    /* Set callbacks */
    p_dec->pf_decode_video = DecodeBlock;
    p_dec->pf_decode_audio = NULL;
    p_dec->pf_decode_sub = NULL;

    /* */
    es_format_Init( &p_dec->fmt_out, VIDEO_ES, VLC_CODEC_I420 );
    p_dec->fmt_out.video.i_width = 100;
    p_dec->fmt_out.video.i_height = 100;
    p_dec->fmt_out.video.i_sar_num = 1;
    p_dec->fmt_out.video.i_sar_den = 1;

    return VLC_SUCCESS;
}

/*** Encoder ***/
#ifdef ENABLE_SOUT
static block_t *EncodeVideo( encoder_t *p_enc, picture_t *p_pict )
{
    (void)p_pict;
    block_t * p_block = block_Alloc( kBufferSize );

    *(mtime_t*)p_block->p_buffer = mdate();
    p_block->i_buffer = kBufferSize;
    p_block->i_length = kBufferSize;
    p_block->i_dts = p_pict->date;

    msg_Dbg( p_enc, "putting %"PRIu64"ms",
             *(mtime_t*)p_block->p_buffer / 1000 );
    return p_block;
}

static block_t *EncodeAudio( encoder_t *p_enc, block_t *p_abuff )
{
    (void)p_abuff;
    (void)p_enc;
    return NULL;
}

static int OpenEncoder ( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;

    msg_Dbg( p_this, "opening stats encoder" );

    p_enc->pf_encode_video = EncodeVideo;
    p_enc->pf_encode_audio = EncodeAudio;

    return VLC_SUCCESS;
}
#endif

/*** Demuxer ***/
struct demux_sys_t
{
    es_format_t     fmt;
    es_out_id_t     *p_es;

    date_t          pts;
};

static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    block_t * p_block = stream_Block( p_demux->s, kBufferSize );

    if( !p_block ) return 1;

    p_block->i_dts = p_block->i_pts =
        date_Increment( &p_sys->pts, kBufferSize );

    msg_Dbg( p_demux, "demux got %d ms offset", (int)(mdate() - *(mtime_t *)p_block->p_buffer) / 1000 );

    //es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block->i_pts );

    es_out_Send( p_demux->out, p_sys->p_es, p_block );

    return 1;
}

static int DemuxControl( demux_t *p_demux, int i_query, va_list args )
{
    return demux_vaControlHelper( p_demux->s,
                                   0, 0, 0, 1,
                                   i_query, args );
}

static int OpenDemux ( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    p_demux->p_sys = NULL;

    /* Only when selected */
    if( *p_demux->psz_demux == '\0' )
        return VLC_EGENERIC;

    msg_Dbg( p_demux, "Init Stat demux" );

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = DemuxControl;

    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    if( !p_demux->p_sys )
        return VLC_ENOMEM;

    date_Init( &p_sys->pts, 1, 1 );
    date_Set( &p_sys->pts, 1 );

    es_format_Init( &p_sys->fmt, VIDEO_ES, VLC_FOURCC('s','t','a','t') );
    p_sys->fmt.video.i_width = 720;
    p_sys->fmt.video.i_height= 480;

    p_sys->p_es = es_out_Add( p_demux->out, &p_sys->fmt );

    return VLC_SUCCESS;
}

static void CloseDemux ( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;

    msg_Dbg( p_demux, "Closing Stat demux" );

    free( p_demux->p_sys );
}

vlc_module_begin ()
    set_shortname( N_("Stats"))
#ifdef ENABLE_SOUT
    set_description( N_("Stats encoder function") )
    set_capability( "encoder", 0 )
    add_shortcut( "stats" )
    set_callbacks( OpenEncoder, NULL )
    add_submodule ()
#endif
        set_section( N_( "Stats decoder" ), NULL )
        set_description( N_("Stats decoder function") )
        set_capability( "decoder", 0 )
        add_shortcut( "stats" )
        set_callbacks( OpenDecoder, NULL )
    add_submodule ()
        set_section( N_( "Stats demux" ), NULL )
        set_description( N_("Stats demux function") )
        set_capability( "demux", 0 )
        add_shortcut( "stats" )
        set_callbacks( OpenDemux, CloseDemux )
vlc_module_end ()
