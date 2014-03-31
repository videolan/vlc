/*****************************************************************************
 * svg.c: svg decoder module making use of librsvg2.
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 *
 * Authors: Adam Leggett <adamvleggett@gmail.com>
 *          Jean-Paul Saman <jpsaman@videolan.org>
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
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>                                  /* g_object_unref( ) */

#include <librsvg/rsvg.h>
#include <cairo/cairo.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

static picture_t *DecodeBlock  ( decoder_t *, block_t ** );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_description( N_("SVG video decoder") )
    set_capability( "decoder", 100 )
    set_callbacks( OpenDecoder, CloseDecoder )
    add_shortcut( "svg" )
vlc_module_end ()

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_SVG )
        return VLC_EGENERIC;

    /* Initialize library */
    rsvg_init();

    /* Set output properties */
    p_dec->fmt_out.i_cat = VIDEO_ES;
    p_dec->fmt_out.i_codec = VLC_CODEC_RGB32;

    /* Set callbacks */
    p_dec->pf_decode_video = DecodeBlock;

    return VLC_SUCCESS;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with a complete image.
 ****************************************************************************/
static picture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    block_t *p_block;
    picture_t *p_pic = NULL;

    RsvgHandle *rsvg = NULL;
    cairo_surface_t *surface = NULL;
    cairo_t *cr = NULL;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;
    *pp_block = NULL;

    if( p_block->i_flags & BLOCK_FLAG_DISCONTINUITY )
    {
        block_Release( p_block );
        return NULL;
    }

    rsvg = rsvg_handle_new_from_data( p_block->p_buffer, p_block->i_buffer, NULL );
    if( !rsvg )
        goto done;

    RsvgDimensionData dim;
    rsvg_handle_get_dimensions( rsvg, &dim );

    p_dec->fmt_out.video.i_chroma = VLC_CODEC_RGB32;
    p_dec->fmt_out.video.i_width  = dim.width;
    p_dec->fmt_out.video.i_height = dim.height;
    p_dec->fmt_out.video.i_visible_width  = dim.width;
    p_dec->fmt_out.video.i_visible_height = dim.height;
    p_dec->fmt_out.video.i_sar_num = 1;
    p_dec->fmt_out.video.i_sar_den = 1;
    p_dec->fmt_out.video.i_rmask = 0x80800000; /* Since librsvg v1.0 */
    p_dec->fmt_out.video.i_gmask = 0x0000ff00;
    p_dec->fmt_out.video.i_bmask = 0x000000ff;
    video_format_FixRgb(&p_dec->fmt_out.video);

    /* Get a new picture */
    p_pic = decoder_NewPicture( p_dec );
    if( !p_pic )
        goto done;

    /* NOTE: Do not use the stride calculation from cairo, because it is wrong:
     * stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, dim.width);
     * Use the stride from VLC its picture_t::p[0].i_pitch, which is correct.
     */
    surface = cairo_image_surface_create_for_data( p_pic->p->p_pixels,
                                                   CAIRO_FORMAT_ARGB32,
                                                   dim.width, dim.height,
                                                   p_pic->p[0].i_pitch );
    if( !surface )
    {
        picture_Release( p_pic );
        p_pic = NULL;
        goto done;
    }

    /* Decode picture */
    cr = cairo_create( surface );
    if( !cr )
    {
        picture_Release( p_pic );
        p_pic = NULL;
        goto done;
    }

    if( !rsvg_handle_render_cairo( rsvg, cr ) )
    {
        picture_Release( p_pic );
        p_pic = NULL;
        goto done;
    }

    p_pic->date = p_block->i_pts > VLC_TS_INVALID ? p_block->i_pts : p_block->i_dts;

done:
    if( rsvg )
        g_object_unref( G_OBJECT( rsvg ) );
    if( cr )
        cairo_destroy( cr );
    if( surface )
        cairo_surface_destroy( surface );

    block_Release( p_block );
    return p_pic;
}

/*****************************************************************************
 * CloseDecoder: png decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    VLC_UNUSED( p_this );
    rsvg_term();
}
