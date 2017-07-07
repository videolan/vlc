/*****************************************************************************
 * bpg.c: bpg decoder module using libbpg.
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
 *
 * Author: Tristan Matthews <tmatth@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <libbpg.h>

struct decoder_sys_t
{
    struct BPGDecoderContext *p_bpg;
};

static int  OpenDecoder(vlc_object_t *);
static void CloseDecoder(vlc_object_t *);

static int DecodeBlock(decoder_t *, block_t *);

/*
 * Module descriptor
 */
vlc_module_begin()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    /* decoder main module */
    set_description( N_("BPG image decoder") )
    set_capability( "video decoder", 60 )
    set_callbacks( OpenDecoder, CloseDecoder )
    add_shortcut( "bpg" )
vlc_module_end()


static int OpenDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_BPG )
    {
        return VLC_EGENERIC;
    }

    decoder_sys_t *p_sys = malloc( sizeof( decoder_sys_t ) );
    if( p_sys == NULL )
    {
        return VLC_ENOMEM;
    }

    p_dec->p_sys = p_sys;

    p_sys->p_bpg = bpg_decoder_open();
    if( !p_sys->p_bpg )
    {
        return VLC_EGENERIC;
    }

    /* Set callbacks */
    p_dec->pf_decode = DecodeBlock;

    return VLC_SUCCESS;
}

/*
 * This function must be fed with a complete compressed frame.
 */
static int DecodeBlock( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    picture_t *p_pic = 0;
    BPGImageInfo img_info;

    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
        goto error;

    /* Decode picture */

    if( bpg_decoder_decode( p_sys->p_bpg,
                            p_block->p_buffer,
                            p_block->i_buffer ) < 0 )
    {
        msg_Err( p_dec, "Could not decode block" );
        goto error;
    }

    if( bpg_decoder_get_info( p_sys->p_bpg, &img_info ) )
    {
        msg_Err( p_dec, "Could not get info for decoder" );
        goto error;
    }

    if( bpg_decoder_start( p_sys->p_bpg, BPG_OUTPUT_FORMAT_RGB24 ) )
    {
        msg_Err( p_dec, "Could not start decoder" );
        goto error;
    }

    /* Set output properties */
    p_dec->fmt_out.i_codec = VLC_CODEC_RGB24;
    p_dec->fmt_out.video.i_visible_width  = p_dec->fmt_out.video.i_width  = img_info.width;
    p_dec->fmt_out.video.i_visible_height = p_dec->fmt_out.video.i_height = img_info.height;
    p_dec->fmt_out.video.i_sar_num = 1;
    p_dec->fmt_out.video.i_sar_den = 1;

    /* Get a new picture */
    if( decoder_UpdateVideoFormat( p_dec ) )
    {
        goto error;
    }
    p_pic = decoder_NewPicture( p_dec );
    if( !p_pic )
    {
        goto error;
    }

    const int img_height = img_info.height;
    for (int i = 0; i < img_height; i++)
    {
        if( bpg_decoder_get_line( p_sys->p_bpg,
                                  p_pic->p->p_pixels + p_pic->p->i_pitch * i )
                                  < 0 )
        {
            msg_Err( p_dec, "Could not decode line" );
            goto error;
        }
    }

    p_pic->date = p_block->i_pts > VLC_TS_INVALID ? p_block->i_pts : p_block->i_dts;

    decoder_QueueVideo( p_dec, p_pic );
error:
    block_Release( p_block );
    return VLCDEC_SUCCESS;
}

static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_bpg )
        bpg_decoder_close( p_sys->p_bpg );

    free( p_sys );
}
