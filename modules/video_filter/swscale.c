/*****************************************************************************
 * swscale.c: scaling and chroma conversion using libswscale
 *****************************************************************************
 * Copyright (C) 1999-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_filter.h>

#ifdef HAVE_LIBSWSCALE_SWSCALE_H
#   include <libswscale/swscale.h>
#elif defined(HAVE_FFMPEG_SWSCALE_H)
#   include <ffmpeg/swscale.h>
#endif

/* Gruikkkkkkkkkk!!!!! */
#include "../codec/avcodec/chroma.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenScaler( vlc_object_t * );
static void CloseScaler( vlc_object_t * );

#define SCALEMODE_TEXT N_("Scaling mode")
#define SCALEMODE_LONGTEXT N_("Scaling mode to use.")

static const int pi_mode_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
const char *const ppsz_mode_descriptions[] =
{ N_("Fast bilinear"), N_("Bilinear"), N_("Bicubic (good quality)"),
  N_("Experimental"), N_("Nearest neighbour (bad quality)"),
  N_("Area"), N_("Luma bicubic / chroma bilinear"), N_("Gauss"),
  N_("SincR"), N_("Lanczos"), N_("Bicubic spline") };

vlc_module_begin();
    set_description( N_("Video scaling filter") );
    set_capability( "video filter2", 1000 );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );
    set_callbacks( OpenScaler, CloseScaler );
    add_integer( "swscale-mode", 2, NULL, SCALEMODE_TEXT, SCALEMODE_LONGTEXT, true );
        change_integer_list( pi_mode_values, ppsz_mode_descriptions, 0 );
vlc_module_end();

/* Version checking */
#if LIBSWSCALE_VERSION_INT >= ((0<<16)+(5<<8)+0)
/****************************************************************************
 * Local prototypes
 ****************************************************************************/

void *( *swscale_fast_memcpy )( void *, const void *, size_t );
static picture_t *Filter( filter_t *, picture_t * );
static int CheckInit( filter_t * );

static int GetParameters( int *pi_fmti, int *pi_fmto,
                          const video_format_t *p_fmti, 
                          const video_format_t *p_fmto  );

/*****************************************************************************
 * filter_sys_t : filter descriptor
 *****************************************************************************/
struct filter_sys_t
{
    struct SwsContext *ctx;
    SwsFilter *p_src_filter;
    SwsFilter *p_dst_filter;
    int i_cpu_mask, i_sws_flags;

    es_format_t fmt_in;
    es_format_t fmt_out;
};


/*****************************************************************************
 * OpenScaler: probe the filter and return score
 *****************************************************************************/
static int OpenScaler( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys;

    unsigned int i_cpu;
    int i_sws_mode;

    float sws_lum_gblur = 0.0, sws_chr_gblur = 0.0;
    int sws_chr_vshift = 0, sws_chr_hshift = 0;
    float sws_chr_sharpen = 0.0, sws_lum_sharpen = 0.0;

    if( GetParameters( NULL, NULL, 
                       &p_filter->fmt_in.video,
                       &p_filter->fmt_out.video ) )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_filter->p_sys = p_sys = malloc(sizeof(filter_sys_t)) ) == NULL )
    {
        return VLC_ENOMEM;
    }

    swscale_fast_memcpy = vlc_memcpy;   /* FIXME pointer assignment may not be atomic */

    /* Set CPU capabilities */
    i_cpu = vlc_CPU();
    p_sys->i_cpu_mask = 0;
    if( i_cpu & CPU_CAPABILITY_MMX )
    {
        p_sys->i_cpu_mask |= SWS_CPU_CAPS_MMX;
    }
#if (LIBSWSCALE_VERSION_INT >= ((0<<16)+(5<<8)+0))
    if( i_cpu & CPU_CAPABILITY_MMXEXT )
    {
        p_sys->i_cpu_mask |= SWS_CPU_CAPS_MMX2;
    }
#endif
    if( i_cpu & CPU_CAPABILITY_3DNOW )
    {
        p_sys->i_cpu_mask |= SWS_CPU_CAPS_3DNOW;
    }
    if( i_cpu & CPU_CAPABILITY_ALTIVEC )
    {
        p_sys->i_cpu_mask |= SWS_CPU_CAPS_ALTIVEC;
    }

    i_sws_mode = var_CreateGetInteger( p_filter, "swscale-mode" );

    switch( i_sws_mode )
    {
    case 0:  p_sys->i_sws_flags = SWS_FAST_BILINEAR; break;
    case 1:  p_sys->i_sws_flags = SWS_BILINEAR; break;
    case 2:  p_sys->i_sws_flags = SWS_BICUBIC; break;
    case 3:  p_sys->i_sws_flags = SWS_X; break;
    case 4:  p_sys->i_sws_flags = SWS_POINT; break;
    case 5:  p_sys->i_sws_flags = SWS_AREA; break;
    case 6:  p_sys->i_sws_flags = SWS_BICUBLIN; break;
    case 7:  p_sys->i_sws_flags = SWS_GAUSS; break;
    case 8:  p_sys->i_sws_flags = SWS_SINC; break;
    case 9:  p_sys->i_sws_flags = SWS_LANCZOS; break;
    case 10: p_sys->i_sws_flags = SWS_SPLINE; break;
    default: p_sys->i_sws_flags = SWS_BICUBIC; i_sws_mode = 2; break;
    }

    p_sys->p_src_filter =
        sws_getDefaultFilter( sws_lum_gblur, sws_chr_gblur,
                              sws_lum_sharpen, sws_chr_sharpen,
                              sws_chr_hshift, sws_chr_vshift, 0 );
    p_sys->p_dst_filter = NULL;

    /* Misc init */
    p_sys->ctx = NULL;
    p_filter->pf_video_filter = Filter;
    es_format_Init( &p_sys->fmt_in, 0, 0 );
    es_format_Init( &p_sys->fmt_out, 0, 0 );

    if( CheckInit( p_filter ) )
    {
        if( p_sys->p_src_filter )
            sws_freeFilter( p_sys->p_src_filter );
        free( p_sys );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_filter, "%ix%i chroma: %4.4s -> %ix%i chroma: %4.4s with scaling using %s",
             p_filter->fmt_in.video.i_width, p_filter->fmt_in.video.i_height,
             (char *)&p_filter->fmt_in.video.i_chroma,
             p_filter->fmt_out.video.i_width, p_filter->fmt_out.video.i_height,
             (char *)&p_filter->fmt_out.video.i_chroma,
             ppsz_mode_descriptions[i_sws_mode] );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseFilter: clean up the filter
 *****************************************************************************/
static void CloseScaler( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    if( p_sys->ctx )
        sws_freeContext( p_sys->ctx );
    if( p_sys->p_src_filter )
        sws_freeFilter( p_sys->p_src_filter );
    free( p_sys );
}

/*****************************************************************************
 * CheckInit: Initialise filter when necessary
 *****************************************************************************/
static bool IsFmtSimilar( const video_format_t *p_fmt1, const video_format_t *p_fmt2 )
{
    return p_fmt1->i_width == p_fmt2->i_width &&
           p_fmt1->i_height == p_fmt2->i_height;
}

static int GetParameters( int *pi_fmti, int *pi_fmto,
                          const video_format_t *p_fmti, 
                          const video_format_t *p_fmto  )
{
    /* Supported Input formats: YV12, I420/IYUV, YUY2, UYVY, BGR32, BGR24,
     * BGR16, BGR15, RGB32, RGB24, Y8/Y800, YVU9/IF09 */
    const int i_fmti = GetFfmpegChroma( p_fmti->i_chroma );

    /* Supported output formats: YV12, I420/IYUV, YUY2, UYVY,
     * {BGR,RGB}{1,4,8,15,16,24,32}, Y8/Y800, YVU9/IF09 */
    const int i_fmto = GetFfmpegChroma( p_fmto->i_chroma );

    if( pi_fmti )
        *pi_fmti = i_fmti;
    if( pi_fmto )
        *pi_fmto = i_fmto;

    if( i_fmti < 0 || i_fmto < 0 )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static int CheckInit( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if( IsFmtSimilar( &p_filter->fmt_in.video,  &p_sys->fmt_in.video ) &&
        IsFmtSimilar( &p_filter->fmt_out.video, &p_sys->fmt_out.video ) &&
        p_sys->ctx )
    {
        return VLC_SUCCESS;
    }

    /* */
    int i_fmt_in, i_fmt_out;
    if( GetParameters( &i_fmt_in, &i_fmt_out, &p_filter->fmt_in.video, &p_filter->fmt_out.video ) )
    {
        msg_Err( p_filter, "format not supported" );
        return VLC_EGENERIC;
    }

    if( p_sys->ctx )
        sws_freeContext( p_sys->ctx );

    p_sys->ctx =
        sws_getContext( p_filter->fmt_in.video.i_width,
                        p_filter->fmt_in.video.i_height, i_fmt_in,
                        p_filter->fmt_out.video.i_width,
                        p_filter->fmt_out.video.i_height, i_fmt_out,
                        p_sys->i_sws_flags | p_sys->i_cpu_mask,
                        p_sys->p_src_filter, p_sys->p_dst_filter, 0 );
    if( !p_sys->ctx )
    {
        msg_Err( p_filter, "could not init SwScaler" );
        return VLC_EGENERIC;
    }

    p_sys->fmt_in = p_filter->fmt_in;
    p_sys->fmt_out = p_filter->fmt_out;

    return VLC_SUCCESS;
}

static void GetPixels( uint8_t *pp_pixel[3], int pi_pitch[3], picture_t *p_picture )
{
    int n;
    for( n = 0; n < __MIN(3 , p_picture->i_planes ); n++ )
    {
        pp_pixel[n] = p_picture->p[n].p_pixels;
        pi_pitch[n] = p_picture->p[n].i_pitch;
    }
}

/****************************************************************************
 * Filter: the whole thing
 ****************************************************************************
 * This function is called just after the thread is launched.
 ****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    uint8_t *src[3]; int src_stride[3];
    uint8_t *dst[3]; int dst_stride[3];
    picture_t *p_pic_dst;

    /* Check if format properties changed */
    if( CheckInit( p_filter ) != VLC_SUCCESS )
    {
        picture_Release( p_pic );
        return NULL;
    }

    /* Request output picture */
    p_pic_dst = filter_NewPicture( p_filter );
    if( !p_pic_dst )
    {
        picture_Release( p_pic );
        return NULL;
    }

    if( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('Y','U','V','A') )
    {
        memset( p_pic_dst->p[3].p_pixels, 0xff, p_filter->fmt_out.video.i_height
                                                 * p_pic_dst->p[3].i_pitch );
    }

    GetPixels( src, src_stride, p_pic );
    GetPixels( dst, dst_stride, p_pic_dst );

#if LIBSWSCALE_VERSION_INT  >= ((0<<16)+(5<<8)+0)
    sws_scale( p_sys->ctx, src, src_stride,
               0, p_filter->fmt_in.video.i_height,
               dst, dst_stride );
#else
    sws_scale_ordered( p_sys->ctx, src, src_stride,
               0, p_filter->fmt_in.video.i_height,
               dst, dst_stride );
#endif

    picture_CopyProperties( p_pic_dst, p_pic );
    picture_Release( p_pic );
    return p_pic_dst;
}

#else /* LIBSWSCALE_VERSION_INT >= ((0<<16)+(5<<8)+0) */

int OpenScaler( vlc_object_t *p_this )
{
    return VLC_EGENERIC;
}

void CloseScaler( vlc_object_t *p_this )
{
}

#endif /* LIBSWSCALE_VERSION_INT >= ((0<<16)+(5<<8)+0) */
