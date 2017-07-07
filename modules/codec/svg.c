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

static int DecodeBlock  ( decoder_t *, block_t * );

#define TEXT_WIDTH       N_("Image width")
#define LONG_TEXT_WIDTH  N_("Specify the width to decode the image too")
#define TEXT_HEIGHT      N_("Image height")
#define LONG_TEXT_HEIGHT N_("Specify the height to decode the image too")
#define TEXT_SCALE       N_("Scale factor")
#define LONG_TEXT_SCALE  N_("Scale factor to apply to image")

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_description( N_("SVG video decoder") )
    set_capability( "video decoder", 100 )
    set_callbacks( OpenDecoder, CloseDecoder )
    add_shortcut( "svg" )

    /* svg options */
    add_integer_with_range( "svg-width", -1, 1, 65535,
                            TEXT_WIDTH, LONG_TEXT_WIDTH, false )
        change_safe()
    add_integer_with_range( "svg-height", -1, 1, 65535,
                            TEXT_HEIGHT, LONG_TEXT_HEIGHT, false )
        change_safe()

    add_float( "svg-scale", -1.0, TEXT_SCALE, LONG_TEXT_SCALE, false )
vlc_module_end ()

struct decoder_sys_t
{
    int32_t i_width;
    int32_t i_height;
    double  f_scale;
};

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_SVG )
        return VLC_EGENERIC;

    decoder_sys_t *p_sys = malloc( sizeof(decoder_sys_t) );
    if (!p_sys)
        return VLC_ENOMEM;
    p_dec->p_sys = p_sys;

    p_sys->i_width = var_InheritInteger( p_this, "svg-width" );
    p_sys->i_height = var_InheritInteger( p_this, "svg-height" );
    p_sys->f_scale = var_InheritFloat( p_this, "svg-scale" );

    /* Initialize library */
#if (GLIB_MAJOR_VERSION < 2 || GLIB_MINOR_VERSION < 36)
    g_type_init();
#endif

    /* Set output properties */
    p_dec->fmt_out.i_codec = VLC_CODEC_BGRA;

    /* Set callbacks */
    p_dec->pf_decode = DecodeBlock;

    return VLC_SUCCESS;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with a complete image.
 ****************************************************************************/
static int DecodeBlock( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys  = (decoder_sys_t *) p_dec->p_sys;
    picture_t *p_pic = NULL;
    int32_t i_width, i_height;

    RsvgHandle *rsvg = NULL;
    cairo_surface_t *surface = NULL;
    cairo_t *cr = NULL;

    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    if( p_block->i_flags & BLOCK_FLAG_CORRUPTED)
    {
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    rsvg = rsvg_handle_new_from_data( p_block->p_buffer, p_block->i_buffer, NULL );
    if( !rsvg )
        goto done;

    RsvgDimensionData dim;
    rsvg_handle_get_dimensions( rsvg, &dim );

    if( p_sys->f_scale > 0.0 )
    {
        i_width  = (int32_t)(p_sys->f_scale * dim.width);
        i_height = (int32_t)(p_sys->f_scale * dim.height);
    }
    else
    {
        /* Keep aspect */
        if( p_sys->i_width < 0 && p_sys->i_height > 0 )
        {
            i_width  = dim.width * p_sys->i_height / dim.height;
            i_height = p_sys->i_height;
        }
        else if( p_sys->i_width > 0 && p_sys->i_height < 0 )
        {
            i_width  = p_sys->i_width;
            i_height = dim.height * p_sys->i_width / dim.height;
        }
        else if( p_sys->i_width > 0 && p_sys->i_height > 0 )
        {
            i_width  = dim.width * p_sys->i_height / dim.height;
            i_height = p_sys->i_height;
        }
        else
        {
            i_width  = dim.width;
            i_height = dim.height;
        }
    }

    p_dec->fmt_out.i_codec =
    p_dec->fmt_out.video.i_chroma = VLC_CODEC_BGRA;
    p_dec->fmt_out.video.i_width  = i_width;
    p_dec->fmt_out.video.i_height = i_height;
    p_dec->fmt_out.video.i_visible_width  = i_width;
    p_dec->fmt_out.video.i_visible_height = i_height;
    p_dec->fmt_out.video.i_sar_num = 1;
    p_dec->fmt_out.video.i_sar_den = 1;
    p_dec->fmt_out.video.i_rmask = 0x80800000; /* Since librsvg v1.0 */
    p_dec->fmt_out.video.i_gmask = 0x0000ff00;
    p_dec->fmt_out.video.i_bmask = 0x000000ff;
    video_format_FixRgb(&p_dec->fmt_out.video);

    /* Get a new picture */
    if( decoder_UpdateVideoFormat( p_dec ) )
        goto done;
    p_pic = decoder_NewPicture( p_dec );
    if( !p_pic )
        goto done;

    /* NOTE: Do not use the stride calculation from cairo, because it is wrong:
     * stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, dim.width);
     * Use the stride from VLC its picture_t::p[0].i_pitch, which is correct.
     */
    memset(p_pic->p[0].p_pixels, 0, p_pic->p[0].i_pitch * p_pic->p[0].i_lines);
    surface = cairo_image_surface_create_for_data( p_pic->p->p_pixels,
                                                   CAIRO_FORMAT_ARGB32,
                                                   i_width, i_height,
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

    if ( i_width != dim.width || i_height != dim.height )
    {
        double sw, sh;
        if ( p_sys->f_scale > 0.0 && !(p_sys->i_width > 0 || p_sys->i_height > 0) )
            sw = sh = p_sys->f_scale;
        else
        {
            double aspect = (double) (dim.width * p_dec->fmt_out.video.i_sar_num) /
                    (dim.height * p_dec->fmt_out.video.i_sar_den);
            sw = aspect * i_width / dim.width;
            sh = aspect * i_height / dim.height;
        }
        cairo_scale(cr, sw, sh);
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
    if( p_pic != NULL )
        decoder_QueueVideo( p_dec, p_pic );
    return VLCDEC_SUCCESS;
}

/*****************************************************************************
 * CloseDecoder: png decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    VLC_UNUSED( p_this );
#if (GLIB_MAJOR_VERSION < 2 || GLIB_MINOR_VERSION < 36)
    rsvg_term();
#endif
}
