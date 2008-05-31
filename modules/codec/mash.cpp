/*****************************************************************************
 * mash.c: Video decoder using openmash codec implementations
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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
#include <vlc_block.h>

#include <p64/p64.h>

/*****************************************************************************
 * decoder_sys_t : video decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /*
     * Common properties
     */
    mtime_t i_pts;
    IntraP64Decoder *p_decoder;
    bool b_inited;
    int i_counter;

};

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

static void *DecodeBlock  ( decoder_t *, block_t ** );

#if 0
static picture_t *DecodeFrame( decoder_t *, block_t * );
static block_t   *SendFrame  ( decoder_t *, block_t * );
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( N_("Video decoder using openmash") );
    set_capability( "decoder", 50 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_VCODEC );
    set_callbacks( OpenDecoder, CloseDecoder );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    switch( p_dec->fmt_in.i_codec )
    {
        /* Planar YUV */
        case VLC_FOURCC('h','2','6','1'):
        case VLC_FOURCC('H','2','6','1'):
            break;

        default:
            return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;
    /* Misc init */
    p_sys->i_pts = 0;
    p_sys->b_inited = false;
    p_sys->i_counter = 0;

    /* Set output properties */
    p_dec->fmt_out.i_cat = VIDEO_ES;
    p_dec->fmt_out.i_codec = VLC_FOURCC('I','4','2','0');

    /* Set callbacks */
    p_dec->pf_decode_video = (picture_t *(*)(decoder_t *, block_t **))
        DecodeBlock;
    p_sys->p_decoder = new IntraP64Decoder();
//     foo->sync();

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    free( p_dec->p_sys );
}


/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with complete frames.
 ****************************************************************************/
static void *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;
    picture_t *p_pic;
    uint32_t i_video_header;
    uint8_t *p_frame;
    int cc, sbit, ebit, mba, gob, quant, mvdh, mvdv;
    int i_width, i_height;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;

    if( !p_sys->i_pts && !p_block->i_pts && !p_block->i_dts )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( p_block );
        return NULL;
    }


    /* Date management */
    if( p_block->i_pts > 0 || p_block->i_dts > 0 )
    {
        if( p_block->i_pts > 0 ) p_sys->i_pts = p_block->i_pts;
        else if( p_block->i_dts > 0 ) p_sys->i_pts = p_block->i_dts;
    }

    i_video_header = *(uint32_t*)p_block->p_buffer; /* yes, it is native endian */
    sbit = i_video_header >> 29; /* start bit position */
    ebit = (i_video_header >> 26) & 7; /* end bit position */
    msg_Dbg( p_dec, "sbit, ebit: %d,%d", sbit, ebit );
    gob = (i_video_header >> 20) & 0xf; /* GOB number */
    if( gob > 12 )
    {
        msg_Warn( p_dec, "invalid gob, buggy vic streamer?");
    }
    mba = (i_video_header >> 15) & 0x1f; /* Macroblock address predictor */
    quant = (i_video_header >> 10) & 0x1f; /* quantizer */
    mvdh = (i_video_header >> 5) & 0x1f; /* horizontal motion vector data */
    mvdv = i_video_header & 0x1f; /* vertical motion vector data */
    cc = p_block->i_buffer - 4;
    msg_Dbg( p_dec, "packet size %d", cc );
 
    /* Find out p_vdec->i_raw_size */
    p_sys->p_decoder->decode( p_block->p_buffer + 4 /*bp?*/,
                              cc /*cc?*/,
                              sbit /*sbit?*/,
                              ebit /*ebit?*/,
                              mba /* mba?*/,
                              gob /* gob?*/,
                              quant /* quant?*/,
                              mvdh /* mvdh?*/,
                              mvdv /* mvdv?*/ );
    i_width = p_sys->p_decoder->width();
    i_height = p_sys->p_decoder->height();
    if( !p_sys->b_inited )
    {
        msg_Dbg( p_dec, "video size is perhaps %dx%d", i_width,
                  i_height);
        vout_InitFormat( &p_dec->fmt_out.video, VLC_FOURCC('I','4','2','0'),
                         i_width, i_height,
                         VOUT_ASPECT_FACTOR * i_width / i_height );
        p_sys->b_inited = true;
    }
    p_pic = NULL;
    p_sys->i_counter++;
//    p_sys->p_decoder->sync();
    if( p_block->i_flags & BLOCK_FLAG_END_OF_FRAME )
    {
        p_pic = p_dec->pf_vout_buffer_new( p_dec );
        if( !p_pic )
        {
            block_Release( p_block );
            return NULL;
        }
        p_sys->p_decoder->sync();
        p_sys->i_counter = 0;
        p_frame = p_sys->p_decoder->frame();
        vlc_memcpy( p_dec, p_pic->p[0].p_pixels, p_frame, i_width*i_height );
        p_frame += i_width * i_height;
        vlc_memcpy( p_dec, p_pic->p[1].p_pixels, p_frame, i_width*i_height/4 );
        p_frame += i_width * i_height/4;
        vlc_memcpy( p_dec, p_pic->p[2].p_pixels, p_frame, i_width*i_height/4 );
        p_pic->date = p_sys->i_pts;
    }
    block_Release( p_block);
    *pp_block = NULL;
    return p_pic;
//    return NULL;
}
