/*****************************************************************************
 * oldmovie.c : Old movie effect video filter
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

#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_rand.h>
#include <vlc_tick.h>

#include "filter_picture.h"

static inline int64_t MOD(int64_t a, int64_t b) {
    return ( ( a % b ) + b ) % b; }

#define SUB_MIN(val, sub_val, min) val = \
        ((val-(int32_t)sub_val)<min?min:val-sub_val)
#define ADD_MAX(val, add_val, max) val = \
        ((val+(int32_t)add_val)>max?max:val+add_val)

static inline int32_t PIX_OFS(int32_t i_x, int32_t i_y, plane_t *ps_plane) {
    return i_x * ps_plane->i_pixel_pitch + i_y * ps_plane->i_pitch; }

#define CHECK_PIX_OFS(i_x, i_y, ps_plane) ( \
        (i_x) >= 0 && (i_y) >= 0 && \
        (i_x) * ps_plane->i_pixel_pitch < ps_plane->i_visible_pitch && \
        (i_y) < ps_plane->i_visible_lines \
    )

static inline void DARKEN_PIXEL(int32_t i_x, int32_t i_y,
    int16_t intensity, plane_t *ps_plane) {
    SUB_MIN( ps_plane->p_pixels[ PIX_OFS(i_x, i_y, ps_plane) ],
        intensity, 0 );
}

static inline void LIGHTEN_PIXEL(int32_t i_x, int32_t i_y,
                                int16_t intensity, plane_t *ps_plane) {
    ADD_MAX( ps_plane->p_pixels[ PIX_OFS(i_x, i_y, ps_plane) ],
        intensity, 0xFF );
}

static inline void CHECK_N_DARKEN_PIXEL(int32_t i_x, int32_t i_y,
                                int16_t intensity, plane_t *ps_plane) {
    if ( likely( CHECK_PIX_OFS(i_x, i_y, ps_plane) ) )
        DARKEN_PIXEL(i_x, i_y, intensity, ps_plane);
}

static inline void CHECK_N_LIGHTEN_PIXEL(int32_t i_x, int32_t i_y,
                                int16_t intensity, plane_t *ps_plane) {
    if ( likely( CHECK_PIX_OFS(i_x, i_y, ps_plane) ) )
        LIGHTEN_PIXEL(i_x, i_y, intensity, ps_plane);
}

#define MAX_SCRATCH        20
#define MAX_HAIR           10
#define MAX_DUST           10

typedef struct {
    int32_t  i_offset;
    int32_t  i_width;
    uint16_t i_intensity;
    vlc_tick_t  i_stop_trigger;
} scratch_t;

typedef struct {
    int32_t  i_x, i_y;
    uint8_t  i_rotation;
    int32_t  i_width;
    int32_t  i_length;
    int32_t  i_curve;
    uint16_t i_intensity;
    vlc_tick_t  i_stop_trigger;
} hair_t;

typedef struct {
    int32_t  i_x, i_y;
    int32_t  i_width;
    uint16_t i_intensity;
    vlc_tick_t  i_stop_trigger;
} dust_t;

typedef struct
{

    /* general data */
    bool b_init;
    size_t   i_planes;
    int32_t *i_height;
    int32_t *i_width;
    int32_t *i_visible_pitch;
    vlc_tick_t i_start_time;
    vlc_tick_t i_last_time;
    vlc_tick_t i_cur_time;

    /* sliding & offset effect */
    vlc_tick_t  i_offset_trigger;
    vlc_tick_t  i_sliding_trigger;
    vlc_tick_t  i_sliding_stop_trig;
    int32_t  i_offset_ofs;
    int32_t  i_sliding_ofs;
    int32_t  i_sliding_speed;

    /* scratch on film */
    vlc_tick_t i_scratch_trigger;
    scratch_t *p_scratch[MAX_SCRATCH];

    /* hair on lens */
    vlc_tick_t i_hair_trigger;
    hair_t    *p_hair[MAX_HAIR];

    /* blotch on film */
    vlc_tick_t i_blotch_trigger;

    /* dust on lens */
    vlc_tick_t i_dust_trigger;
    dust_t    *p_dust[MAX_DUST];
} filter_sys_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

static picture_t *Filter( filter_t *, picture_t * );

static int  oldmovie_allocate_data( filter_t *, picture_t * );
static void oldmovie_free_allocated_data( filter_t * );

static void oldmovie_shutter_effect( filter_t *, picture_t * );
static int  oldmovie_sliding_offset_effect( filter_t *, picture_t * );
static void oldmovie_black_n_white_effect( picture_t * );
static int  oldmovie_dark_border_effect( filter_t *, picture_t * );
static int  oldmovie_film_scratch_effect( filter_t *, picture_t * );
static void oldmovie_film_blotch_effect( filter_t *, picture_t * );
static void oldmovie_film_dust_effect( filter_t *, picture_t * );
static int  oldmovie_lens_hair_effect( filter_t *, picture_t * );
static int  oldmovie_lens_dust_effect( filter_t *, picture_t * );

static void oldmovie_define_hair_location( filter_t *p_filter, hair_t* ps_hair );
static void oldmovie_define_dust_location( filter_t *p_filter, dust_t* ps_dust );
static int  oldmovie_sliding_offset_apply( filter_t *p_filter, picture_t *p_pic_out );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin()
    set_description( N_("Old movie effect video filter") )
    set_shortname( N_( "Old movie" ))
    set_capability( "video filter", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    set_callbacks( Open, Close )
vlc_module_end()

/**
 * Open the filter
 */
static int Open( vlc_object_t *p_this ) {
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    /* Assert video in match with video out */
    if( !es_format_IsSimilar( &p_filter->fmt_in, &p_filter->fmt_out ) ) {
        msg_Err( p_filter, "Input and output format does not match" );
        return VLC_EGENERIC;
    }

    /* Reject 0 bpp and unsupported chroma */
    const vlc_fourcc_t fourcc = p_filter->fmt_in.video.i_chroma;
    const vlc_chroma_description_t *p_chroma =
        vlc_fourcc_GetChromaDescription( p_filter->fmt_in.video.i_chroma );
    if( !p_chroma || p_chroma->pixel_size == 0
        || p_chroma->plane_count < 3 || p_chroma->pixel_size > 1
        || !vlc_fourcc_IsYUV( fourcc ) ) {

        msg_Err( p_filter, "Unsupported chroma (%4.4s)", (char*)&fourcc );
        return VLC_EGENERIC;
    }

    /* Allocate structure */
    p_filter->p_sys = p_sys = calloc( 1, sizeof(*p_sys) );
    if( unlikely( !p_sys ) )
        return VLC_ENOMEM;

    /* init data */
    p_filter->pf_video_filter = Filter;
    p_sys->i_start_time = p_sys->i_cur_time = p_sys->i_last_time = vlc_tick_now();

    return VLC_SUCCESS;
}

/**
 * Close the filter
 */
static void Close( vlc_object_t *p_this ) {
    filter_t *p_filter  = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    /* Free allocated memory */
    oldmovie_free_allocated_data( p_filter );
    free( p_sys );
}

/**
 * Filter a picture
 */
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic_in ) {
    if( unlikely( !p_pic_in || !p_filter ) )
        return NULL;

    filter_sys_t *p_sys = p_filter->p_sys;

    picture_t *p_pic_out = filter_NewPicture( p_filter );
    if( unlikely( !p_pic_out ) ) {
        picture_Release( p_pic_in );
        return NULL;
    }

   /*
    * manage time
    */
    p_sys->i_last_time = p_sys->i_cur_time;
    p_sys->i_cur_time  = vlc_tick_now();

   /*
    * allocate data
    */
    if ( unlikely( !p_sys->b_init ) )
        if ( unlikely( oldmovie_allocate_data( p_filter, p_pic_in ) != VLC_SUCCESS ) ) {
            picture_Release( p_pic_in );
            return NULL;
        }
    p_sys->b_init = true;

   /*
    * preset output pic: raw copy src to dst
    */
    picture_CopyPixels(p_pic_out, p_pic_in);

   /*
    * apply several effects on picture
    */
    oldmovie_black_n_white_effect( p_pic_out );

    /* simulates projector shutter blinking effect */
    oldmovie_shutter_effect(p_filter, p_pic_out);

    if ( unlikely( oldmovie_sliding_offset_effect( p_filter, p_pic_out ) != VLC_SUCCESS ) )
        return CopyInfoAndRelease( p_pic_out, p_pic_in );

    oldmovie_dark_border_effect( p_filter, p_pic_out );

    if ( unlikely( oldmovie_film_scratch_effect(p_filter, p_pic_out) != VLC_SUCCESS ) )
        return CopyInfoAndRelease( p_pic_out, p_pic_in );

    oldmovie_film_blotch_effect(p_filter, p_pic_out);

    if ( unlikely( oldmovie_lens_hair_effect( p_filter, p_pic_out ) != VLC_SUCCESS ) )
        return CopyInfoAndRelease( p_pic_out, p_pic_in );

    if ( unlikely( oldmovie_lens_dust_effect( p_filter, p_pic_out ) != VLC_SUCCESS ) )
        return CopyInfoAndRelease( p_pic_out, p_pic_in );

    oldmovie_film_dust_effect( p_filter, p_pic_out );

    return CopyInfoAndRelease( p_pic_out, p_pic_in );
}

/*
 * Allocate data
 */
static int oldmovie_allocate_data( filter_t *p_filter, picture_t *p_pic_in ) {
    filter_sys_t *p_sys = p_filter->p_sys;

    oldmovie_free_allocated_data( p_filter );

   /*
    * take into account different characteristics for each plane
    */
    p_sys->i_planes = p_pic_in->i_planes;
    p_sys->i_height = calloc( p_sys->i_planes, sizeof(int32_t) );
    p_sys->i_width  = calloc( p_sys->i_planes, sizeof(int32_t) );
    p_sys->i_visible_pitch = calloc( p_sys->i_planes, sizeof(int32_t) );

    if( unlikely( !p_sys->i_height || !p_sys->i_width || !p_sys->i_visible_pitch ) ) {
        oldmovie_free_allocated_data( p_filter );
        return VLC_ENOMEM;
    }

    for (size_t i_p=0; i_p < p_sys->i_planes; i_p++) {
        p_sys->i_visible_pitch [i_p] = (int) p_pic_in->p[i_p].i_visible_pitch;
        p_sys->i_height[i_p]         = (int) p_pic_in->p[i_p].i_visible_lines;
        p_sys->i_width [i_p]         = (int) p_pic_in->p[i_p].i_visible_pitch
                                     / p_pic_in->p[i_p].i_pixel_pitch;
    }
    return VLC_SUCCESS;
}

/**
 * Free allocated data
 */
static void oldmovie_free_allocated_data( filter_t *p_filter ) {
    filter_sys_t *p_sys = p_filter->p_sys;

    for ( size_t i_s = 0; i_s < MAX_SCRATCH; i_s++ )
        FREENULL(p_sys->p_scratch[i_s]);

    for ( size_t i_h = 0; i_h < MAX_HAIR; i_h++ )
        FREENULL(p_sys->p_hair[i_h]);

    for ( size_t i_d = 0; i_d < MAX_DUST; i_d++ )
        FREENULL(p_sys->p_dust[i_d]);

    p_sys->i_planes = 0;
    FREENULL( p_sys->i_height );
    FREENULL( p_sys->i_width );
    FREENULL( p_sys->i_visible_pitch );
}

/**
 * Projector shutter effect
 */
static void oldmovie_shutter_effect( filter_t *p_filter, picture_t *p_pic_out ) {
    filter_sys_t *p_sys = p_filter->p_sys;

#define SHUTTER_FREQ      2
#define SHUTTER_SPEED     25
#define SHUTTER_HEIGHT    1.5

#define SHUTTER_INTENSITY 50

#define SUB_FRAME (p_sys->i_cur_time % (CLOCK_FREQ / SHUTTER_FREQ))

   /*
    * depending on current time: define shutter location on picture
    */
    int32_t i_shutter_sup = VLC_CLIP((int64_t)SUB_FRAME
                                      * p_pic_out->p[Y_PLANE].i_visible_lines
                                      * SHUTTER_SPEED / CLOCK_FREQ,
                                      0, p_pic_out->p[Y_PLANE].i_visible_lines);

    int32_t i_shutter_inf = VLC_CLIP((int64_t)SUB_FRAME
                                      * p_pic_out->p[Y_PLANE].i_visible_lines
                                      * SHUTTER_SPEED / CLOCK_FREQ
                                      - SHUTTER_HEIGHT * p_pic_out->p[Y_PLANE].i_visible_lines,
                                      0, p_pic_out->p[Y_PLANE].i_visible_lines);

    int32_t i_width = p_pic_out->p[Y_PLANE].i_visible_pitch
                    / p_pic_out->p[Y_PLANE].i_pixel_pitch;

   /*
    * darken pixels currently occulted by shutter
    */
    for ( int32_t i_y = i_shutter_inf; i_y < i_shutter_sup; i_y++ )
        for ( int32_t i_x = 0; i_x < i_width; i_x++ )
            DARKEN_PIXEL( i_x, i_y, SHUTTER_INTENSITY, &p_pic_out->p[Y_PLANE] );
}

static vlc_tick_t RandomEnd(filter_sys_t *p_sys, vlc_tick_t modulo)
{
    return p_sys->i_cur_time + (uint64_t)vlc_mrand48() % modulo + modulo / 2;
}

/**
 * sliding & offset effect
 */
static int oldmovie_sliding_offset_effect( filter_t *p_filter, picture_t *p_pic_out ) {
    filter_sys_t *p_sys = p_filter->p_sys;


   /**
    * one shot offset section
    */

#define OFFSET_AVERAGE_PERIOD   VLC_TICK_FROM_SEC(10)

    /* start trigger to be (re)initialized */
    if ( p_sys->i_offset_trigger == 0
         || p_sys->i_sliding_speed != 0 ) { /* do not mix sliding and offset */
        /* random trigger for offset effect */
        p_sys->i_offset_trigger = RandomEnd(p_sys, OFFSET_AVERAGE_PERIOD);
        p_sys->i_offset_ofs = 0;
    } else if ( p_sys->i_offset_trigger <= p_sys->i_cur_time ) {
        /* trigger for offset effect */
        p_sys->i_offset_trigger = 0;
        p_sys->i_offset_ofs = MOD( ( (uint32_t)vlc_mrand48() ),
                                  p_sys->i_height[Y_PLANE] ) * 100;
    } else
        p_sys->i_offset_ofs = 0;


    /**
    * sliding section
    */

#define SLIDING_AVERAGE_PERIOD   VLC_TICK_FROM_SEC(20)
#define SLIDING_AVERAGE_DURATION VLC_TICK_FROM_SEC(3)

    /* start trigger to be (re)initialized */
    if (    ( p_sys->i_sliding_stop_trig  == 0 )
         && ( p_sys->i_sliding_trigger    == 0 )
         && ( p_sys->i_sliding_speed      == 0 ) ) {
        /* random trigger which enable sliding effect */
        p_sys->i_sliding_trigger = RandomEnd(p_sys, SLIDING_AVERAGE_PERIOD);
    }
    /* start trigger just occurs */
    else if (    ( p_sys->i_sliding_stop_trig  == 0 )
              && ( p_sys->i_sliding_trigger    <= p_sys->i_cur_time )
              && ( p_sys->i_sliding_speed      == 0 ) ) {
        /* init sliding parameters */
        p_sys->i_sliding_trigger   = 0;
        p_sys->i_sliding_stop_trig = RandomEnd(p_sys, SLIDING_AVERAGE_DURATION);
        p_sys->i_sliding_ofs = 0;
        /* note: sliding speed unit = image per 100 s */
        p_sys->i_sliding_speed = MOD(((int32_t) vlc_mrand48() ), 201) - 100;
    }
    /* stop trigger disabling sliding effect */
    else if (   ( p_sys->i_sliding_stop_trig  <= p_sys->i_cur_time )
             && ( p_sys->i_sliding_trigger    == 0 ) ) {

        /* first increase speed to ensure we will come back to stable image */
        if ( abs(p_sys->i_sliding_speed) < 50 )
            p_sys->i_sliding_speed += 5;

        long long i_position = p_sys->i_sliding_speed
             * p_sys->i_height[Y_PLANE]
             * SEC_FROM_VLC_TICK( p_sys->i_cur_time - p_sys->i_last_time );

        /* check if offset is close to 0 and then ready to stop */
        if ( abs( p_sys->i_sliding_ofs ) < llabs( i_position )
             ||  abs( p_sys->i_sliding_ofs ) < p_sys->i_height[Y_PLANE] * 100 / 20 ) {

            /* reset sliding parameters */
            p_sys->i_sliding_ofs     = p_sys->i_sliding_speed = 0;
            p_sys->i_sliding_trigger = p_sys->i_sliding_stop_trig = 0;
        }
    }

    /* update offset */
    p_sys->i_sliding_ofs += p_sys->i_sliding_speed * p_sys->i_height[Y_PLANE]
                      * SEC_FROM_VLC_TICK( p_sys->i_cur_time - p_sys->i_last_time );

    p_sys->i_sliding_ofs = MOD( p_sys->i_sliding_ofs,
                                p_sys->i_height[Y_PLANE] * 100 );

    /* apply offset */
    return oldmovie_sliding_offset_apply( p_filter, p_pic_out );
}

/**
* apply both sliding and offset effect
*/
static int oldmovie_sliding_offset_apply( filter_t *p_filter, picture_t *p_pic_out )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    assert(p_pic_out->i_planes > 0);
    size_t i_planes = p_pic_out->i_planes;

    for ( size_t i_p = 0; i_p < i_planes; i_p++ ) {
        /* first allocate temporary buffer for swap operation */
        uint8_t *p_temp_buf = calloc( p_pic_out->p[i_p].i_lines * p_pic_out->p[i_p].i_pitch,
                                      sizeof(uint8_t) );
        if ( unlikely( !p_temp_buf ) )
            return VLC_ENOMEM;
        memcpy( p_temp_buf,p_pic_out->p[i_p].p_pixels,
                p_pic_out->p[i_p].i_lines * p_pic_out->p[i_p].i_pitch );

        /* copy lines to output_pic */
        assert(p_pic_out->p[i_p].i_visible_lines > 0);
        size_t i_visible_lines = p_pic_out->p[i_p].i_visible_lines;
        for ( size_t i_y = 0; i_y < i_visible_lines; i_y++ ) {
            size_t i_ofs = MOD( ( p_sys->i_offset_ofs + p_sys->i_sliding_ofs )
                                 /100,
                                 p_sys->i_height[Y_PLANE] );
            i_ofs *= i_visible_lines;
            i_ofs /= p_sys->i_height[Y_PLANE];

            memcpy( &p_pic_out->p[i_p].p_pixels[ i_y * p_pic_out->p[i_p].i_pitch ],
                    &p_temp_buf[ ( ( i_y + i_ofs ) % i_visible_lines ) * p_pic_out->p[i_p].i_pitch ],
                    p_pic_out->p[i_p].i_visible_pitch);
        }
        free( p_temp_buf );
    }

    return VLC_SUCCESS;
}

/**
 * Black and white transform including a touch of sepia effect
 */
static void oldmovie_black_n_white_effect( picture_t *p_pic_out )
{
    assert(p_pic_out->p[Y_PLANE].i_visible_lines > 0);
    assert(p_pic_out->p[Y_PLANE].i_visible_pitch > 0);
    size_t i_visible_lines = p_pic_out->p[Y_PLANE].i_visible_lines;
    size_t i_visible_pitch = p_pic_out->p[Y_PLANE].i_visible_pitch;
    size_t i_pixel_pitch   = p_pic_out->p[Y_PLANE].i_pixel_pitch;

    for ( size_t i_y = 0; i_y < i_visible_lines; i_y++ )
        for ( size_t i_x = 0; i_x < i_visible_pitch; i_x += i_pixel_pitch ) {
            size_t i_pix_ofs = i_x + i_y * p_pic_out->p[Y_PLANE].i_pitch;
            p_pic_out->p[Y_PLANE].p_pixels[i_pix_ofs] -= p_pic_out->p[Y_PLANE].p_pixels[i_pix_ofs] >> 2;
            p_pic_out->p[Y_PLANE].p_pixels[i_pix_ofs] += 30;
        }

    memset( p_pic_out->p[U_PLANE].p_pixels, 122,
            p_pic_out->p[U_PLANE].i_lines * p_pic_out->p[U_PLANE].i_pitch );
    memset( p_pic_out->p[V_PLANE].p_pixels, 132,
            p_pic_out->p[V_PLANE].i_lines * p_pic_out->p[V_PLANE].i_pitch );
}

/**
 * Smooth darker borders effect
 */
static int oldmovie_dark_border_effect( filter_t *p_filter, picture_t *p_pic_out )
{
    filter_sys_t *p_sys = p_filter->p_sys;

#define BORDER_DIST 20

    for ( int32_t i_y = 0; i_y < p_sys->i_height[Y_PLANE]; i_y++ )
        for ( int32_t i_x = 0; i_x < p_sys->i_width[Y_PLANE]; i_x++ ) {

            int32_t i_x_border_dist = __MIN( i_x, p_sys->i_width[Y_PLANE] - i_x);
            int32_t i_y_border_dist = __MIN( i_y, p_sys->i_height[Y_PLANE] - i_y);

            int32_t i_border_dist = __MAX(BORDER_DIST - i_x_border_dist,0)
                                  + __MAX(BORDER_DIST - i_y_border_dist,0);

            i_border_dist = __MIN(BORDER_DIST, i_border_dist);

            if ( i_border_dist == 0 )
                continue;

            uint32_t i_pix_ofs = i_x * p_pic_out->p[Y_PLANE].i_pixel_pitch
                               + i_y * p_pic_out->p[Y_PLANE].i_pitch;

            SUB_MIN( p_pic_out->p[Y_PLANE].p_pixels[i_pix_ofs],
                     i_border_dist * 255 / BORDER_DIST, 0 );
        }

    return VLC_SUCCESS;
}

/**
 * Vertical scratch random management and effect
 */
static int oldmovie_film_scratch_effect( filter_t *p_filter, picture_t *p_pic_out )
{
    filter_sys_t *p_sys = p_filter->p_sys;

#define SCRATCH_GENERATOR_PERIOD VLC_TICK_FROM_SEC(2)
#define SCRATCH_DURATION         VLC_TICK_FROM_MS(500)

    /* generate new scratch */
    if ( p_sys->i_scratch_trigger <= p_sys->i_cur_time ) {
        for ( uint32_t i_s = 0; i_s < MAX_SCRATCH; i_s++ )
            if ( p_sys->p_scratch[i_s] == NULL ) {
                /* allocate data */
                p_sys->p_scratch[i_s] = calloc( 1, sizeof(scratch_t) );
                if ( unlikely( !p_sys->p_scratch[i_s] ) )
                    return VLC_ENOMEM;

                /* set random parameters */
                p_sys->p_scratch[i_s]->i_offset = ( ( (unsigned)vlc_mrand48() )
                                                % __MAX( p_sys->i_width[Y_PLANE] - 10, 1 ) )
                                                + 5;
                p_sys->p_scratch[i_s]->i_width  = ( ( (unsigned)vlc_mrand48() )
                                                % __MAX( p_sys->i_width[Y_PLANE] / 500, 1 ) )
                                                + 1;
                p_sys->p_scratch[i_s]->i_intensity = (unsigned) vlc_mrand48() % 50 + 10;
                p_sys->p_scratch[i_s]->i_stop_trigger = RandomEnd(p_sys, SCRATCH_DURATION);

                break;
            }
        p_sys->i_scratch_trigger = RandomEnd(p_sys, SCRATCH_GENERATOR_PERIOD);
    }

    /* manage and apply current scratch */
    for ( uint32_t i_s = 0; i_s < MAX_SCRATCH; i_s++ )
        if ( p_sys->p_scratch[i_s] ) {
            /* remove outdated scratch */
            if ( p_sys->p_scratch[i_s]->i_stop_trigger <= p_sys->i_cur_time ) {
                FREENULL( p_sys->p_scratch[i_s] );
                continue;
            }

            /* otherwise apply scratch */
            for ( int32_t i_y = 0; i_y < p_pic_out->p[Y_PLANE].i_visible_lines; i_y++ )
                for ( int32_t i_x = p_sys->p_scratch[i_s]->i_offset;
                      i_x < __MIN(p_sys->p_scratch[i_s]->i_offset
                      + p_sys->p_scratch[i_s]->i_width, p_sys->i_width[Y_PLANE] );
                      i_x++ )
                    DARKEN_PIXEL( i_x, i_y, p_sys->p_scratch[i_s]->i_intensity,
                                  &p_pic_out->p[Y_PLANE] );
        }

    return VLC_SUCCESS;
}

/**
 * Blotch addition
 *    bigger than dust but only during one frame (due to a local film damage)
 */
static void oldmovie_film_blotch_effect( filter_t *p_filter, picture_t *p_pic_out )
{
    filter_sys_t *p_sys = p_filter->p_sys;

#define BLOTCH_GENERATOR_PERIOD VLC_TICK_FROM_SEC(5)

    /* generate blotch */
    if ( p_sys->i_blotch_trigger <= p_sys->i_cur_time ) {
        /* set random parameters */
        int32_t i_bx =        (unsigned)vlc_mrand48() % p_sys->i_width[Y_PLANE];
        int32_t i_by =        (unsigned)vlc_mrand48() % p_sys->i_height[Y_PLANE];
        int32_t i_width =     (unsigned)vlc_mrand48() % __MAX( 1, p_sys->i_width[Y_PLANE] / 10 ) + 1;
        int32_t i_intensity = (unsigned)vlc_mrand48() % 50 + 20;

        if ( (unsigned)vlc_mrand48() & 0x01 ) {
            /* draw dark blotch */
            for ( int32_t i_y = -i_width + 1; i_y < i_width; i_y++ )
                for ( int32_t i_x = -i_width + 1; i_x < i_width; i_x++ )
                    if ( i_x * i_x + i_y * i_y <= i_width * i_width )
                        CHECK_N_DARKEN_PIXEL( i_x + i_bx, i_y + i_by,
                                              i_intensity, &p_pic_out->p[Y_PLANE] );
        } else {
            /* draw light blotch */
            for ( int32_t i_y = -i_width+1; i_y < i_width; i_y++ )
                for ( int32_t i_x = -i_width+1; i_x < i_width; i_x++ )
                    if ( i_x * i_x + i_y * i_y <= i_width * i_width )
                        CHECK_N_LIGHTEN_PIXEL( i_x + i_bx, i_y + i_by,
                                               i_intensity, &p_pic_out->p[Y_PLANE] );
        }

        p_sys->i_blotch_trigger = RandomEnd(p_sys, BLOTCH_GENERATOR_PERIOD);
    }
}

/**
 * Dust dots addition, visible during one frame only (film damages)
 */
static void oldmovie_film_dust_effect( filter_t *p_filter, picture_t *p_pic_out ) {
#define ONESHOT_DUST_RATIO 1000

    filter_sys_t *p_sys = p_filter->p_sys;

    for ( int32_t i_dust = 0;
          i_dust < p_sys->i_width[Y_PLANE] * p_sys->i_height[Y_PLANE] / ONESHOT_DUST_RATIO;
          i_dust++)
        if ( (unsigned)vlc_mrand48() % 5 < 3 )
            DARKEN_PIXEL( (unsigned)vlc_mrand48() % p_sys->i_width[Y_PLANE],
                          (unsigned)vlc_mrand48() % p_sys->i_height[Y_PLANE],
                          150, &p_pic_out->p[Y_PLANE] );
        else
            LIGHTEN_PIXEL( (unsigned)vlc_mrand48() % p_sys->i_width[Y_PLANE],
                           (unsigned)vlc_mrand48() % p_sys->i_height[Y_PLANE],
                           50, &p_pic_out->p[Y_PLANE] );
}

/**
 * Hair and dust on projector lens
 *
 */
#define HAIR_GENERATOR_PERIOD VLC_TICK_FROM_SEC(50)
#define HAIR_DURATION         VLC_TICK_FROM_SEC(50)
#define DUST_GENERATOR_PERIOD VLC_TICK_FROM_SEC(100)
#define DUST_DURATION         VLC_TICK_FROM_SEC(4)

/**
 * Define hair location on the lens and timeout
 *
 */
static void oldmovie_define_hair_location( filter_t *p_filter, hair_t* ps_hair ) {
    filter_sys_t *p_sys = p_filter->p_sys;

    ps_hair->i_x = (unsigned)vlc_mrand48() % p_sys->i_width[Y_PLANE];
    ps_hair->i_y = (unsigned)vlc_mrand48() % p_sys->i_height[Y_PLANE];
    ps_hair->i_rotation = (unsigned)vlc_mrand48() % 200;

    ps_hair->i_stop_trigger = RandomEnd(p_sys, HAIR_DURATION);
}

/**
 * Show black hair on the screen
 *       after random duration it is removed or re-located
 */
static int oldmovie_lens_hair_effect( filter_t *p_filter, picture_t *p_pic_out ) {
    filter_sys_t *p_sys = p_filter->p_sys;

    /* generate new hair */
    if ( p_sys->i_hair_trigger <= p_sys->i_cur_time ) {
        for ( uint32_t i_h = 0; i_h < MAX_HAIR; i_h++ )
            if ( p_sys->p_hair[i_h] == NULL ) {
                /* allocate data */
                p_sys->p_hair[i_h] = calloc( 1, sizeof(hair_t) );
                if ( unlikely( !p_sys->p_hair[i_h] ) )
                    return VLC_ENOMEM;

                /* set random parameters */
                p_sys->p_hair[i_h]->i_length = (unsigned)vlc_mrand48()
                                             % ( p_sys->i_width[Y_PLANE] / 3 ) + 5;
                p_sys->p_hair[i_h]->i_curve  = MOD( (int32_t)vlc_mrand48(), 80 ) - 40;
                p_sys->p_hair[i_h]->i_width  = (unsigned)vlc_mrand48()
                                             % __MAX( 1, p_sys->i_width[Y_PLANE] / 1500 ) + 1;
                p_sys->p_hair[i_h]->i_intensity = (unsigned)vlc_mrand48() % 50 + 20;

                oldmovie_define_hair_location( p_filter, p_sys->p_hair[i_h] );

                break;
            }
        p_sys->i_hair_trigger = RandomEnd(p_sys, HAIR_GENERATOR_PERIOD);
    }

    /* manage and apply current hair */
    for ( uint32_t i_h = 0; i_h < MAX_HAIR; i_h++ )
        if ( p_sys->p_hair[i_h] ) {
            /* remove outdated ones */
            if ( p_sys->p_hair[i_h]->i_stop_trigger <= p_sys->i_cur_time ) {
                /* select between moving or removing hair */
                if ( (unsigned)vlc_mrand48() % 2 == 0 )
                    /* move hair */
                    oldmovie_define_hair_location( p_filter, p_sys->p_hair[i_h] );
                else {
                    /* remove hair */
                    FREENULL( p_sys->p_hair[i_h] );
                    continue;
                }
            }

            /* draw hair */
            double  f_base_x   = (double)p_sys->p_hair[i_h]->i_x;
            double  f_base_y   = (double)p_sys->p_hair[i_h]->i_y;

            for ( int32_t i_l = 0; i_l < p_sys->p_hair[i_h]->i_length; i_l++ ) {
                uint32_t i_current_rot = p_sys->p_hair[i_h]->i_rotation
                                       + p_sys->p_hair[i_h]->i_curve * i_l / 100;
                f_base_x += cos( (double)i_current_rot / 128.0 * M_PI );
                f_base_y += sin( (double)i_current_rot / 128.0 * M_PI );
                double f_current_x = f_base_x;
                double f_current_y = f_base_y;
                for ( int32_t i_w = 0; i_w < p_sys->p_hair[i_h]->i_width; i_w++ ) {
                    f_current_x += sin( (double)i_current_rot / 128.0 * M_PI );
                    f_current_y += cos( (double)i_current_rot / 128.0 * M_PI );
                    CHECK_N_DARKEN_PIXEL( (int32_t) f_current_x,
                                          (int32_t) f_current_y,
                                          p_sys->p_hair[i_h]->i_intensity,
                                          &p_pic_out->p[Y_PLANE] );
                }
            }
        }

    return VLC_SUCCESS;
}

/**
 * Define dust location on the lens and timeout
 *
 */
static void oldmovie_define_dust_location( filter_t *p_filter, dust_t* ps_dust ) {
    filter_sys_t *p_sys = p_filter->p_sys;

    ps_dust->i_x = (unsigned)vlc_mrand48() % p_sys->i_width[Y_PLANE];
    ps_dust->i_y = (unsigned)vlc_mrand48() % p_sys->i_height[Y_PLANE];

    ps_dust->i_stop_trigger = RandomEnd(p_sys, HAIR_DURATION);


    ps_dust->i_x = MOD( (int32_t)vlc_mrand48(), p_sys->i_width[Y_PLANE]  );
    ps_dust->i_y = MOD( (int32_t)vlc_mrand48(), p_sys->i_height[Y_PLANE] );

    ps_dust->i_stop_trigger = RandomEnd(p_sys, DUST_DURATION);
}

/**
 * Dust addition
 *    smaller than blotch but will remain on the screen for long time
 */
static int oldmovie_lens_dust_effect( filter_t *p_filter, picture_t *p_pic_out ) {
    filter_sys_t *p_sys = p_filter->p_sys;

    /* generate new dust */
    if ( p_sys->i_dust_trigger <= p_sys->i_cur_time ) {
        for ( uint32_t i_d = 0; i_d < MAX_DUST; i_d++ )
            if ( p_sys->p_dust[i_d] == NULL ) {
                /* allocate data */
                p_sys->p_dust[i_d] = calloc( 1, sizeof(dust_t) );
                if ( unlikely( !p_sys->p_dust[i_d] ) )
                    return VLC_ENOMEM;

                /* set random parameters */
                oldmovie_define_dust_location( p_filter, p_sys->p_dust[i_d] );
                p_sys->p_dust[i_d]->i_width = MOD( (int32_t)vlc_mrand48(), 5 ) + 1;
                p_sys->p_dust[i_d]->i_intensity = (unsigned)vlc_mrand48() % 30 + 30;

                break;
            }
        p_sys->i_dust_trigger = RandomEnd(p_sys, DUST_GENERATOR_PERIOD);
    }

    /* manage and apply current dust */
    for ( uint32_t i_d = 0; i_d < MAX_DUST; i_d++ )
        if ( p_sys->p_dust[i_d] ) {
            /* remove outdated ones */
            if ( p_sys->p_dust[i_d]->i_stop_trigger <= p_sys->i_cur_time ) {
                /* select between moving or removing dust */
                if ( (unsigned)vlc_mrand48() % 2 == 0 )
                    /* move dust */
                    oldmovie_define_dust_location( p_filter, p_sys->p_dust[i_d] );
                else {
                    /* remove dust */
                    FREENULL( p_sys->p_dust[i_d] );
                    continue;
                }
            }

            /* draw dust */
            for ( int32_t i_y = -p_sys->p_dust[i_d]->i_width + 1; i_y < p_sys->p_dust[i_d]->i_width; i_y++ )
                for ( int32_t i_x = -p_sys->p_dust[i_d]->i_width + 1; i_x < p_sys->p_dust[i_d]->i_width; i_x++ )
                    if ( i_x * i_x + i_y * i_y <= p_sys->p_dust[i_d]->i_width * p_sys->p_dust[i_d]->i_width )
                        CHECK_N_DARKEN_PIXEL( i_x + p_sys->p_dust[i_d]->i_x,
                                              i_y + p_sys->p_dust[i_d]->i_y,
                                              p_sys->p_dust[i_d]->i_intensity,
                                              &p_pic_out->p[Y_PLANE] );
        }

    return VLC_SUCCESS;
}
