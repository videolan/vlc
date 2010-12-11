/*****************************************************************************
 * sepia.c : Sepia video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2010 the VideoLAN team
 * $Id$
 *
 * Authors: Branko Kokanovic <branko.kokanovic@gmail.com>
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

#include <assert.h>
#include <vlc_filter.h>
#include "filter_picture.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create      ( vlc_object_t * );
static void Destroy     ( vlc_object_t * );

static void RVSepia( picture_t *, picture_t *, int );
static void PlanarI420Sepia( picture_t *, picture_t *, int);
static void PackedYUVSepia( picture_t *, picture_t *, int);
static void YuvSepia2( uint8_t *, uint8_t *, uint8_t *, uint8_t *,
                      const uint8_t, const uint8_t, const uint8_t, const uint8_t,
                      int );
static void YuvSepia4( uint8_t *, uint8_t *, uint8_t *, uint8_t *, uint8_t *,
                      uint8_t *, const uint8_t, const uint8_t, const uint8_t,
                      const uint8_t, const uint8_t, const uint8_t, int );
static void Sepia( int *, int *, int *, int );
static picture_t *Filter( filter_t *, picture_t * );

static const char *const ppsz_filter_options[] = {
    "intensity", NULL
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define SEPIA_INTENSITY_TEXT N_("Sepia intensity")
#define SEPIA_INTENSITY_LONGTEXT N_("Intensity of sepia effect" )

#define CFG_PREFIX "sepia-"

vlc_module_begin ()
    set_description( N_("Sepia video filter") )
    set_shortname( N_("Sepia" ) )
    set_help( N_("Gives video a warmer tone by applying sepia effect") )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_capability( "video filter2", 0 )
    add_integer_with_range( CFG_PREFIX "intensity", 30, 0, 255, NULL,
                           SEPIA_INTENSITY_TEXT, SEPIA_INTENSITY_LONGTEXT,
                           false )
    set_callbacks( Create, Destroy )
vlc_module_end ()

/*****************************************************************************
 * callback prototypes
 *****************************************************************************/
static int FilterCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

typedef void (*SepiaFunction)( picture_t *, picture_t *, int );

static const struct
{
    vlc_fourcc_t i_chroma;
    SepiaFunction pf_sepia;
} p_sepia_cfg[] = {
    { VLC_CODEC_I420, PlanarI420Sepia },
    { VLC_CODEC_RGB24, RVSepia },
    { VLC_CODEC_RGB32, RVSepia },
    { VLC_CODEC_UYVY, PackedYUVSepia },
    { VLC_CODEC_VYUY, PackedYUVSepia },
    { VLC_CODEC_YUYV, PackedYUVSepia },
    { VLC_CODEC_YVYU, PackedYUVSepia },
    { 0, NULL }
};

/*****************************************************************************
 * filter_sys_t: adjust filter method descriptor
 *****************************************************************************/
struct filter_sys_t
{
    SepiaFunction pf_sepia;
    int i_intensity;
    vlc_spinlock_t lock;
};

/*****************************************************************************
 * Create: allocates Sepia video thread output method
 *****************************************************************************
 * This function allocates and initializes a Sepia vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;

    p_sys->pf_sepia = NULL;

    for( int i = 0; p_sepia_cfg[i].i_chroma != 0; i++ )
    {
        if( p_sepia_cfg[i].i_chroma != p_filter->fmt_in.video.i_chroma )
            continue;
        p_sys->pf_sepia = p_sepia_cfg[i].pf_sepia;
    }

    if( p_sys->pf_sepia == NULL )
    {
        msg_Err( p_filter, "Unsupported input chroma (%4.4s)",
                (char*)&(p_filter->fmt_in.video.i_chroma) );
        free( p_sys );
        return VLC_EGENERIC;
    }

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );
    p_sys->i_intensity= var_CreateGetIntegerCommand( p_filter,
                       CFG_PREFIX "intensity" );

    vlc_spin_init( &p_sys->lock );

    var_AddCallback( p_filter, CFG_PREFIX "intensity", FilterCallback, NULL );

    p_filter->pf_video_filter = Filter;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy sepia video thread output method
 *****************************************************************************
 * Terminate an output method
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    var_DelCallback( p_filter, CFG_PREFIX "intensity", FilterCallback, NULL );

    vlc_spin_destroy( &p_filter->p_sys->lock );
    free( p_filter->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to sepia image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    int intensity;

    if( !p_pic ) return NULL;

    filter_sys_t *p_sys = p_filter->p_sys;
    vlc_spin_lock( &p_sys->lock );
    intensity = p_sys->i_intensity;
    vlc_spin_unlock( &p_sys->lock );

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        msg_Warn( p_filter, "can't get output picture" );
        picture_Release( p_pic );
        return NULL;
    }

    p_sys->pf_sepia( p_pic, p_outpic, intensity );

    return CopyInfoAndRelease( p_outpic, p_pic );
}

/*****************************************************************************
 * PlanarI420Sepia: Applies sepia to one frame of the planar I420 video
 *****************************************************************************
 * This function applies sepia effect to one frame of the video by iterating
 * through video lines. We iterate for every two lines and for every two pixels
 * in line to calculate new sepia values for four y components as well for u
 * and v components.
 *****************************************************************************/
static void PlanarI420Sepia( picture_t *p_pic, picture_t *p_outpic,
                               int i_intensity )
{
    /* iterate for every two visible line in the frame */
    for( int y = 0; y < p_pic->p[Y_PLANE].i_visible_lines - 1; y += 2)
    {
        const int i_sy_line1_start = y * p_pic->p[Y_PLANE].i_pitch;
        const int i_sy_line2_start = ( y + 1 ) * p_pic->p[Y_PLANE].i_pitch;
        const int i_su_line_start = (y/2) * p_pic->p[U_PLANE].i_pitch;
        const int i_sv_line_start = (y/2) * p_pic->p[V_PLANE].i_pitch;

        const int i_dy_line1_start = y * p_outpic->p[Y_PLANE].i_pitch;
        const int i_dy_line2_start = ( y + 1 ) * p_outpic->p[Y_PLANE].i_pitch;
        const int i_du_line_start = (y/2) * p_outpic->p[U_PLANE].i_pitch;
        const int i_dv_line_start = (y/2) * p_outpic->p[V_PLANE].i_pitch;
        /* iterate for every two visible line in the frame */
	    for( int x = 0; x < p_pic->p[Y_PLANE].i_visible_pitch - 1; x += 2)
        {
            uint8_t sy1, sy2, sy3, sy4, su, sv;
            uint8_t dy1, dy2, dy3, dy4, du, dv;
            const int i_sy_line1_offset = i_sy_line1_start + x;
            const int i_sy_line2_offset = i_sy_line2_start + x;
            const int i_dy_line1_offset = i_dy_line1_start + x;
            const int i_dy_line2_offset = i_dy_line2_start + x;
            /* get four y components and u and v component */
            sy1 = p_pic->p[Y_PLANE].p_pixels[i_sy_line1_offset];
            sy2 = p_pic->p[Y_PLANE].p_pixels[i_sy_line1_offset + 1];
            sy3 = p_pic->p[Y_PLANE].p_pixels[i_sy_line2_offset];
            sy4 = p_pic->p[Y_PLANE].p_pixels[i_sy_line2_offset + 1];
		    su = p_pic->p[U_PLANE].p_pixels[i_su_line_start + (x/2)];
		    sv = p_pic->p[V_PLANE].p_pixels[i_sv_line_start + (x/2)];
            /* calculate sepia values */
            YuvSepia4( &dy1, &dy2, &dy3, &dy4, &du, &dv,
                      sy1, sy2, sy3, sy4, su, sv, i_intensity );
            /* put new sepia values for all four y components and u and v */
            p_outpic->p[Y_PLANE].p_pixels[i_dy_line1_offset] = dy1;
            p_outpic->p[Y_PLANE].p_pixels[i_dy_line1_offset + 1] = dy2;
            p_outpic->p[Y_PLANE].p_pixels[i_dy_line2_offset] = dy3;
            p_outpic->p[Y_PLANE].p_pixels[i_dy_line2_offset + 1] = dy4;
            p_outpic->p[U_PLANE].p_pixels[i_du_line_start + (x/2)] = du;
            p_outpic->p[V_PLANE].p_pixels[i_dv_line_start + (x/2)] = dv;
	    }
    }
}

/*****************************************************************************
 * PackedYUVSepia: Applies sepia to one frame of the packed YUV video
 *****************************************************************************
 * This function applies sepia effext to one frame of the video by iterating
 * through video lines. In every pass, we calculate new values for pixels
 * (UYVY, VYUY, YUYV and YVYU formats are supported)
 *****************************************************************************/
static void PackedYUVSepia( picture_t *p_pic, picture_t *p_outpic,
                           int i_intensity )
{
    uint8_t *p_in, *p_in_end, *p_line_start, *p_line_end, *p_out;
    int i_yindex = 1, i_uindex = 2, i_vindex = 0;

    GetPackedYuvOffsets( p_outpic->format.i_chroma,
                        &i_yindex, &i_uindex, &i_vindex );

    p_in = p_pic->p[0].p_pixels;
    p_in_end = p_in + p_pic->p[0].i_visible_lines
        * p_pic->p[0].i_pitch;
    p_out = p_outpic->p[0].p_pixels;

    while( p_in < p_in_end )
    {
        p_line_start = p_in;
        p_line_end = p_in + p_pic->p[0].i_visible_pitch;
        while( p_in < p_line_end )
        {
            /* calculate new, sepia values */
            YuvSepia2( &p_out[i_yindex], &p_out[i_yindex + 2], &p_out[i_uindex],
                     &p_out[i_vindex], p_in[i_yindex], p_in[i_yindex + 2],
                     p_in[i_uindex], p_in[i_vindex], i_intensity );
            p_in += 4;
            p_out += 4;
        }
        p_in += p_pic->p[0].i_pitch - p_pic->p[0].i_visible_pitch;
        p_out += p_outpic->p[0].i_pitch
            - p_outpic->p[0].i_visible_pitch;
    }
}

/*****************************************************************************
 * RVSepia: Applies sepia to one frame of the RV24/RV32 video
 *****************************************************************************
 * This function applies sepia effect to one frame of the video by iterating
 * through video lines and calculating new values for every byte in chunks of
 * 3 (RV24) or 4 (RV32) bytes.
 *****************************************************************************/
static void RVSepia( picture_t *p_pic, picture_t *p_outpic, int i_intensity )
{
    uint8_t *p_in, *p_in_end, *p_line_start, *p_line_end, *p_out;
    int i_r, i_g, i_b;
    bool b_isRV32 = p_pic->format.i_chroma == VLC_CODEC_RGB32;
    int i_rindex = 0, i_gindex = 1, i_bindex = 2;

    GetPackedRgbIndexes( &p_outpic->format, &i_rindex, &i_gindex, &i_bindex );

    p_in = p_pic->p[0].p_pixels;
    p_in_end = p_in + p_pic->p[0].i_visible_lines
        * p_pic->p[0].i_pitch;
    p_out = p_outpic->p[0].p_pixels;

    while( p_in < p_in_end )
    {
        p_line_start = p_in;
        p_line_end = p_in + p_pic->p[0].i_visible_pitch;
        while( p_in < p_line_end )
        {
            /* extract r,g,b values */
            i_r = p_in[i_rindex];
            i_g = p_in[i_gindex];
            i_b = p_in[i_bindex];
            p_in += 3;
            /* do sepia */
            Sepia( &i_r, &i_g, &i_b, i_intensity );
            /* put new r,g,b values */
            p_out[i_rindex] = i_r;
            p_out[i_gindex] = i_g;
            p_out[i_bindex] = i_b;
            p_out += 3;
            /* for rv32 we take 4 chunks at the time */
            if ( b_isRV32 )
            {
                /* alpha channel stays the same */
                *p_out++ = *p_in++;
            }
        }
        p_in += p_pic->p[0].i_pitch - p_pic->p[0].i_visible_pitch;
        p_out += p_outpic->p[0].i_pitch
            - p_outpic->p[0].i_visible_pitch;
    }
}

/*****************************************************************************
 * YuvSepia2: Calculates sepia to YUV values for two given Y values
 *****************************************************************************
 * This function calculates sepia values of YUV color space for a given sepia
 * intensity. It converts YUV color values to theirs RGB equivalents,
 * calculates sepia values and then converts RGB values to YUV values again.
 *****************************************************************************/
static void YuvSepia2( uint8_t* sepia_y1, uint8_t* sepia_y2, uint8_t* sepia_u,
                      uint8_t* sepia_v, const uint8_t y1, const uint8_t y2,
                      const uint8_t u, const uint8_t v, int i_intensity )
{
    int r1, g1, b1; /* for y1 new value */
    int r2, b2, g2; /* for y2 new value */
    int r3, g3, b3; /* for new values of u and v */
    /* fist convert YUV -> RGB */
    yuv_to_rgb( &r1, &g1, &b1, y1, u, v );
    yuv_to_rgb( &r2, &g2, &b2, y2, u, v );
    yuv_to_rgb( &r3, &g3, &b3, ( y1 + y2 ) / 2, u, v );
    /* calculates new values for r, g and b components */
    Sepia( &r1, &g1, &b1, i_intensity );
    Sepia( &r2, &g2, &b2, i_intensity );
    Sepia( &r3, &g3, &b3, i_intensity );
    /* convert from calculated RGB -> YUV */
    *sepia_y1 = ( ( 66 * r1 + 129 * g1 +  25 * b1 + 128 ) >> 8 ) +  16;
    *sepia_y2 = ( ( 66 * r2 + 129 * g2 +  25 * b2 + 128 ) >> 8 ) +  16;
    *sepia_u = ( ( -38 * r3 -  74 * g3 + 112 * b3 + 128 ) >> 8 ) + 128;
    *sepia_v = ( ( 112 * r3 -  94 * g3 -  18 * b3 + 128 ) >> 8 ) + 128;
}

/*****************************************************************************
 * YuvSepia4: Calculates sepia to YUV values for given four Y values
 *****************************************************************************
 * This function calculates sepia values of YUV color space for a given sepia
 * intensity. It converts YUV color values to theirs RGB equivalents,
 * calculates sepia values and then converts RGB values to YUV values again.
 *****************************************************************************/
static void YuvSepia4( uint8_t* sepia_y1, uint8_t* sepia_y2, uint8_t* sepia_y3,
                      uint8_t* sepia_y4, uint8_t* sepia_u, uint8_t* sepia_v,
                      const uint8_t y1, const uint8_t y2, const uint8_t y3,
                      const uint8_t y4, const uint8_t u, uint8_t v,
                      int i_intensity )
{
    int r1, g1, b1; /* for y1 new value */
    int r2, b2, g2; /* for y2 new value */
    int r3, b3, g3; /* for y3 new value */
    int r4, b4, g4; /* for y4 new value */
    int r5, g5, b5; /* for new values of u and v */
    /* fist convert YUV -> RGB */
    yuv_to_rgb( &r1, &g1, &b1, y1, u, v );
    yuv_to_rgb( &r2, &g2, &b2, y2, u, v );
    yuv_to_rgb( &r3, &g3, &b3, y3, u, v );
    yuv_to_rgb( &r4, &g4, &b4, y4, u, v );
    yuv_to_rgb( &r5, &g5, &b5, ( y1 + y2 + y3 + y4) / 4, u, v );
    /* calculates new values for r, g and b components */
    Sepia( &r1, &g1, &b1, i_intensity );
    Sepia( &r2, &g2, &b2, i_intensity );
    Sepia( &r3, &g3, &b3, i_intensity );
    Sepia( &r4, &g4, &b4, i_intensity );
    Sepia( &r5, &g5, &b5, i_intensity );
    /* convert from calculated RGB -> YUV */
    *sepia_y1 = ( ( 66 * r1 + 129 * g1 +  25 * b1 + 128 ) >> 8 ) +  16;
    *sepia_y2 = ( ( 66 * r2 + 129 * g2 +  25 * b2 + 128 ) >> 8 ) +  16;
    *sepia_y3 = ( ( 66 * r3 + 129 * g3 +  25 * b3 + 128 ) >> 8 ) +  16;
    *sepia_y4 = ( ( 66 * r4 + 129 * g4 +  25 * b4 + 128 ) >> 8 ) +  16;
    *sepia_u = ( ( -38 * r5 -  74 * g5 + 112 * b5 + 128 ) >> 8 ) + 128;
    *sepia_v = ( ( 112 * r5 -  94 * g5 -  18 * b5 + 128 ) >> 8 ) + 128;
}

/*****************************************************************************
 * Sepia: Calculates sepia of RGB values
 *****************************************************************************
 * This function calculates sepia values of RGB color space for a given sepia
 * intensity. Sepia algorithm is taken from here:
 * http://groups.google.com/group/comp.lang.java.programmer/browse_thread/
 *   thread/9d20a72c40b119d0/18f12770ec6d9dd6
 *****************************************************************************/
static void Sepia( int *p_r, int *p_g, int *p_b, int i_intensity )
{
    int i_sepia_depth = 20;
    int16_t i_round;
    i_round = ( *p_r + *p_g + *p_b ) / 3;
    *p_r = vlc_uint8( i_round + ( i_sepia_depth * 2 ) );
    *p_g = vlc_uint8( i_round + i_sepia_depth );
    *p_b = vlc_uint8( i_round - i_intensity );
}

static int FilterCallback ( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data )
{
    VLC_UNUSED(psz_var); VLC_UNUSED(oldval); VLC_UNUSED(p_data);
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    vlc_spin_lock( &p_sys->lock );
    p_sys->i_intensity = newval.i_int;
    vlc_spin_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}
