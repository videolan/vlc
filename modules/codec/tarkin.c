/*****************************************************************************
 * tarkin.c: tarkin decoder module making use of libtarkin.
 *****************************************************************************
 * Copyright (C) 2001-2003 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
#include <vlc_codec.h>
#include <vlc_vout.h>
#include <ogg/ogg.h>

/* FIXME */
// use 16 bit signed integers as wavelet coefficients
#define TYPE int16_t
// we'll actually use TYPE_BITS bits of type (e.g. 9 magnitude + 1 sign)
#define TYPE_BITS 10
// use the rle entropy coder
#define RLECODER 1

#include <tarkin.h>

/*****************************************************************************
 * decoder_sys_t : tarkin decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /*
     * Tarkin properties
     */
    TarkinStream *tarkin_stream;

    TarkinInfo       ti;                        /* tarkin bitstream settings */
    TarkinComment    tc;                   /* tarkin bitstream user comments */
    TarkinTime       tarkdate;

    int i_headers;
    mtime_t i_pts;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder  ( vlc_object_t * );
static void CloseDecoder ( vlc_object_t * );

static void *DecodeBlock ( decoder_t *, block_t ** );
static picture_t *DecodePacket ( decoder_t *, block_t **, ogg_packet * );

static void tarkin_CopyPicture( decoder_t *, picture_t *, uint8_t *, int );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( N_("Tarkin decoder module") );
    set_capability( "decoder", 100 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_VCODEC );
    set_callbacks( OpenDecoder, CloseDecoder );
    add_shortcut( "tarkin" );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('t','a','r','k') )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;

    /* Set output properties */
    p_dec->fmt_out.i_cat = VIDEO_ES;
    p_sys->i_headers = 0;

    /* Set callbacks */
    p_dec->pf_decode_video = (picture_t *(*)(decoder_t *, block_t **))
        DecodeBlock;
    p_dec->pf_packetize    = (block_t *(*)(decoder_t *, block_t **))
        DecodeBlock;

    /* Init supporting Tarkin structures needed in header parsing */
    p_sys->tarkin_stream = tarkin_stream_new();
    tarkin_info_init( &p_sys->ti );
    tarkin_comment_init( &p_sys->tc );

    return VLC_SUCCESS;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with ogg packets.
 ****************************************************************************/
static void *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;
    ogg_packet oggpacket;

    if( !pp_block ) return NULL;

    if( *pp_block )
    {
        /* Block to Ogg packet */
        oggpacket.packet = (*pp_block)->p_buffer;
        oggpacket.bytes = (*pp_block)->i_buffer;
    }
    else
    {
        /* Block to Ogg packet */
        oggpacket.packet = NULL;
        oggpacket.bytes = 0;
    }

    p_block = *pp_block;

    oggpacket.granulepos = -1;
    oggpacket.b_o_s = 0;
    oggpacket.e_o_s = 0;
    oggpacket.packetno = 0;

    if( p_sys->i_headers == 0 )
    {
        /* Take care of the initial Tarkin header */

        oggpacket.b_o_s = 1; /* yes this actually is a b_o_s packet :) */
        if( tarkin_synthesis_headerin( &p_sys->ti, &p_sys->tc, &oggpacket )
            < 0 )
        {
            msg_Err( p_dec, "this bitstream does not contain Tarkin "
                     "video data.");
            block_Release( p_block );
            return NULL;
        }
        p_sys->i_headers++;

        block_Release( p_block );
        return NULL;
    }

    if( p_sys->i_headers == 1 )
    {
        if( tarkin_synthesis_headerin( &p_sys->ti, &p_sys->tc, &oggpacket )
            < 0 )
        {
            msg_Err( p_dec, "2nd Tarkin header is corrupted." );
            block_Release( p_block );
            return NULL;
        }
        p_sys->i_headers++;
        block_Release( p_block );
        return NULL;
    }

    if( p_sys->i_headers == 2 )
    {
        if( tarkin_synthesis_headerin( &p_sys->ti, &p_sys->tc, &oggpacket )
            < 0 )
        {
            msg_Err( p_dec, "3rd Tarkin header is corrupted." );
            block_Release( p_block );
            return NULL;
        }
        p_sys->i_headers++;

        /* Initialize the tarkin decoder */
        tarkin_synthesis_init( p_sys->tarkin_stream, &p_sys->ti );

        msg_Err( p_dec, "Tarkin codec initialized");

        block_Release( p_block );
        return NULL;
    }

    return DecodePacket( p_dec, pp_block, &oggpacket );
}

/*****************************************************************************
 * DecodePacket: decodes a Tarkin packet.
 *****************************************************************************/
static picture_t *DecodePacket( decoder_t *p_dec, block_t **pp_block,
                                ogg_packet *p_oggpacket )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t *rgb;

    if( p_oggpacket->bytes )
    {
        tarkin_synthesis_packetin( p_sys->tarkin_stream, p_oggpacket );
        //block_Release( *pp_block ); /* FIXME duplicate packet */
        *pp_block = NULL;
    }

    if( tarkin_synthesis_frameout( p_sys->tarkin_stream,
                                   &rgb, 0, &p_sys->tarkdate ) == 0 )
    {
        int i_width, i_height, i_chroma, i_stride;
        picture_t *p_pic;

        msg_Err( p_dec, "Tarkin frame decoded" );

        i_width = p_sys->tarkin_stream->layer->desc.width;
        i_height = p_sys->tarkin_stream->layer->desc.height;

        switch( p_sys->tarkin_stream->layer->desc.format )
        {
        case TARKIN_RGB24:
            i_chroma = VLC_FOURCC('R','V','2','4');
            i_stride = i_width * 3;
            break;
        case TARKIN_RGB32:
            i_chroma = VLC_FOURCC('R','V','3','2');
            i_stride = i_width * 4;
            break;
        case TARKIN_RGBA:
            i_chroma = VLC_FOURCC('R','G','B','A');
            i_stride = i_width * 4;
            break;
        default:
            i_chroma = VLC_FOURCC('I','4','2','0');
            i_stride = i_width;
            break;
        }

        /* Set output properties */
        p_dec->fmt_out.video.i_width = i_width;
        p_dec->fmt_out.video.i_height = i_height;

        p_dec->fmt_out.video.i_aspect =
            VOUT_ASPECT_FACTOR * i_width / i_height;
        p_dec->fmt_out.i_codec = i_chroma;

        /* Get a new picture */
        if( (p_pic = p_dec->pf_vout_buffer_new( p_dec )) )
        {
            tarkin_CopyPicture( p_dec, p_pic, rgb, i_stride );

            tarkin_synthesis_freeframe( p_sys->tarkin_stream, rgb );

            p_pic->date = mdate() + DEFAULT_PTS_DELAY/*i_pts*/;

            return p_pic;
        }
    }

    return NULL;
}

/*****************************************************************************
 * CloseDecoder: tarkin decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    tarkin_stream_destroy( p_sys->tarkin_stream );

    free( p_sys );
}

/*****************************************************************************
 * tarkin_CopyPicture: copy a picture from tarkin internal buffers to a
 *                     picture_t structure.
 *****************************************************************************/
static void tarkin_CopyPicture( decoder_t *p_dec, picture_t *p_pic,
                                uint8_t *p_src, int i_pitch )
{
    int i_plane, i_line, i_src_stride, i_dst_stride;
    uint8_t *p_dst;

    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        p_dst = p_pic->p[i_plane].p_pixels;
        i_dst_stride = p_pic->p[i_plane].i_pitch;
        i_src_stride = i_pitch;

        for( i_line = 0; i_line < p_pic->p[i_plane].i_visible_lines; i_line++ )
        {
            vlc_memcpy( p_dst, p_src, i_src_stride );

            p_src += i_src_stride;
            p_dst += i_dst_stride;
        }
    }
}
