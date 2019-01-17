/*****************************************************************************
 * freeze.c : Freezing video filter
 *****************************************************************************
 * Copyright (C) 2013      Vianney Boyer
 *
 * Authors: Vianney Boyer <vlcvboyer -at- gmail -dot- com>
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
#include <vlc_filter.h>
#include <vlc_mouse.h>
#include <vlc_picture.h>
#include "filter_picture.h"

#ifndef MOD
#   define MOD(a, b) ((((a)%(b)) + (b))%(b))
#endif

typedef struct
{
    bool b_init;

    int32_t i_planes;
    int32_t *i_height;
    int32_t *i_width;
    int32_t *i_visible_pitch;
    int8_t  ***pi_freezed_picture;   /* records freezed pixels */
    int16_t **pi_freezing_countdown; /* freezed pixel delay    */
    bool    **pb_update_cache;       /* update chache request  */

} filter_sys_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

static picture_t *Filter( filter_t *, picture_t * );

static int  freeze_mouse( filter_t *, vlc_mouse_t *,
                   const vlc_mouse_t *, const vlc_mouse_t * );
static int  freeze_allocate_data( filter_t *, picture_t * );
static void freeze_free_allocated_data( filter_t * );


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define CFG_PREFIX "freeze-"

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin()
    set_description( N_("Freezing interactive video filter") )
    set_shortname(   N_("Freeze" ) )
    set_capability(  "video filter", 0 )
    set_category(    CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    set_callbacks( Open, Close )
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/**
 * Open the filter
 */
static int Open( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    /* Assert video in match with video out */
    if( !es_format_IsSimilar( &p_filter->fmt_in, &p_filter->fmt_out ) ) {
        msg_Err( p_filter, "Input and output format does not match" );
        return VLC_EGENERIC;
    }

    /* Reject 0 bpp and unsupported chroma */
    const vlc_fourcc_t fourcc = p_filter->fmt_in.video.i_chroma;
    const vlc_chroma_description_t *p_chroma
          = vlc_fourcc_GetChromaDescription( p_filter->fmt_in.video.i_chroma );
    if( !p_chroma || p_chroma->pixel_size == 0
        || p_chroma->plane_count < 3 || p_chroma->pixel_size > 1
        || !vlc_fourcc_IsYUV( fourcc ) )
    {
        msg_Err( p_filter, "Unsupported chroma (%4.4s)", (char*)&fourcc );
        return VLC_EGENERIC;
    }

    /* Allocate structure */
    p_filter->p_sys = p_sys = calloc(1, sizeof( *p_sys ) );
    if( unlikely(!p_sys) )
        return VLC_ENOMEM;

    /* init data */

    p_filter->pf_video_filter = Filter;
    p_filter->pf_video_mouse  = freeze_mouse;

    return VLC_SUCCESS;
}

/**
 * Close the filter
 */
static void Close( vlc_object_t *p_this ) {
    filter_t *p_filter  = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    /* Free allocated memory */
    freeze_free_allocated_data( p_filter );
    free( p_sys );
}

/**
 * Filter a picture
 */
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic_in ) {
    if( !p_pic_in || !p_filter) return NULL;

    filter_sys_t *p_sys = p_filter->p_sys;

    picture_t *p_pic_out = filter_NewPicture( p_filter );
    if( unlikely(!p_pic_out) ) {
        picture_Release( p_pic_in );
        return NULL;
    }

   /*
    * allocate data
    */
    if ( unlikely(!p_sys->b_init) )
        if (freeze_allocate_data( p_filter, p_pic_in ) != VLC_SUCCESS)
        {
            picture_Release( p_pic_in );
            return NULL;
        }
    p_sys->b_init = true;

   /*
    * preset output pic: raw copy src to dst
    */
    picture_CopyPixels(p_pic_out, p_pic_in);

   /*
    * cache original pict pixels selected with mouse pointer
    */
    for ( int32_t i_p = 0; i_p < p_sys->i_planes; i_p++ )
        for ( int32_t i_r = 0; i_r < p_sys->i_height[i_p]; i_r++ )
            for ( int32_t i_c = 0; i_c < p_sys->i_width[i_p]; i_c++ )
            {
                uint32_t i_Yr = i_r * p_sys->i_height[Y_PLANE]
                              / p_sys->i_height[i_p];
                uint32_t i_Yc = i_c * p_sys->i_width[Y_PLANE]
                              / p_sys->i_width[i_p];

                if ( p_sys->pb_update_cache[i_Yr][i_Yc] )
                    p_sys->pi_freezed_picture[i_p][i_r][i_c]
                        = p_pic_in->p[i_p].p_pixels[i_r*p_pic_out->p[i_p].i_pitch
                        + i_c*p_pic_out->p[i_p].i_pixel_pitch];
            }

   /*
    * countdown freezed pixel delay & reset pb_update_cache flag
    */
    for ( int32_t i_Yr = 0; i_Yr < p_sys->i_height[Y_PLANE]; i_Yr++)
        for ( int32_t i_Yc = 0; i_Yc < p_sys->i_width[Y_PLANE]; i_Yc++)
        {
            if ( p_sys->pi_freezing_countdown[i_Yr][i_Yc] > 0 )
                 p_sys->pi_freezing_countdown[i_Yr][i_Yc]--;
            p_sys->pb_update_cache[i_Yr][i_Yc] = false;
        }

   /*
    * apply filter: draw freezed pixels over current picture
    */
    for ( int32_t i_p = 0; i_p < p_sys->i_planes; i_p++ )
        for ( int32_t i_r = 0; i_r < p_sys->i_height[i_p]; i_r++ )
            for ( int32_t i_c = 0; i_c < p_sys->i_width[i_p]; i_c++ )
            {
                uint32_t i_Yr = i_r * p_sys->i_height[Y_PLANE]
                              / p_sys->i_height[i_p];
                uint32_t i_Yc = i_c * p_sys->i_width[Y_PLANE]
                              / p_sys->i_width[i_p];

                if ( p_sys->pi_freezing_countdown[i_Yr][i_Yc] > 0 )
                    p_pic_out->p[i_p].p_pixels[i_r * p_pic_out->p[i_p].i_pitch
                        + i_c * p_pic_out->p[i_p].i_pixel_pitch]
                        = p_sys->pi_freezed_picture[i_p][i_r][i_c];
            }

    return CopyInfoAndRelease( p_pic_out, p_pic_in );
}

/*
 * mouse callback
 **/
static int freeze_mouse( filter_t *p_filter, vlc_mouse_t *p_mouse,
                  const vlc_mouse_t *p_old, const vlc_mouse_t *p_new )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    const video_format_t  *p_fmt_in = &p_filter->fmt_in.video;

    /* Only take events inside the video area */
    if( p_new->i_x < 0 || p_new->i_x >= (int)p_fmt_in->i_width ||
        p_new->i_y < 0 || p_new->i_y >= (int)p_fmt_in->i_height )
        return VLC_EGENERIC;

    if ( unlikely(!p_sys->b_init) )
    {
        *p_mouse = *p_new;
        return VLC_SUCCESS;
    }

    int32_t i_base_timeout = 0;
    if( vlc_mouse_HasPressed( p_old, p_new, MOUSE_BUTTON_LEFT ) )
        i_base_timeout = 100;
    else if( vlc_mouse_IsLeftPressed( p_new ) )
        i_base_timeout = 50;

    if( i_base_timeout > 0 )
    {
        /*
         * find pixels selected by user to apply freezing filter
         */
        int32_t i_min_sq_radius = (p_sys->i_width[Y_PLANE] / 15)
                                * (p_sys->i_width[Y_PLANE] / 15);
        for ( int32_t i_r = 0; i_r < p_sys->i_height[Y_PLANE]; i_r++)
            for ( int32_t i_c = 0; i_c < p_sys->i_width[Y_PLANE]; i_c++)
            {
                int32_t i_sq_dist =   ( p_new->i_x - i_c )
                                    * ( p_new->i_x - i_c )
                                    + ( p_new->i_y - i_r )
                                    * ( p_new->i_y - i_r );
                i_sq_dist = __MAX(0, i_sq_dist - i_min_sq_radius);

                uint16_t i_timeout = __MAX(i_base_timeout - i_sq_dist, 0);

                /* ask to update chache for pixel to be freezed just now */
                if ( p_sys->pi_freezing_countdown[i_r][i_c] == 0 && i_timeout > 0)
                     p_sys->pb_update_cache[i_r][i_c] = true;

                /* set freezing delay */
                if ( p_sys->pi_freezing_countdown[i_r][i_c] < i_timeout )
                     p_sys->pi_freezing_countdown[i_r][i_c] = i_timeout;
            }
    }

    return VLC_EGENERIC;
}


/*
 * Allocate data
 */
static int freeze_allocate_data( filter_t *p_filter, picture_t *p_pic_in )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    freeze_free_allocated_data( p_filter );

   /*
    * take into account different characteristics for each plane
    */
    p_sys->i_planes        = p_pic_in->i_planes;
    p_sys->i_height        = calloc( p_sys->i_planes, sizeof(int32_t) );
    p_sys->i_width         = calloc( p_sys->i_planes, sizeof(int32_t) );
    p_sys->i_visible_pitch = calloc( p_sys->i_planes, sizeof(int32_t) );

    if ( unlikely( !p_sys->i_height || !p_sys->i_width || !p_sys->i_visible_pitch ) )
    {
        freeze_free_allocated_data( p_filter );
        return VLC_ENOMEM;
    }

    /* init data */
    for ( int32_t i_p = 0; i_p < p_sys->i_planes; i_p++ )
    {
        p_sys->i_visible_pitch [i_p] = (int) p_pic_in->p[i_p].i_visible_pitch;
        p_sys->i_height[i_p]         = (int) p_pic_in->p[i_p].i_visible_lines;
        p_sys->i_width[i_p]          = (int) p_pic_in->p[i_p].i_visible_pitch
                                     / p_pic_in->p[i_p].i_pixel_pitch;
    }

    /* buffer used to countdown freezing delay */
    p_sys->pi_freezing_countdown
        = calloc( p_sys->i_height[Y_PLANE], sizeof(int16_t*) );
    if ( unlikely( !p_sys->pi_freezing_countdown ) )
    {
        freeze_free_allocated_data( p_filter );
        return VLC_ENOMEM;
    }

    for ( int32_t i_r = 0; i_r < p_sys->i_height[Y_PLANE]; i_r++ )
    {
        p_sys->pi_freezing_countdown[i_r]
            = calloc( p_sys->i_width[Y_PLANE], sizeof(int16_t) );
        if ( unlikely( !p_sys->pi_freezing_countdown[i_r] ) )
        {
            freeze_free_allocated_data( p_filter );
            return VLC_ENOMEM;
        }
    }

    /* buffer used to cache freezed pixels colors */
    p_sys->pi_freezed_picture = calloc( p_sys->i_planes, sizeof(int8_t**) );
    if( unlikely( !p_sys->pi_freezed_picture ) )
    {
        freeze_free_allocated_data( p_filter );
        return VLC_ENOMEM;
    }

    for ( int32_t i_p = 0; i_p < p_sys->i_planes; i_p++)
    {
        p_sys->pi_freezed_picture[i_p]
            = calloc( p_sys->i_height[i_p], sizeof(int8_t*) );
        if ( unlikely(!p_sys->pi_freezed_picture[i_p]) )
        {
            freeze_free_allocated_data( p_filter );
            return VLC_ENOMEM;
        }
        for ( int32_t i_r = 0; i_r < p_sys->i_height[i_p]; i_r++ )
        {
            p_sys->pi_freezed_picture[i_p][i_r]
                = calloc( p_sys->i_width[i_p], sizeof(int8_t) );
            if ( unlikely( !p_sys->pi_freezed_picture[i_p][i_r] ) )
            {
                freeze_free_allocated_data( p_filter );
                return VLC_ENOMEM;
            }
        }
    }

    /* flag used to manage freezed pixels cache update */
    p_sys->pb_update_cache
        = calloc( p_sys->i_height[Y_PLANE], sizeof(bool*) );
    if( unlikely( !p_sys->pb_update_cache ) )
    {
        freeze_free_allocated_data( p_filter );
        return VLC_ENOMEM;
    }

    for ( int32_t i_r = 0; i_r < p_sys->i_height[Y_PLANE]; i_r++ )
    {
        p_sys->pb_update_cache[i_r]
            = calloc( p_sys->i_width[Y_PLANE], sizeof(bool) );
        if ( unlikely( !p_sys->pb_update_cache[i_r] ) )
        {
            freeze_free_allocated_data( p_filter );
            return VLC_ENOMEM;
        }
    }

    return VLC_SUCCESS;
}

/**
 * Free allocated data
 */
static void freeze_free_allocated_data( filter_t *p_filter ) {
    filter_sys_t *p_sys = p_filter->p_sys;

    if (p_sys->pi_freezing_countdown)
        for ( int32_t i_r = 0; i_r < p_sys->i_height[Y_PLANE]; i_r++ )
            free( p_sys->pi_freezing_countdown[i_r] );
    FREENULL( p_sys->pi_freezing_countdown );

    if ( p_sys->pb_update_cache )
        for ( int32_t i_r = 0; i_r < p_sys->i_height[Y_PLANE]; i_r++ )
            free( p_sys->pb_update_cache[i_r] );
    FREENULL( p_sys->pb_update_cache );

    if ( p_sys->pi_freezed_picture )
        for ( int32_t i_p=0; i_p < p_sys->i_planes; i_p++ ) {
            for ( int32_t i_r=0; i_r < p_sys->i_height[i_p]; i_r++ )
                free( p_sys->pi_freezed_picture[i_p][i_r] );
            free( p_sys->pi_freezed_picture[i_p] );
            }
    FREENULL( p_sys->pi_freezed_picture );

    p_sys->i_planes = 0;
    FREENULL( p_sys->i_height );
    FREENULL( p_sys->i_width );
    FREENULL( p_sys->i_visible_pitch );
}
