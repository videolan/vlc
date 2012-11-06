/*****************************************************************************
 * sharpen.c: Sharpen video filter
 *****************************************************************************
 * Copyright (C) 2003-2007 VLC authors and VideoLAN
 * $Id$
 *
 * Author: Jérémy DEMEULE <dj_mulder at djduron dot no-ip dot org>
 *         Jean-Baptiste Kempf <jb at videolan dot org>
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

/* The sharpen filter. */
/*
 * static int filter[] = { -1, -1, -1,
 *                         -1,  8, -1,
 *                         -1, -1, -1 };
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <vlc_filter.h>
#include "filter_picture.h"

#define SIG_TEXT N_("Sharpen strength (0-2)")
#define SIG_LONGTEXT N_("Set the Sharpen strength, between 0 and 2. Defaults to 0.05.")

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );
static int SharpenCallback( vlc_object_t *, char const *,
                            vlc_value_t, vlc_value_t, void * );

#define SHARPEN_HELP N_("Augment contrast between contours.")
#define FILTER_PREFIX "sharpen-"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Sharpen video filter") )
    set_shortname( N_("Sharpen") )
    set_help(SHARPEN_HELP)
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_capability( "video filter2", 0 )
    add_float_with_range( "sharpen-sigma", 0.05, 0.0, 2.0,
        SIG_TEXT, SIG_LONGTEXT, false )
    add_shortcut( "sharpen" )
    set_callbacks( Create, Destroy )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "sigma", NULL
};

/*****************************************************************************
 * filter_sys_t: Sharpen video filter descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Sharpen specific properties of an output thread.
 *****************************************************************************/

struct filter_sys_t
{
    vlc_mutex_t lock;
    int tab_precalc[512];
};

/*****************************************************************************
 * clip: avoid negative value and value > 255
 *****************************************************************************/
inline static int32_t clip( int32_t a )
{
    return (a > 255) ? 255 : (a < 0) ? 0 : a;
}

static void init_precalc_table(filter_sys_t *p_filter, float sigma)
{
    for(int i = 0; i < 512; ++i)
    {
        p_filter->tab_precalc[i] = (i - 256) * sigma;
    }
}

/*****************************************************************************
 * Create: allocates Sharpen video thread output method
 *****************************************************************************
 * This function allocates and initializes a Sharpen vout method.
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
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;

    p_filter->pf_video_filter = Filter;

    config_ChainParse( p_filter, FILTER_PREFIX, ppsz_filter_options,
                   p_filter->p_cfg );

    float sigma = var_CreateGetFloatCommand( p_filter, FILTER_PREFIX "sigma" );
    init_precalc_table(p_filter->p_sys, sigma);

    vlc_mutex_init( &p_filter->p_sys->lock );
    var_AddCallback( p_filter, FILTER_PREFIX "sigma",
                     SharpenCallback, p_filter->p_sys );

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Destroy: destroy Sharpen video thread output method
 *****************************************************************************
 * Terminate an output method created by SharpenCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    var_DelCallback( p_filter, FILTER_PREFIX "sigma", SharpenCallback, p_sys );
    vlc_mutex_destroy( &p_sys->lock );
    free( p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Invert image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    int i, j;
    uint8_t *p_src = NULL;
    uint8_t *p_out = NULL;
    int i_src_pitch;
    int i_out_pitch;
    int pix;
    const int v1 = -1;
    const int v2 = 3; /* 2^3 = 8 */

    if( !p_pic ) return NULL;

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }

    /* process the Y plane */
    p_src = p_pic->p[Y_PLANE].p_pixels;
    p_out = p_outpic->p[Y_PLANE].p_pixels;
    i_src_pitch = p_pic->p[Y_PLANE].i_pitch;
    i_out_pitch = p_outpic->p[Y_PLANE].i_pitch;

    /* perform convolution only on Y plane. Avoid border line. */
    vlc_mutex_lock( &p_filter->p_sys->lock );
    for( i = 0; i < p_pic->p[Y_PLANE].i_visible_lines; i++ )
    {
        if( (i == 0) || (i == p_pic->p[Y_PLANE].i_visible_lines - 1) )
        {
            for( j = 0; j < p_pic->p[Y_PLANE].i_visible_pitch; j++ )
                p_out[i * i_out_pitch + j] = clip( p_src[i * i_src_pitch + j] );
            continue ;
        }
        for( j = 0; j < p_pic->p[Y_PLANE].i_visible_pitch; j++ )
        {
            if( (j == 0) || (j == p_pic->p[Y_PLANE].i_visible_pitch - 1) )
            {
                p_out[i * i_out_pitch + j] = p_src[i * i_src_pitch + j];
                continue ;
            }

            pix = (p_src[(i - 1) * i_src_pitch + j - 1] * v1) +
                  (p_src[(i - 1) * i_src_pitch + j    ] * v1) +
                  (p_src[(i - 1) * i_src_pitch + j + 1] * v1) +
                  (p_src[(i    ) * i_src_pitch + j - 1] * v1) +
                  (p_src[(i    ) * i_src_pitch + j    ] << v2) +
                  (p_src[(i    ) * i_src_pitch + j + 1] * v1) +
                  (p_src[(i + 1) * i_src_pitch + j - 1] * v1) +
                  (p_src[(i + 1) * i_src_pitch + j    ] * v1) +
                  (p_src[(i + 1) * i_src_pitch + j + 1] * v1);

           pix = pix >= 0 ? clip(pix) : -clip(pix * -1);
           p_out[i * i_out_pitch + j] = clip( p_src[i * i_src_pitch + j] +
               p_filter->p_sys->tab_precalc[pix + 256] );
        }
    }
    vlc_mutex_unlock( &p_filter->p_sys->lock );

    plane_CopyPixels( &p_outpic->p[U_PLANE], &p_pic->p[U_PLANE] );
    plane_CopyPixels( &p_outpic->p[V_PLANE], &p_pic->p[V_PLANE] );

    return CopyInfoAndRelease( p_outpic, p_pic );
}

static int SharpenCallback( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(oldval); VLC_UNUSED(psz_var);
    filter_sys_t *p_sys = (filter_sys_t *)p_data;

    vlc_mutex_lock( &p_sys->lock );
    init_precalc_table( p_sys,  VLC_CLIP( newval.f_float, 0., 2. ) );
    vlc_mutex_unlock( &p_sys->lock );
    return VLC_SUCCESS;
}
