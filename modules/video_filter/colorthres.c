/*****************************************************************************
 * colorthres.c: Threshold color based on similarity to reference color
 *****************************************************************************
 * Copyright (C) 2000-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Sigmund Augdal <dnumgis@videolan.org>
 *          Antoine Cellerier <dionoea at videolan dot org>
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
#include <vlc_sout.h>
#include <vlc_atomic.h>
#include <vlc_filter.h>
#include "filter_picture.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );
static picture_t *FilterPacked( filter_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define COLOR_TEXT N_("Color")
#define COLOR_LONGTEXT N_("Colors similar to this will be kept, others will be "\
    "grayscaled. This must be an hexadecimal (like HTML colors). The first two "\
    "chars are for red, then green, then blue. #000000 = black, #FF0000 = red,"\
    " #00FF00 = green, #FFFF00 = yellow (red + green), #FFFFFF = white" )
#define COLOR_HELP N_("Select one color in the video")
static const int pi_color_values[] = {
  0x00FF0000, 0x00FF00FF, 0x00FFFF00, 0x0000FF00, 0x000000FF, 0x0000FFFF };

static const char *const ppsz_color_descriptions[] = {
  N_("Red"), N_("Fuchsia"), N_("Yellow"), N_("Lime"), N_("Blue"), N_("Aqua") };

#define CFG_PREFIX "colorthres-"

vlc_module_begin ()
    set_description( N_("Color threshold filter") )
    set_shortname( N_("Color threshold" ))
    set_help(COLOR_HELP)
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_capability( "video filter2", 0 )
    add_rgb( CFG_PREFIX "color", 0x00FF0000, COLOR_TEXT,
                 COLOR_LONGTEXT, false )
        change_integer_list( pi_color_values, ppsz_color_descriptions )
    add_integer( CFG_PREFIX "saturationthres", 20,
                 N_("Saturation threshold"), "", false )
    add_integer( CFG_PREFIX "similaritythres", 15,
                 N_("Similarity threshold"), "", false )
    set_callbacks( Create, Destroy )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "color", "saturationthres", "similaritythres", NULL
};

/*****************************************************************************
 * callback prototypes
 *****************************************************************************/
static int FilterCallback( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );


/*****************************************************************************
 * filter_sys_t: adjust filter method descriptor
 *****************************************************************************/
struct filter_sys_t
{
    atomic_int i_simthres;
    atomic_int i_satthres;
    atomic_int i_color;
};

/*****************************************************************************
 * Create: allocates adjust video thread output method
 *****************************************************************************
 * This function allocates and initializes a adjust vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    switch( p_filter->fmt_in.video.i_chroma )
    {
        CASE_PLANAR_YUV
            p_filter->pf_video_filter = Filter;
            break;

        CASE_PACKED_YUV_422
            p_filter->pf_video_filter = FilterPacked;
            break;

        default:
            msg_Err( p_filter, "Unsupported input chroma (%4.4s)",
                     (char*)&(p_filter->fmt_in.video.i_chroma) );
            return VLC_EGENERIC;
    }

    if( p_filter->fmt_in.video.i_chroma != p_filter->fmt_out.video.i_chroma )
    {
        msg_Err( p_filter, "Input and output chromas don't match" );
        return VLC_EGENERIC;
    }

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );
    atomic_init( &p_sys->i_color,
                 var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "color" ) );
    atomic_init( &p_sys->i_simthres,
       var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "similaritythres" ) );
    atomic_init( &p_sys->i_satthres,
       var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "saturationthres" ) );

    var_AddCallback( p_filter, CFG_PREFIX "color", FilterCallback, p_sys );
    var_AddCallback( p_filter, CFG_PREFIX "similaritythres", FilterCallback, p_sys );
    var_AddCallback( p_filter, CFG_PREFIX "saturationthres", FilterCallback, p_sys );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy adjust video thread output method
 *****************************************************************************
 * Terminate an output method created by adjustCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    var_DelCallback( p_filter, CFG_PREFIX "color", FilterCallback, p_sys );
    var_DelCallback( p_filter, CFG_PREFIX "similaritythres", FilterCallback, p_sys );
    var_DelCallback( p_filter, CFG_PREFIX "saturationthres", FilterCallback, p_sys );
    free( p_sys );
}

static void GetReference( int *refu, int *refv, int *reflength,
                          uint32_t i_color )
{
    int i_red   = ( i_color & 0xFF0000 ) >> 16;
    int i_green = ( i_color & 0x00FF00 ) >> 8;
    int i_blue  = ( i_color & 0x0000FF );
    int i_u = (int8_t)(( -38 * i_red - 74 * i_green + 112 * i_blue + 128) >> 8) + 128;
    int i_v = (int8_t)(( 112 * i_red - 94 * i_green -  18 * i_blue + 128) >> 8) + 128;
    *refu = i_u - 0x80;
    *refv = i_v - 0x80;
    *reflength = sqrt(*refu * *refu + *refv * *refv);
}

static bool IsSimilar( int u, int v,
                       int refu, int refv, int reflength,
                       int i_satthres, int i_simthres )
{
    int length = sqrt(u * u + v * v);

    int diffu = refu * length - u * reflength;
    int diffv = refv * length - v * reflength;
    int64_t difflen2 = diffu * diffu + diffv * diffv;
    int64_t thres = length * reflength;
    thres *= thres;
    return length > i_satthres && (difflen2 * i_simthres < thres);
}
/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to adjust modified image,
 * waits until it is displayed and switch the two rendering buffers, preparing
 * next frame.
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    filter_sys_t *p_sys = p_filter->p_sys;
    int i_simthres = atomic_load( &p_sys->i_simthres );
    int i_satthres = atomic_load( &p_sys->i_satthres );
    int i_color = atomic_load( &p_sys->i_color );

    if( !p_pic ) return NULL;

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }

    /* Copy the Y plane */
    plane_CopyPixels( &p_outpic->p[Y_PLANE], &p_pic->p[Y_PLANE] );

    /*
     * Do the U and V planes
     */
    int refu, refv, reflength;
    GetReference( &refu, &refv, &reflength, i_color );

    for( int y = 0; y < p_pic->p[U_PLANE].i_visible_lines; y++ )
    {
        uint8_t *p_src_u = &p_pic->p[U_PLANE].p_pixels[y * p_pic->p[U_PLANE].i_pitch];
        uint8_t *p_src_v = &p_pic->p[V_PLANE].p_pixels[y * p_pic->p[V_PLANE].i_pitch];
        uint8_t *p_dst_u = &p_outpic->p[U_PLANE].p_pixels[y * p_outpic->p[U_PLANE].i_pitch];
        uint8_t *p_dst_v = &p_outpic->p[V_PLANE].p_pixels[y * p_outpic->p[V_PLANE].i_pitch];

        for( int x = 0; x < p_pic->p[U_PLANE].i_visible_pitch; x++ )
        {
            if( IsSimilar( *p_src_u - 0x80, *p_src_v - 0x80,
                           refu, refv, reflength,
                           i_satthres, i_simthres ) )

            {
                *p_dst_u++ = *p_src_u;
                *p_dst_v++ = *p_src_v;
            }
            else
            {
                *p_dst_u++ = 0x80;
                *p_dst_v++ = 0x80;
            }
            p_src_u++;
            p_src_v++;
        }
    }

    return CopyInfoAndRelease( p_outpic, p_pic );
}

static picture_t *FilterPacked( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    filter_sys_t *p_sys = p_filter->p_sys;
    int i_simthres = atomic_load( &p_sys->i_simthres );
    int i_satthres = atomic_load( &p_sys->i_satthres );
    int i_color = atomic_load( &p_sys->i_color );

    if( !p_pic ) return NULL;

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }

    int i_y_offset, i_u_offset, i_v_offset;
    int i_ret = GetPackedYuvOffsets( p_filter->fmt_in.video.i_chroma,
                                     &i_y_offset, &i_u_offset, &i_v_offset );
    if( i_ret == VLC_EGENERIC )
    {
        picture_Release( p_pic );
        return NULL;
    }

    /*
     * Copy Y and do the U and V planes
     */
    int refu, refv, reflength;
    GetReference( &refu, &refv, &reflength, i_color );

    for( int y = 0; y < p_pic->p->i_visible_lines; y++ )
    {
        uint8_t *p_src = &p_pic->p->p_pixels[y * p_pic->p->i_pitch];
        uint8_t *p_dst = &p_outpic->p->p_pixels[y * p_outpic->p->i_pitch];

        for( int x = 0; x < p_pic->p->i_visible_pitch / 4; x++ )
        {
            p_dst[i_y_offset + 0] = p_src[i_y_offset + 0];
            p_dst[i_y_offset + 2] = p_src[i_y_offset + 2];

            if( IsSimilar( p_src[i_u_offset] - 0x80, p_src[i_v_offset] - 0x80,
                           refu, refv, reflength,
                           i_satthres, i_simthres ) )
            {
                p_dst[i_u_offset] = p_src[i_u_offset];
                p_dst[i_v_offset] = p_src[i_v_offset];
            }
            else
            {
                p_dst[i_u_offset] = 0x80;
                p_dst[i_v_offset] = 0x80;
            }

            p_dst += 4;
            p_src += 4;
        }
    }

    return CopyInfoAndRelease( p_outpic, p_pic );
}

static int FilterCallback ( vlc_object_t *p_this, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    filter_sys_t *p_sys = p_data;

    if( !strcmp( psz_var, CFG_PREFIX "color" ) )
        atomic_store( &p_sys->i_color, newval.i_int );
    else if( !strcmp( psz_var, CFG_PREFIX "similaritythres" ) )
        atomic_store( &p_sys->i_simthres, newval.i_int );
    else /* CFG_PREFIX "saturationthres" */
        atomic_store( &p_sys->i_satthres, newval.i_int );

    (void)p_this; (void)oldval;
    return VLC_SUCCESS;
}
