/*****************************************************************************
 * filter.c : filter_t helpers.
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Author: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#include <vlc_common.h>
#include <libvlc.h>
#include <vlc_filter.h>

filter_t *filter_NewBlend( vlc_object_t *p_this,
                           vlc_fourcc_t i_chroma_dst )
{
    filter_t *p_blend = vlc_custom_create( p_this, sizeof(*p_blend),
                                           VLC_OBJECT_GENERIC, "blend" );
    if( !p_blend )
        return NULL;

    es_format_Init( &p_blend->fmt_in, VIDEO_ES, 0 );

    es_format_Init( &p_blend->fmt_out, VIDEO_ES, 0 );

    p_blend->fmt_out.i_codec        = 
    p_blend->fmt_out.video.i_chroma = i_chroma_dst;

    /* The blend module will be loaded when needed with the real
    * input format */
    p_blend->p_module = NULL;

    /* */
    vlc_object_attach( p_blend, p_this );

    return p_blend;
}

int filter_ConfigureBlend( filter_t *p_blend,
                           int i_dst_width, int i_dst_height,
                           const video_format_t *p_src )
{
    /* */
    if( p_blend->p_module &&
        p_blend->fmt_in.video.i_chroma != p_src->i_chroma )
    {
        /* The chroma is not the same, we need to reload the blend module */
        module_unneed( p_blend, p_blend->p_module );
        p_blend->p_module = NULL;
    }

    /* */

    p_blend->fmt_in.i_codec = p_src->i_chroma;
    p_blend->fmt_in.video   = *p_src;

    /* */
    p_blend->fmt_out.video.i_width          =
    p_blend->fmt_out.video.i_visible_width  = i_dst_width;
    p_blend->fmt_out.video.i_height         =
    p_blend->fmt_out.video.i_visible_height = i_dst_height;

    /* */
    if( !p_blend->p_module )
        p_blend->p_module = module_need( p_blend, "video blending", NULL, false );
    if( !p_blend->p_module )
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

int filter_Blend( filter_t *p_blend,
                  picture_t *p_dst, int i_dst_x, int i_dst_y,
                  const picture_t *p_src, int i_alpha )
{
    if( !p_blend->p_module )
        return VLC_EGENERIC;

    p_blend->pf_video_blend( p_blend, p_dst, p_src, i_dst_x, i_dst_y, i_alpha );
    return VLC_SUCCESS;
}

void filter_DeleteBlend( filter_t *p_blend )
{
    if( p_blend->p_module )
        module_unneed( p_blend, p_blend->p_module );

    vlc_object_detach( p_blend );
    vlc_object_release( p_blend );
}

