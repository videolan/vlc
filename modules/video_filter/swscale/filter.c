/*****************************************************************************
 * filter.c: video scaling module using the swscale library
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id$
 *
 * Author: Gildas Bazin <gbazin@videolan.org>
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

#include "common.h"
#include "swscale.h"

void *( *swscale_fast_memcpy )( void *, const void *, size_t );

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

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenFilter ( vlc_object_t * );
static void CloseFilter( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );
static int CheckInit( filter_t * );
static int GetSwscaleChroma( vlc_fourcc_t );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define MODE_TEXT N_("Scaling mode")
#define MODE_LONGTEXT N_("You can choose the default scaling mode.")

static int pi_mode_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
static char *ppsz_mode_descriptions[] =
{ N_("Fast bilinear"), N_("Bilinear"), N_("Bicubic (good quality)"),
  N_("Experimental"), N_("Nearest neighbour (bad quality)"),
  N_("Area"), N_("Luma bicubic / chroma bilinear"), N_("Gauss"),
  N_("SincR"), N_("Lanczos"), N_("Bicubic spline") };

vlc_module_begin();
    set_description( _("Video scaling filter") );
    set_capability( "video filter2", 1000 );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );
    set_callbacks( OpenFilter, CloseFilter );

    add_integer( "swscale-mode", 0, NULL, MODE_TEXT, MODE_LONGTEXT, VLC_TRUE );
        change_integer_list( pi_mode_values, ppsz_mode_descriptions, 0 );
vlc_module_end();

/*****************************************************************************
 * OpenFilter: probe the filter and return score
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys;
    vlc_value_t val;

    unsigned int i_fmt_in, i_fmt_out;
    int i_sws_mode;

    float sws_lum_gblur = 0.0, sws_chr_gblur = 0.0;
    int sws_chr_vshift = 0, sws_chr_hshift = 0;
    float sws_chr_sharpen = 0.0, sws_lum_sharpen = 0.0;

    /* Supported Input formats: YV12, I420/IYUV, YUY2, UYVY, BGR32, BGR24,
     * BGR16, BGR15, RGB32, RGB24, Y8/Y800, YVU9/IF09 */
    if( !(i_fmt_in = GetSwscaleChroma(p_filter->fmt_in.video.i_chroma)) )
    {
        return VLC_EGENERIC;
    }

    /* Supported output formats: YV12, I420/IYUV, YUY2, UYVY,
     * {BGR,RGB}{1,4,8,15,16,24,32}, Y8/Y800, YVU9/IF09 */
    if( !(i_fmt_out = GetSwscaleChroma(p_filter->fmt_out.video.i_chroma)) )
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

    swscale_fast_memcpy = p_filter->p_vlc->pf_memcpy;

    /* Set CPU capabilities */
    p_sys->i_cpu_mask = 0;
    if( p_filter->p_libvlc->i_cpu & CPU_CAPABILITY_MMX )
    {
        p_sys->i_cpu_mask |= SWS_CPU_CAPS_MMX;
    }
    if( p_filter->p_libvlc->i_cpu & CPU_CAPABILITY_MMXEXT )
    {
        p_sys->i_cpu_mask |= SWS_CPU_CAPS_MMX2;
    }
    if( p_filter->p_libvlc->i_cpu & CPU_CAPABILITY_3DNOW )
    {
        p_sys->i_cpu_mask |= SWS_CPU_CAPS_3DNOW;
    }
    if( p_filter->p_libvlc->i_cpu & CPU_CAPABILITY_ALTIVEC )
    {
        p_sys->i_cpu_mask |= SWS_CPU_CAPS_ALTIVEC;
    }

    var_Create( p_filter, "swscale-mode", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    var_Get( p_filter, "swscale-mode", &val );
    i_sws_mode = val.i_int;

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
    default: p_sys->i_sws_flags = SWS_FAST_BILINEAR; i_sws_mode = 0; break;
    }

    p_sys->p_src_filter = 0; p_sys->p_dst_filter = 0;
    p_sys->p_src_filter =
        sws_getDefaultFilter( sws_lum_gblur, sws_chr_gblur,
                              sws_lum_sharpen, sws_chr_sharpen,
                              sws_chr_hshift, sws_chr_vshift, 0 );

    /* Misc init */
    p_sys->ctx = NULL;
    p_filter->pf_video_filter = Filter;
    es_format_Init( &p_sys->fmt_in, 0, 0 );
    es_format_Init( &p_sys->fmt_out, 0, 0 );

    if( CheckInit( p_filter ) != VLC_SUCCESS )
    {
        if( p_sys->p_src_filter ) sws_freeFilter( p_sys->p_src_filter );
        free( p_sys );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_filter, "%ix%i chroma: %4.4s -> %ix%i chroma: %4.4s",
             p_filter->fmt_in.video.i_width, p_filter->fmt_in.video.i_height,
             (char *)&p_filter->fmt_in.video.i_chroma,
             p_filter->fmt_out.video.i_width, p_filter->fmt_out.video.i_height,
             (char *)&p_filter->fmt_out.video.i_chroma );

    if( p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width ||
        p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height )
    {
        msg_Dbg( p_filter, "scaling mode: %s",
                 ppsz_mode_descriptions[i_sws_mode] );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseFilter: clean up the filter
 *****************************************************************************/
static void CloseFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    if( p_sys->ctx ) sws_freeContext( p_sys->ctx );
    if( p_sys->p_src_filter ) sws_freeFilter( p_sys->p_src_filter );
    free( p_sys );
}

/*****************************************************************************
 * CheckInit: Initialise filter when necessary
 *****************************************************************************/
static int CheckInit( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if( p_filter->fmt_in.video.i_width != p_sys->fmt_in.video.i_width ||
        p_filter->fmt_in.video.i_height != p_sys->fmt_in.video.i_height ||
        p_filter->fmt_out.video.i_width != p_sys->fmt_out.video.i_width ||
        p_filter->fmt_out.video.i_height != p_sys->fmt_out.video.i_height )
    {
        unsigned int i_fmt_in, i_fmt_out;

        if( !(i_fmt_in = GetSwscaleChroma(p_filter->fmt_in.video.i_chroma)) ||
            !(i_fmt_out = GetSwscaleChroma(p_filter->fmt_out.video.i_chroma)) )
        {
            msg_Err( p_filter, "format not supported" );
            return VLC_EGENERIC;
        }

        if( p_sys->ctx ) sws_freeContext( p_sys->ctx );

        p_sys->ctx =
            sws_getContext( p_filter->fmt_in.video.i_width,
                            p_filter->fmt_in.video.i_height, i_fmt_in,
                            p_filter->fmt_out.video.i_width,
                            p_filter->fmt_out.video.i_height, i_fmt_out,
                            p_sys->i_sws_flags | p_sys->i_cpu_mask,
                            p_sys->p_src_filter, p_sys->p_dst_filter );
        if( !p_sys->ctx )
        {
            msg_Err( p_filter, "could not init SwScaler" );
            return VLC_EGENERIC;
        }

        p_sys->fmt_in = p_filter->fmt_in;
        p_sys->fmt_out = p_filter->fmt_out;
    }

    return VLC_SUCCESS;
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
    int i_plane;
    int i_nb_planes = p_pic->i_planes;

    /* Check if format properties changed */
    if( CheckInit( p_filter ) != VLC_SUCCESS ) return 0;

    /* Request output picture */
    p_pic_dst = p_filter->pf_vout_buffer_new( p_filter );
    if( !p_pic_dst )
    {
        msg_Warn( p_filter, "can't get output picture" );
        return NULL;
    }

    if( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('Y','U','V','A') )
    {
        i_nb_planes = 3;
        memset( p_pic_dst->p[3].p_pixels, 0xff, p_filter->fmt_out.video.i_height
                                                 * p_pic_dst->p[3].i_pitch );
    }

    for( i_plane = 0; i_plane < __MIN(3, p_pic->i_planes); i_plane++ )
    {
        src[i_plane] = p_pic->p[i_plane].p_pixels;
        src_stride[i_plane] = p_pic->p[i_plane].i_pitch;
    }
    for( i_plane = 0; i_plane < __MIN(3, i_nb_planes); i_plane++ )
    {
        dst[i_plane] = p_pic_dst->p[i_plane].p_pixels;
        dst_stride[i_plane] = p_pic_dst->p[i_plane].i_pitch;
    }

    sws_scale_ordered( p_sys->ctx, src, src_stride,
                       0, p_filter->fmt_in.video.i_height,
                       dst, dst_stride );

    p_pic_dst->date = p_pic->date;
    p_pic_dst->b_force = p_pic->b_force;
    p_pic_dst->i_nb_fields = p_pic->i_nb_fields;
    p_pic_dst->b_progressive = p_pic->b_progressive;
    p_pic_dst->b_top_field_first = p_pic->b_top_field_first;

    p_pic->pf_release( p_pic );
    return p_pic_dst;
}

/*****************************************************************************
 * Chroma fourcc -> ffmpeg_id mapping
 *****************************************************************************/
static struct
{
    vlc_fourcc_t  i_chroma;
    unsigned int  i_swscale_chroma;

} chroma_table[] =
{
    /* Planar YUV formats */
    { VLC_FOURCC('Y','V','1','2'), IMGFMT_YV12 },
    { VLC_FOURCC('I','4','2','0'), IMGFMT_I420 },
    { VLC_FOURCC('I','Y','U','V'), IMGFMT_IYUV },
    { VLC_FOURCC('I','4','4','4'), IMGFMT_444P },
    { VLC_FOURCC('Y','U','V','A'), IMGFMT_444P },
    { VLC_FOURCC('I','4','2','2'), IMGFMT_422P },
    { VLC_FOURCC('I','4','1','1'), IMGFMT_411P },
#if 0
    { VLC_FOURCC('Y','U','V','P'), IMGFMT_Y800 },
#endif

    /* Packed YUV formats */
    { VLC_FOURCC('U','Y','V','Y'), IMGFMT_UYVY },
    { VLC_FOURCC('Y','U','Y','2'), IMGFMT_YUY2 },

    /* Packed RGB formats */
    { VLC_FOURCC('R','V','1','5'), IMGFMT_RGB15 },
    { VLC_FOURCC('R','V','1','6'), IMGFMT_RGB16 },
    { VLC_FOURCC('R','V','2','4'), IMGFMT_RGB24 },
    { VLC_FOURCC('R','V','3','2'), IMGFMT_RGB32 },
    { VLC_FOURCC('G','R','E','Y'), IMGFMT_RGB8 },

    {0}
};

static int GetSwscaleChroma( vlc_fourcc_t i_chroma )
{
    int i;

    for( i = 0; chroma_table[i].i_chroma != 0; i++ )
    {
        if( chroma_table[i].i_chroma == i_chroma )
            return chroma_table[i].i_swscale_chroma;
    }
    return 0;
}
