/*****************************************************************************
 * psychedelic.c : Psychedelic video effect plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2006 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Antoine Cellerier <dionoea -at- videolan -dot- org>
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

#include <math.h>                                            /* sin(), cos() */

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_image.h>
#include "filter_picture.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Psychedelic video filter") )
    set_shortname( N_( "Psychedelic" ))
    set_capability( "video filter", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_shortcut( "psychedelic" )
    set_callbacks( Create, Destroy )
vlc_module_end ()

/*****************************************************************************
 * filter_sys_t: Distort video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Distort specific properties of an output thread.
 *****************************************************************************/
typedef struct
{
    image_handler_t *p_image;
    unsigned int x, y, scale;
    int xinc, yinc, scaleinc;
    uint8_t u,v;
} filter_sys_t;

/*****************************************************************************
 * Create: allocates Distort video thread output method
 *****************************************************************************
 * This function allocates and initializes a Distort vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    const vlc_fourcc_t fourcc = p_filter->fmt_in.video.i_chroma;
    const vlc_chroma_description_t *p_chroma = vlc_fourcc_GetChromaDescription( fourcc );
    if( !p_chroma || p_chroma->plane_count != 3 || p_chroma->pixel_size != 1 ) {
        msg_Err( p_filter, "Unsupported chroma (%4.4s)", (char*)&fourcc );
        return VLC_EGENERIC;
    }

    /* Allocate structure */
    filter_sys_t *p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;
    p_filter->p_sys = p_sys;

    p_filter->pf_video_filter = Filter;

    p_sys->x = 10;
    p_sys->y = 10;
    p_sys->scale = 1;
    p_sys->xinc = 1;
    p_sys->yinc = 1;
    p_sys->scaleinc = 1;
    p_sys->u = 0;
    p_sys->v = 0;
    p_sys->p_image = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy Distort video thread output method
 *****************************************************************************
 * Terminate an output method created by DistortCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    if( p_sys->p_image )
        image_HandlerDelete( p_sys->p_image );
    p_sys->p_image = NULL;

    free( p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Distort image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;

    unsigned int w, h;
    uint8_t u,v;

    picture_t *p_converted;
    video_format_t fmt_out;

    if( !p_pic ) return NULL;

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }

    filter_sys_t *p_sys = p_filter->p_sys;

    if( !p_sys->p_image )
        p_sys->p_image = image_HandlerCreate( p_filter );

    /* chrominance */
    u = p_sys->u;
    v = p_sys->v;
    for( int y = 0; y < p_outpic->p[U_PLANE].i_lines; y++ )
    {
        memset(
                p_outpic->p[U_PLANE].p_pixels+y*p_outpic->p[U_PLANE].i_pitch,
                u, p_outpic->p[U_PLANE].i_pitch );
        memset(
                p_outpic->p[V_PLANE].p_pixels+y*p_outpic->p[V_PLANE].i_pitch,
                v, p_outpic->p[V_PLANE].i_pitch );
        if( v == 0 && u != 0 )
            u --;
        else if( u == 0xff )
            v --;
        else if( v == 0xff )
            u ++;
        else if( u == 0 )
            v ++;
    }

    /* luminance */
    plane_CopyPixels( &p_outpic->p[Y_PLANE], &p_pic->p[Y_PLANE] );

    /* image visualization */
    fmt_out = p_filter->fmt_out.video;
    fmt_out.i_width = p_filter->fmt_out.video.i_width*p_sys->scale/150;
    fmt_out.i_height = p_filter->fmt_out.video.i_height*p_sys->scale/150;
    fmt_out.i_visible_width = fmt_out.i_width;
    fmt_out.i_visible_height = fmt_out.i_height;
    p_converted = image_Convert( p_sys->p_image, p_pic,
                                 &(p_pic->format), &fmt_out );

    if( p_converted )
    {
#define copyimage( plane, b ) \
        for( int y = 0; y<p_converted->p[plane].i_visible_lines; y++ ) { \
        for( int x = 0; x<p_converted->p[plane].i_visible_pitch; x++ ) { \
            int nx, ny; \
            if( p_sys->yinc == 1 ) \
                ny= y; \
            else \
                ny = p_converted->p[plane].i_visible_lines-y -1; \
            if( p_sys->xinc == 1 ) \
                nx = x; \
            else \
                nx = p_converted->p[plane].i_visible_pitch-x -1; \
            p_outpic->p[plane].p_pixels[(p_sys->x*b+nx)+(ny+p_sys->y*b)*p_outpic->p[plane].i_pitch ] = p_converted->p[plane].p_pixels[y*p_converted->p[plane].i_pitch+x]; \
        } }
        copyimage( Y_PLANE, 2 );
        copyimage( U_PLANE, 1 );
        copyimage( V_PLANE, 1 );
#undef copyimage

        picture_Release( p_converted );
    }
    else
    {
        msg_Err( p_filter, "Image scaling failed miserably." );
    }

    p_sys->x += p_sys->xinc;
    p_sys->y += p_sys->yinc;

    p_sys->scale += p_sys->scaleinc;
    if( p_sys->scale >= 50 ) p_sys->scaleinc = -1;
    if( p_sys->scale <= 1 ) p_sys->scaleinc = 1;

    w = p_filter->fmt_out.video.i_width*p_sys->scale/150;
    h = p_filter->fmt_out.video.i_height*p_sys->scale/150;
    if( p_sys->x*2 + w >= p_filter->fmt_out.video.i_width )
        p_sys->xinc = -1;
    if( p_sys->x <= 0 )
        p_sys->xinc = 1;

    if( p_sys->x*2 + w >= p_filter->fmt_out.video.i_width )
        p_sys->x = (p_filter->fmt_out.video.i_width-w)/2;
    if( p_sys->y*2 + h >= p_filter->fmt_out.video.i_height )
        p_sys->y = (p_filter->fmt_out.video.i_height-h)/2;

    if( p_sys->y*2 + h >= p_filter->fmt_out.video.i_height )
        p_sys->yinc = -1;
    if( p_sys->y <= 0 )
        p_sys->yinc = 1;

    for( int y = 0; y < 16; y++ )
    {
        if( p_sys->v == 0 && p_sys->u != 0 )
            p_sys->u -= 1;
        else if( p_sys->u == 0xff )
            p_sys->v -= 1;
        else if( p_sys->v == 0xff )
            p_sys->u += 1;
        else if( p_sys->u == 0 )
            p_sys->v += 1;
    }

    return CopyInfoAndRelease( p_outpic, p_pic );
}
