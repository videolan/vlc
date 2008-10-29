/*****************************************************************************
 * noise.c : "add grain to image" video filter
 *****************************************************************************
 * Copyright (C) 2000-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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
#include <vlc_vout.h>

#include "vlc_filter.h"
#include "filter_picture.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );

#define FILTER_PREFIX "grain-"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Grain video filter") )
    set_shortname( N_( "Grain" ))
    set_capability( "video filter2", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    set_callbacks( Create, Destroy )
vlc_module_end ()

struct filter_sys_t
{
    int *p_noise;
};

static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    switch( p_filter->fmt_in.video.i_chroma )
    {
        CASE_PLANAR_YUV
            break;

        default:
            msg_Err( p_filter, "Unsupported input chroma (%4s)",
                     (char*)&(p_filter->fmt_in.video.i_chroma) );
            return VLC_EGENERIC;
    }

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;

    p_filter->pf_video_filter = Filter;

    p_filter->p_sys->p_noise = NULL;

    return VLC_SUCCESS;
}

static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    free( p_filter->p_sys->p_noise );
    free( p_filter->p_sys );
}

static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    filter_sys_t *p_sys = p_filter->p_sys;
    int i_index;

    if( !p_pic ) return NULL;

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }

    {
        uint8_t *p_in = p_pic->p[Y_PLANE].p_pixels;
        uint8_t *p_out = p_outpic->p[Y_PLANE].p_pixels;

        const int i_num_lines = p_pic->p[Y_PLANE].i_visible_lines;
        const int i_num_cols = p_pic->p[Y_PLANE].i_visible_pitch;
        const int i_pitch = p_pic->p[Y_PLANE].i_pitch;

        int i_line, i_col;

        int *p_noise = p_sys->p_noise;
        if( !p_noise )
        {
            p_noise = p_sys->p_noise =
                (int*)malloc(i_pitch*i_num_lines*sizeof(int));
        }

        for( i_line = 0; i_line < i_num_lines; i_line++ )
        {
            for( i_col = 0; i_col < i_num_cols; i_col++ )
            {
                p_noise[i_line*i_pitch+i_col] = ((rand()&0x1f)-0x0f);
            }
        }

        for( i_line = 2/*0*/ ; i_line < i_num_lines-2/**/; i_line++ )
        {
            for( i_col = 2/*0*/; i_col < i_num_cols/2; i_col++ )
            {
                p_out[i_line*i_pitch+i_col] = clip_uint8_vlc(
                          p_in[i_line*i_pitch+i_col]
#if 0
                        + p_noise[i_line*i_pitch+i_col] );
#else
/* 2 rows up */
              + ((  ( p_noise[(i_line-2)*i_pitch+i_col-2]<<1 )
              + ( p_noise[(i_line-2)*i_pitch+i_col-1]<<2 )
              + ( p_noise[(i_line-2)*i_pitch+i_col]<<2 )
              + ( p_noise[(i_line-2)*i_pitch+i_col+1]<<2 )
              + ( p_noise[(i_line-2)*i_pitch+i_col+2]<<1 )
              /* 1 row up */
              + ( p_noise[(i_line-1)*i_pitch+i_col-2]<<2 )
              + ( p_noise[(i_line-1)*i_pitch+i_col-1]<<3 )
              + ( p_noise[(i_line-1)*i_pitch+i_col]*12 )
              + ( p_noise[(i_line-1)*i_pitch+i_col+1]<<3 )
              + ( p_noise[(i_line-1)*i_pitch+i_col+2]<<2 )
              /* */
              + ( p_noise[i_line*i_pitch+i_col-2]<<2 )
              + ( p_noise[i_line*i_pitch+i_col-1]*12 )
              + ( p_noise[i_line*i_pitch+i_col]<<4 )
              + ( p_noise[i_line*i_pitch+i_col+1]*12 )
              + ( p_noise[i_line*i_pitch+i_col+2]<<2 )
              /* 1 row down */
              + ( p_noise[(i_line+1)*i_pitch+i_col-2]<<2 )
              + ( p_noise[(i_line+1)*i_pitch+i_col-1]<<3 )
              + ( p_noise[(i_line+1)*i_pitch+i_col]*12 )
              + ( p_noise[(i_line+1)*i_pitch+i_col+1]<<3 )
              + ( p_noise[(i_line+1)*i_pitch+i_col+2]<<2 )
              /* 2 rows down */
              + ( p_noise[(i_line+2)*i_pitch+i_col-2]<<1 )
              + ( p_noise[(i_line+2)*i_pitch+i_col-1]<<2 )
              + ( p_noise[(i_line+2)*i_pitch+i_col]<<2 )
              + ( p_noise[(i_line+2)*i_pitch+i_col+1]<<2 )
              + ( p_noise[(i_line+2)*i_pitch+i_col+2]<<1 )
              )>>7/*/152*/));
#endif

            }
            for( ; i_col < i_num_cols; i_col++ )
                p_out[i_line*i_pitch+i_col] = p_in[i_line*i_pitch+i_col];
        }
    }

    for( i_index = 1; i_index < p_pic->i_planes; i_index++ )
    {
        uint8_t *p_in = p_pic->p[i_index].p_pixels;
        uint8_t *p_out = p_outpic->p[i_index].p_pixels;

        const int i_lines = p_pic->p[i_index].i_lines;
        const int i_pitch = p_pic->p[i_index].i_pitch;

        vlc_memcpy( p_out, p_in, i_lines * i_pitch );

    }

    return CopyInfoAndRelease( p_outpic, p_pic );
}
