/*****************************************************************************
 * rv32.c: conversion plugin to RV32 format.
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Author: Cyril Deguet <asmax@videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include "vlc_filter.h"

/*****************************************************************************
 * filter_sys_t : filter descriptor
 *****************************************************************************/
struct filter_sys_t
{
    es_format_t fmt_in;
    es_format_t fmt_out;
};

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenFilter ( vlc_object_t * );
static void CloseFilter( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("RV32 conversion filter") );
    set_capability( "video filter2", 1 );
    set_callbacks( OpenFilter, CloseFilter );
vlc_module_end();

/*****************************************************************************
 * OpenFilter: probe the filter and return score
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys;

    /* XXX Only support RV24 -> RV32 conversion */
    if( p_filter->fmt_in.video.i_chroma != VLC_FOURCC('R','V','2','4') ||
        p_filter->fmt_out.video.i_chroma != VLC_FOURCC('R', 'V', '3', '2') )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_filter->p_sys = p_sys =
          (filter_sys_t *)malloc(sizeof(filter_sys_t)) ) == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_EGENERIC;
    }

    p_filter->pf_video_filter = Filter;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseFilter: clean up the filter
 *****************************************************************************/
static void CloseFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    free( p_sys );
}

/****************************************************************************
 * Filter: the whole thing
 ****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_pic_dst;
    int i_plane, i;
    unsigned int j;

    /* Request output picture */
    p_pic_dst = p_filter->pf_vout_buffer_new( p_filter );
    if( !p_pic_dst )
    {
        msg_Warn( p_filter, "can't get output picture" );
        if( p_pic->pf_release )
            p_pic->pf_release( p_pic );
        return NULL;
    }

    /* Convert RV24 to RV32 */
    for( i_plane = 0; i_plane < p_pic_dst->i_planes; i_plane++ )
    {
        uint8_t *p_src = p_pic->p[i_plane].p_pixels;
        uint8_t *p_dst = p_pic_dst->p[i_plane].p_pixels;
        unsigned int i_width = p_filter->fmt_out.video.i_width;

        for( i = 0; i < p_pic_dst->p[i_plane].i_lines; i++ )
        {
            for( j = 0; j < i_width; j++ )
            {
                *(p_dst++) = p_src[2];
                *(p_dst++) = p_src[1];
                *(p_dst++) = p_src[0];
                *(p_dst++) = 0xff;  /* Alpha */
                p_src += 3;
            }
            p_src += p_pic->p[i_plane].i_pitch - 3 * i_width;
            p_dst += p_pic_dst->p[i_plane].i_pitch - 4 * i_width;
        }
    }

    p_pic_dst->date = p_pic->date;
    p_pic_dst->b_force = p_pic->b_force;
    p_pic_dst->i_nb_fields = p_pic->i_nb_fields;
    p_pic_dst->b_progressive = p_pic->b_progressive;
    p_pic_dst->b_top_field_first = p_pic->b_top_field_first;

    p_pic->pf_release( p_pic );
    return p_pic_dst;
}

