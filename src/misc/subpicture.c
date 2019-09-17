/*****************************************************************************
 * subpicture.c: Subpicture functions
 *****************************************************************************
 * Copyright (C) 2010 Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#include <vlc_common.h>
#include <vlc_image.h>
#include <vlc_subpicture.h>
#include "subpicture.h"

struct subpicture_private_t
{
    video_format_t src;
    video_format_t dst;
};

subpicture_t *subpicture_New( const subpicture_updater_t *p_upd )
{
    subpicture_t *p_subpic = calloc( 1, sizeof(*p_subpic) );
    if( !p_subpic )
        return NULL;

    p_subpic->i_order    = 0;
    p_subpic->b_absolute = true;
    p_subpic->b_fade     = false;
    p_subpic->b_subtitle = false;
    p_subpic->i_alpha    = 0xFF;
    p_subpic->p_region   = NULL;

    if( p_upd )
    {
        subpicture_private_t *p_private = malloc( sizeof(*p_private) );
        if( !p_private )
        {
            free( p_subpic );
            return NULL;
        }
        video_format_Init( &p_private->src, 0 );
        video_format_Init( &p_private->dst, 0 );

        p_subpic->updater   = *p_upd;
        p_subpic->p_private = p_private;
    }
    else
    {
        p_subpic->p_private = NULL;

        p_subpic->updater.pf_validate = NULL;
        p_subpic->updater.pf_update   = NULL;
        p_subpic->updater.pf_destroy  = NULL;
        p_subpic->updater.p_sys       = NULL;
    }
    return p_subpic;
}

void subpicture_Delete( subpicture_t *p_subpic )
{
    subpicture_region_ChainDelete( p_subpic->p_region );
    p_subpic->p_region = NULL;

    if( p_subpic->updater.pf_destroy )
        p_subpic->updater.pf_destroy( p_subpic );

    if( p_subpic->p_private )
    {
        video_format_Clean( &p_subpic->p_private->src );
        video_format_Clean( &p_subpic->p_private->dst );
    }

    free( p_subpic->p_private );
    free( p_subpic );
}

subpicture_t *subpicture_NewFromPicture( vlc_object_t *p_obj,
                                         picture_t *p_picture, vlc_fourcc_t i_chroma )
{
    /* */
    video_format_t fmt_in = p_picture->format;

    /* */
    video_format_t fmt_out;
    fmt_out = fmt_in;
    fmt_out.i_chroma = i_chroma;

    /* */
    image_handler_t *p_image = image_HandlerCreate( p_obj );
    if( !p_image )
        return NULL;

    picture_t *p_pip = image_Convert( p_image, p_picture, &fmt_in, &fmt_out );

    image_HandlerDelete( p_image );

    if( !p_pip )
        return NULL;

    subpicture_t *p_subpic = subpicture_New( NULL );
    if( !p_subpic )
    {
         picture_Release( p_pip );
         return NULL;
    }

    p_subpic->i_original_picture_width  = fmt_out.i_visible_width;
    p_subpic->i_original_picture_height = fmt_out.i_visible_height;

    fmt_out.i_sar_num =
    fmt_out.i_sar_den = 0;

    p_subpic->p_region = subpicture_region_New( &fmt_out );
    if( p_subpic->p_region )
    {
        picture_Release( p_subpic->p_region->p_picture );
        p_subpic->p_region->p_picture = p_pip;
    }
    else
    {
        picture_Release( p_pip );
    }
    return p_subpic;
}

void subpicture_Update( subpicture_t *p_subpicture,
                        const video_format_t *p_fmt_src,
                        const video_format_t *p_fmt_dst,
                        vlc_tick_t i_ts )
{
    subpicture_updater_t *p_upd = &p_subpicture->updater;
    subpicture_private_t *p_private = p_subpicture->p_private;

    if( !p_upd->pf_validate )
        return;
    if( !p_upd->pf_validate( p_subpicture,
                          !video_format_IsSimilar( p_fmt_src,
                                                   &p_private->src ), p_fmt_src,
                          !video_format_IsSimilar( p_fmt_dst,
                                                   &p_private->dst ), p_fmt_dst,
                          i_ts ) )
        return;

    subpicture_region_ChainDelete( p_subpicture->p_region );
    p_subpicture->p_region = NULL;

    p_upd->pf_update( p_subpicture, p_fmt_src, p_fmt_dst, i_ts );

    video_format_Clean( &p_private->src );
    video_format_Clean( &p_private->dst );

    video_format_Copy( &p_private->src, p_fmt_src );
    video_format_Copy( &p_private->dst, p_fmt_dst );
}


subpicture_region_private_t *subpicture_region_private_New( video_format_t *p_fmt )
{
    subpicture_region_private_t *p_private = malloc( sizeof(*p_private) );

    if( !p_private )
        return NULL;

    if ( video_format_Copy( &p_private->fmt, p_fmt ) != VLC_SUCCESS )
    {
        free( p_private );
        return NULL;
    }

    p_private->p_picture = NULL;
    return p_private;
}

void subpicture_region_private_Delete( subpicture_region_private_t *p_private )
{
    if( p_private->p_picture )
        picture_Release( p_private->p_picture );
    video_format_Clean( &p_private->fmt );
    free( p_private );
}

subpicture_region_t * subpicture_region_NewInternal( const video_format_t *p_fmt )
{
    subpicture_region_t *p_region = calloc( 1, sizeof(*p_region ) );
    if( !p_region )
        return NULL;

    p_region->zoom_h.den = p_region->zoom_h.num = 1;
    p_region->zoom_v.den = p_region->zoom_v.num = 1;

    if ( p_fmt->i_chroma == VLC_CODEC_YUVP )
    {
        video_format_Copy( &p_region->fmt, p_fmt );
        /* YUVP should have a palette */
        if( p_region->fmt.p_palette == NULL )
        {
            p_region->fmt.p_palette = calloc( 1, sizeof(*p_region->fmt.p_palette) );
            if( p_region->fmt.p_palette == NULL )
            {
                free( p_region );
                return NULL;
            }
        }
    }
    else
    {
        p_region->fmt = *p_fmt;
        p_region->fmt.p_palette = NULL;
    }

    p_region->i_alpha = 0xff;
    p_region->b_balanced_text = true;

    return p_region;
}

subpicture_region_t *subpicture_region_New( const video_format_t *p_fmt )
{
    subpicture_region_t *p_region =
        subpicture_region_NewInternal( p_fmt );
    if( !p_region )
        return NULL;

    if( p_fmt->i_chroma == VLC_CODEC_TEXT )
        return p_region;

    p_region->p_picture = picture_NewFromFormat( p_fmt );
    if( !p_region->p_picture )
    {
        video_format_Clean( &p_region->fmt );
        free( p_region );
        return NULL;
    }

    return p_region;
}

void subpicture_region_Delete( subpicture_region_t *p_region )
{
    if( !p_region )
        return;

    if( p_region->p_private )
        subpicture_region_private_Delete( p_region->p_private );

    if( p_region->p_picture )
        picture_Release( p_region->p_picture );

    text_segment_ChainDelete( p_region->p_text );
    video_format_Clean( &p_region->fmt );
    free( p_region );
}

void subpicture_region_ChainDelete( subpicture_region_t *p_head )
{
    while( p_head )
    {
        subpicture_region_t *p_next = p_head->p_next;

        subpicture_region_Delete( p_head );

        p_head = p_next;
    }
}

#include <vlc_filter.h>

unsigned picture_BlendSubpicture(picture_t *dst,
                                 vlc_blender_t *blend, subpicture_t *src)
{
    unsigned done = 0;

    assert(src && !src->b_fade && src->b_absolute);

    for (subpicture_region_t *r = src->p_region; r != NULL; r = r->p_next) {
        assert(r->p_picture && r->i_align == 0);
        if (filter_ConfigureBlend(blend, dst->format.i_width,
                                  dst->format.i_height,  &r->fmt)
         || filter_Blend(blend, dst, r->i_x, r->i_y, r->p_picture,
                         src->i_alpha * r->i_alpha / 255))
            msg_Err(blend, "blending %4.4s to %4.4s failed",
                    (char *)&blend->fmt_in.video.i_chroma,
                    (char *)&blend->fmt_out.video.i_chroma );
        else
            done++;
    }
    return done;
}

subpicture_region_t* subpicture_region_Copy( subpicture_region_t *p_region_src )
{
    if (!p_region_src)
        return NULL;
    subpicture_region_t *p_region_dst = subpicture_region_New(&p_region_src->fmt);
    if (unlikely(!p_region_dst))
        return NULL;

    p_region_dst->i_x      = p_region_src->i_x;
    p_region_dst->i_y      = p_region_src->i_y;
    p_region_dst->i_align  = p_region_src->i_align;
    p_region_dst->i_alpha  = p_region_src->i_alpha;

    p_region_dst->p_text = text_segment_Copy( p_region_src->p_text );

    //Palette is already copied by subpicture_region_New, we just have to duplicate p_pixels
    for (int i = 0; i < p_region_src->p_picture->i_planes; i++)
        memcpy(p_region_dst->p_picture->p[i].p_pixels,
               p_region_src->p_picture->p[i].p_pixels,
               p_region_src->p_picture->p[i].i_lines * p_region_src->p_picture->p[i].i_pitch);
    return p_region_dst;
}
