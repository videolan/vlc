/*****************************************************************************
 * picture.c : picture management functions
 *****************************************************************************
 * Copyright (C) 2000-2010 VLC authors and VideoLAN
 * Copyright (C) 2009-2010 Laurent Aimar
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
#include <assert.h>
#include <limits.h>

#include <vlc_common.h>
#include "picture.h"
#include <vlc_image.h>
#include <vlc_block.h>

static void PictureDestroyContext( picture_t *p_picture )
{
    picture_context_t *ctx = p_picture->context;
    if (ctx != NULL)
    {
        ctx->destroy(ctx);
        p_picture->context = NULL;
    }
}

/**
 * Destroys a picture allocated by picture_NewFromResource() but without
 * a custom destruction callback.
 */
static void picture_DestroyFromResource( picture_t *p_picture )
{
    free( p_picture->p_sys );
    free( p_picture );
}

/**
 * Destroys a picture allocated with picture_NewFromFormat().
 */
static void picture_Destroy( picture_t *p_picture )
{
    aligned_free( p_picture->p[0].p_pixels );
    free( p_picture );
}

/*****************************************************************************
 *
 *****************************************************************************/
void picture_Reset( picture_t *p_picture )
{
    /* */
    p_picture->date = VLC_TICK_INVALID;
    p_picture->b_force = false;
    p_picture->b_progressive = false;
    p_picture->i_nb_fields = 2;
    p_picture->b_top_field_first = false;
    PictureDestroyContext( p_picture );
}

/*****************************************************************************
 *
 *****************************************************************************/
static int LCM( int a, int b )
{
    return a * b / GCD( a, b );
}

int picture_Setup( picture_t *p_picture, const video_format_t *restrict fmt )
{
    const vlc_chroma_description_t *p_dsc =
        vlc_fourcc_GetChromaDescription( fmt->i_chroma );
    if( unlikely(!p_dsc) )
        return VLC_EGENERIC;

    /* Store default values */
    p_picture->i_planes = 0;
    for( unsigned i = 0; i < VOUT_MAX_PLANES; i++ )
    {
        plane_t *p = &p_picture->p[i];
        p->p_pixels = NULL;
        p->i_pixel_pitch = 0;
    }

    p_picture->i_nb_fields = 2;

    video_format_Setup( &p_picture->format, fmt->i_chroma, fmt->i_width, fmt->i_height,
                        fmt->i_visible_width, fmt->i_visible_height,
                        fmt->i_sar_num, fmt->i_sar_den );
    if( fmt->i_x_offset < fmt->i_width &&
        fmt->i_y_offset < fmt->i_height &&
        fmt->i_visible_width  > 0 && fmt->i_x_offset + fmt->i_visible_width  <= fmt->i_width &&
        fmt->i_visible_height > 0 && fmt->i_y_offset + fmt->i_visible_height <= fmt->i_height )
        video_format_CopyCrop( &p_picture->format, fmt );

    /* We want V (width/height) to respect:
        (V * p_dsc->p[i].w.i_num) % p_dsc->p[i].w.i_den == 0
        (V * p_dsc->p[i].w.i_num/p_dsc->p[i].w.i_den * p_dsc->i_pixel_size) % 16 == 0
       Which is respected if you have
       V % lcm( p_dsc->p[0..planes].w.i_den * 16) == 0
    */
    unsigned i_modulo_w = 1;
    unsigned i_modulo_h = 1;
    unsigned i_ratio_h  = 1;

    for( unsigned i = 0; i < p_dsc->plane_count; i++ )
    {
        i_modulo_w = LCM( i_modulo_w, 16 * p_dsc->p[i].w.den );
        i_modulo_h = LCM( i_modulo_h, 16 * p_dsc->p[i].h.den );
        if( i_ratio_h < p_dsc->p[i].h.den )
            i_ratio_h = p_dsc->p[i].h.den;
    }
    i_modulo_h = LCM( i_modulo_h, 32 );

    unsigned width, height;

    if (unlikely(add_overflow(fmt->i_width, i_modulo_w - 1, &width))
     || unlikely(add_overflow(fmt->i_height, i_modulo_h - 1, &height)))
        return VLC_EGENERIC;

    width = width / i_modulo_w * i_modulo_w;
    height = height / i_modulo_h * i_modulo_h;

    /* plane_t uses 'int'. */
    if (unlikely(width > INT_MAX) || unlikely(height > INT_MAX))
        return VLC_EGENERIC;

    for( unsigned i = 0; i < p_dsc->plane_count; i++ )
    {
        plane_t *p = &p_picture->p[i];
        const vlc_rational_t *h = &p_dsc->p[i].h;
        const vlc_rational_t *w = &p_dsc->p[i].w;

        /* A plane cannot be over-sampled. This could lead to overflow. */
        assert(h->den >= h->num);
        assert(w->den >= w->num);

        p->i_lines = height * h->num / h->den;
        p->i_visible_lines = (fmt->i_visible_height + (h->den - 1)) / h->den * h->num;

        p->i_pitch = width * w->num / w->den * p_dsc->pixel_size;
        p->i_visible_pitch = (fmt->i_visible_width + (w->den - 1)) / w->den * w->num
                             * p_dsc->pixel_size;
        p->i_pixel_pitch = p_dsc->pixel_size;

        assert( (p->i_pitch % 16) == 0 );
    }
    p_picture->i_planes = p_dsc->plane_count;

    return VLC_SUCCESS;
}

/*****************************************************************************
 *
 *****************************************************************************/

static picture_priv_t *picture_NewPrivate(const video_format_t *restrict p_fmt)
{
    /* */
    picture_priv_t *priv = malloc( sizeof (*priv) );
    if( unlikely(priv == NULL) )
        return NULL;

    picture_t *p_picture = &priv->picture;

    memset( p_picture, 0, sizeof( *p_picture ) );

    /* Make sure the real dimensions are a multiple of 16 */
    if( picture_Setup( p_picture, p_fmt ) )
    {
        free( p_picture );
        return NULL;
    }

    atomic_init( &priv->gc.refs, 1 );
    priv->gc.opaque = NULL;

    return priv;
}

picture_t *picture_NewFromResource( const video_format_t *p_fmt, const picture_resource_t *p_resource )
{
    assert(p_resource != NULL);

    picture_priv_t *priv = picture_NewPrivate(p_fmt);
    if (unlikely(priv == NULL))
        return NULL;

    picture_t *p_picture = &priv->picture;

    p_picture->p_sys = p_resource->p_sys;

    if( p_resource->pf_destroy != NULL )
        priv->gc.destroy = p_resource->pf_destroy;
    else
        priv->gc.destroy = picture_DestroyFromResource;

    for( int i = 0; i < p_picture->i_planes; i++ )
    {
        p_picture->p[i].p_pixels = p_resource->p[i].p_pixels;
        p_picture->p[i].i_lines  = p_resource->p[i].i_lines;
        p_picture->p[i].i_pitch  = p_resource->p[i].i_pitch;
    }

    return p_picture;
}

#define PICTURE_SW_SIZE_MAX (UINT32_C(1) << 28) /* 256MB: 8K * 8K * 4*/

picture_t *picture_NewFromFormat(const video_format_t *restrict fmt)
{
    picture_priv_t *priv = picture_NewPrivate(fmt);
    if (unlikely(priv == NULL))
        return NULL;

    priv->gc.destroy = picture_Destroy;

    picture_t *pic = &priv->picture;
    if (pic->i_planes == 0)
        return pic;

    /* Calculate how big the new image should be */
    size_t plane_sizes[PICTURE_PLANE_MAX];
    size_t pic_size = 0;

    for (int i = 0; i < pic->i_planes; i++)
    {
        const plane_t *p = &pic->p[i];

        if (unlikely(mul_overflow(p->i_pitch, p->i_lines, &plane_sizes[i]))
         || unlikely(add_overflow(pic_size, plane_sizes[i], &pic_size)))
            goto error;
    }

    if (unlikely(pic_size >= PICTURE_SW_SIZE_MAX))
        goto error;

    uint8_t *buf = aligned_alloc(16, pic_size);
    if (unlikely(buf == NULL))
        goto error;

    /* Fill the p_pixels field for each plane */
    for (int i = 0; i < pic->i_planes; i++)
    {
        pic->p[i].p_pixels = buf;
        buf += plane_sizes[i];
    }

    return pic;
error:
    free(pic);
    return NULL;
}

picture_t *picture_New( vlc_fourcc_t i_chroma, int i_width, int i_height, int i_sar_num, int i_sar_den )
{
    video_format_t fmt;

    video_format_Init( &fmt, 0 );
    video_format_Setup( &fmt, i_chroma, i_width, i_height,
                        i_width, i_height, i_sar_num, i_sar_den );

    return picture_NewFromFormat( &fmt );
}

/*****************************************************************************
 *
 *****************************************************************************/

picture_t *picture_Hold( picture_t *p_picture )
{
    assert( p_picture != NULL );

    picture_priv_t *priv = (picture_priv_t *)p_picture;
    uintptr_t refs = atomic_fetch_add( &priv->gc.refs, 1 );
    assert( refs > 0 );
    return p_picture;
}

void picture_Release( picture_t *p_picture )
{
    assert( p_picture != NULL );

    picture_priv_t *priv = (picture_priv_t *)p_picture;
    uintptr_t refs = atomic_fetch_sub( &priv->gc.refs, 1 );
    assert( refs != 0 );
    if( refs > 1 )
        return;

    PictureDestroyContext( p_picture );
    assert( priv->gc.destroy != NULL );
    priv->gc.destroy( p_picture );
}

/*****************************************************************************
 *
 *****************************************************************************/
void plane_CopyPixels( plane_t *p_dst, const plane_t *p_src )
{
    const unsigned i_width  = __MIN( p_dst->i_visible_pitch,
                                     p_src->i_visible_pitch );
    const unsigned i_height = __MIN( p_dst->i_visible_lines,
                                     p_src->i_visible_lines );

    /* The 2x visible pitch check does two things:
       1) Makes field plane_t's work correctly (see the deinterlacer module)
       2) Moves less data if the pitch and visible pitch differ much.
    */
    if( p_src->i_pitch == p_dst->i_pitch  &&
        p_src->i_pitch < 2*p_src->i_visible_pitch )
    {
        /* There are margins, but with the same width : perfect ! */
        memcpy( p_dst->p_pixels, p_src->p_pixels,
                    p_src->i_pitch * i_height );
    }
    else
    {
        /* We need to proceed line by line */
        uint8_t *p_in = p_src->p_pixels;
        uint8_t *p_out = p_dst->p_pixels;

        assert( p_in );
        assert( p_out );

        for( int i_line = i_height; i_line--; )
        {
            memcpy( p_out, p_in, i_width );
            p_in += p_src->i_pitch;
            p_out += p_dst->i_pitch;
        }
    }
}

void picture_CopyProperties( picture_t *p_dst, const picture_t *p_src )
{
    p_dst->date = p_src->date;
    p_dst->b_force = p_src->b_force;

    p_dst->b_progressive = p_src->b_progressive;
    p_dst->i_nb_fields = p_src->i_nb_fields;
    p_dst->b_top_field_first = p_src->b_top_field_first;
}

void picture_CopyPixels( picture_t *p_dst, const picture_t *p_src )
{
    for( int i = 0; i < p_src->i_planes ; i++ )
        plane_CopyPixels( p_dst->p+i, p_src->p+i );

    assert( p_dst->context == NULL );

    if( p_src->context != NULL )
        p_dst->context = p_src->context->copy( p_src->context );
}

void picture_Copy( picture_t *p_dst, const picture_t *p_src )
{
    picture_CopyPixels( p_dst, p_src );
    picture_CopyProperties( p_dst, p_src );
}

static void picture_DestroyClone(picture_t *clone)
{
    picture_t *picture = ((picture_priv_t *)clone)->gc.opaque;

    free(clone);
    picture_Release(picture);
}

picture_t *picture_Clone(picture_t *picture)
{
    /* TODO: merge common code with picture_pool_ClonePicture(). */
    picture_resource_t res = {
        .p_sys = picture->p_sys,
        .pf_destroy = picture_DestroyClone,
    };

    for (int i = 0; i < picture->i_planes; i++) {
        res.p[i].p_pixels = picture->p[i].p_pixels;
        res.p[i].i_lines = picture->p[i].i_lines;
        res.p[i].i_pitch = picture->p[i].i_pitch;
    }

    picture_t *clone = picture_NewFromResource(&picture->format, &res);
    if (likely(clone != NULL)) {
        ((picture_priv_t *)clone)->gc.opaque = picture;
        picture_Hold(picture);

        if (picture->context != NULL)
            clone->context = picture->context->copy(picture->context);
    }
    return clone;
}

/*****************************************************************************
 *
 *****************************************************************************/
int picture_Export( vlc_object_t *p_obj,
                    block_t **pp_image,
                    video_format_t *p_fmt,
                    picture_t *p_picture,
                    vlc_fourcc_t i_format,
                    int i_override_width, int i_override_height )
{
    /* */
    video_format_t fmt_in = p_picture->format;
    if( fmt_in.i_sar_num <= 0 || fmt_in.i_sar_den <= 0 )
    {
        fmt_in.i_sar_num =
        fmt_in.i_sar_den = 1;
    }

    /* */
    video_format_t fmt_out;
    memset( &fmt_out, 0, sizeof(fmt_out) );
    fmt_out.i_sar_num =
    fmt_out.i_sar_den = 1;
    fmt_out.i_chroma  = i_format;

    /* compute original width/height */
    unsigned int i_width, i_height, i_original_width, i_original_height;
    if( fmt_in.i_visible_width > 0 && fmt_in.i_visible_height > 0 )
    {
        i_width = fmt_in.i_visible_width;
        i_height = fmt_in.i_visible_height;
    }
    else
    {
        i_width = fmt_in.i_width;
        i_height = fmt_in.i_height;
    }
    if( fmt_in.i_sar_num >= fmt_in.i_sar_den )
    {
        i_original_width = (int64_t)i_width * fmt_in.i_sar_num / fmt_in.i_sar_den;
        i_original_height = i_height;
    }
    else
    {
        i_original_width =  i_width;
        i_original_height = i_height * fmt_in.i_sar_den / fmt_in.i_sar_num;
    }

    /* */
    fmt_out.i_width  = ( i_override_width < 0 ) ?
                       i_original_width : (unsigned)i_override_width;
    fmt_out.i_height = ( i_override_height < 0 ) ?
                       i_original_height : (unsigned)i_override_height;

    /* scale if only one direction is provided */
    if( fmt_out.i_height == 0 && fmt_out.i_width > 0 )
    {
        fmt_out.i_height = i_height * fmt_out.i_width
                         * fmt_in.i_sar_den / fmt_in.i_width / fmt_in.i_sar_num;
    }
    else if( fmt_out.i_width == 0 && fmt_out.i_height > 0 )
    {
        fmt_out.i_width  = i_width * fmt_out.i_height
                         * fmt_in.i_sar_num / fmt_in.i_height / fmt_in.i_sar_den;
    }

    image_handler_t *p_image = image_HandlerCreate( p_obj );
    if( !p_image )
        return VLC_ENOMEM;

    block_t *p_block = image_Write( p_image, p_picture, &fmt_in, &fmt_out );

    image_HandlerDelete( p_image );

    if( !p_block )
        return VLC_EGENERIC;

    p_block->i_pts =
    p_block->i_dts = p_picture->date;

    if( p_fmt )
        *p_fmt = fmt_out;
    *pp_image = p_block;

    return VLC_SUCCESS;
}
