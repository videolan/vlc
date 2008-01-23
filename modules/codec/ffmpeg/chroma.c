/*****************************************************************************
 * chroma.c: chroma conversion using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#include <vlc/vlc.h>
#include <vlc_vout.h>

#if defined(HAVE_FFMPEG_SWSCALE_H) || defined(HAVE_LIBSWSCALE_TREE)
#include <vlc_filter.h>
#endif

/* ffmpeg header */
#ifdef HAVE_FFMPEG_AVCODEC_H
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#include "ffmpeg.h"

#if !defined(HAVE_FFMPEG_SWSCALE_H) && !defined(HAVE_LIBSWSCALE_TREE)
void E_(InitLibavcodec) ( vlc_object_t *p_object );
static void ChromaConversion( vout_thread_t *, picture_t *, picture_t * );

/*****************************************************************************
 * chroma_sys_t: chroma method descriptor
 *****************************************************************************
 * This structure is part of the chroma transformation descriptor, it
 * describes the chroma plugin specific properties.
 *****************************************************************************/
struct chroma_sys_t
{
    int i_src_vlc_chroma;
    int i_src_ffmpeg_chroma;
    int i_dst_vlc_chroma;
    int i_dst_ffmpeg_chroma;
    AVPicture tmp_pic;
    ImgReSampleContext *p_rsc;
};

/*****************************************************************************
 * OpenChroma: allocate a chroma function
 *****************************************************************************
 * This function allocates and initializes a chroma function
 *****************************************************************************/
int E_(OpenChroma)( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    int i_ffmpeg_chroma[2], i_vlc_chroma[2], i;

    /*
     * Check the source chroma first, then the destination chroma
     */
    i_vlc_chroma[0] = p_vout->render.i_chroma;
    i_vlc_chroma[1] = p_vout->output.i_chroma;
    for( i = 0; i < 2; i++ )
    {
        i_ffmpeg_chroma[i] = E_(GetFfmpegChroma)( i_vlc_chroma[i] );
        if( i_ffmpeg_chroma[i] < 0 ) return VLC_EGENERIC;
    }

    p_vout->chroma.pf_convert = ChromaConversion;

    p_vout->chroma.p_sys = malloc( sizeof( chroma_sys_t ) );
    if( p_vout->chroma.p_sys == NULL )
    {
        return VLC_ENOMEM;
    }

    p_vout->chroma.p_sys->i_src_vlc_chroma = p_vout->render.i_chroma;
    p_vout->chroma.p_sys->i_dst_vlc_chroma = p_vout->output.i_chroma;
    p_vout->chroma.p_sys->i_src_ffmpeg_chroma = i_ffmpeg_chroma[0];
    p_vout->chroma.p_sys->i_dst_ffmpeg_chroma = i_ffmpeg_chroma[1];

    if( ( p_vout->render.i_height != p_vout->output.i_height ||
          p_vout->render.i_width != p_vout->output.i_width ) &&
        ( p_vout->chroma.p_sys->i_dst_vlc_chroma == VLC_FOURCC('I','4','2','0') ||
          p_vout->chroma.p_sys->i_dst_vlc_chroma == VLC_FOURCC('Y','V','1','2') ))
    {
        msg_Dbg( p_vout, "preparing to resample picture" );
        p_vout->chroma.p_sys->p_rsc =
            img_resample_init( p_vout->output.i_width, p_vout->output.i_height,
                               p_vout->render.i_width, p_vout->render.i_height );
        avpicture_alloc( &p_vout->chroma.p_sys->tmp_pic,
                         p_vout->chroma.p_sys->i_dst_ffmpeg_chroma,
                         p_vout->render.i_width, p_vout->render.i_height );
    }
    else
    {
        msg_Dbg( p_vout, "no resampling" );
        p_vout->chroma.p_sys->p_rsc = NULL;
    }

    /* libavcodec needs to be initialized for some chroma conversions */
    E_(InitLibavcodec)(p_this);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ChromaConversion: actual chroma conversion function
 *****************************************************************************/
static void ChromaConversion( vout_thread_t *p_vout,
                              picture_t *p_src, picture_t *p_dest )
{
    AVPicture src_pic;
    AVPicture dest_pic;
    int i;

    /* Prepare the AVPictures for converion */
    for( i = 0; i < p_src->i_planes; i++ )
    {
        src_pic.data[i] = p_src->p[i].p_pixels;
        src_pic.linesize[i] = p_src->p[i].i_pitch;
    }
    for( i = 0; i < p_dest->i_planes; i++ )
    {
        dest_pic.data[i] = p_dest->p[i].p_pixels;
        dest_pic.linesize[i] = p_dest->p[i].i_pitch;
    }

    /* Special cases */
    if( p_vout->chroma.p_sys->i_src_vlc_chroma == VLC_FOURCC('Y','V','1','2') ||
        p_vout->chroma.p_sys->i_src_vlc_chroma == VLC_FOURCC('Y','V','U','9') )
    {
        /* Invert U and V */
        src_pic.data[1] = p_src->p[2].p_pixels;
        src_pic.data[2] = p_src->p[1].p_pixels;
    }
    if( p_vout->chroma.p_sys->i_dst_vlc_chroma == VLC_FOURCC('Y','V','1','2') ||
        p_vout->chroma.p_sys->i_dst_vlc_chroma == VLC_FOURCC('Y','V','U','9') )
    {
        /* Invert U and V */
        dest_pic.data[1] = p_dest->p[2].p_pixels;
        dest_pic.data[2] = p_dest->p[1].p_pixels;
    }
    if( p_vout->chroma.p_sys->i_src_ffmpeg_chroma == PIX_FMT_RGB24 )
        if( p_vout->render.i_bmask == 0x00ff0000 )
            p_vout->chroma.p_sys->i_src_ffmpeg_chroma = PIX_FMT_BGR24;

    if( p_vout->chroma.p_sys->p_rsc )
    {
        img_convert( &p_vout->chroma.p_sys->tmp_pic,
                     p_vout->chroma.p_sys->i_dst_ffmpeg_chroma,
                     &src_pic, p_vout->chroma.p_sys->i_src_ffmpeg_chroma,
                     p_vout->render.i_width, p_vout->render.i_height );
        img_resample( p_vout->chroma.p_sys->p_rsc, &dest_pic,
                      &p_vout->chroma.p_sys->tmp_pic );
    }
    else
    {
        img_convert( &dest_pic, p_vout->chroma.p_sys->i_dst_ffmpeg_chroma,
                     &src_pic, p_vout->chroma.p_sys->i_src_ffmpeg_chroma,
                     p_vout->render.i_width, p_vout->render.i_height );
    }
}

/*****************************************************************************
 * CloseChroma: free the chroma function
 *****************************************************************************
 * This function frees the previously allocated chroma function
 *****************************************************************************/
void E_(CloseChroma)( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    if( p_vout->chroma.p_sys->p_rsc )
    {
        img_resample_close( p_vout->chroma.p_sys->p_rsc );
        avpicture_free( &p_vout->chroma.p_sys->tmp_pic );
    }
    free( p_vout->chroma.p_sys );
}
#else

static void ChromaConversion( vout_thread_t *, picture_t *, picture_t * );

/*****************************************************************************
 * chroma_sys_t: chroma method descriptor
 *****************************************************************************
 * This structure is part of the chroma transformation descriptor, it
 * describes the chroma plugin specific properties.
 *****************************************************************************/
struct chroma_sys_t
{
    filter_t *p_swscaler;
};

/*****************************************************************************
 * Video Filter2 functions
 *****************************************************************************/
struct filter_owner_sys_t
{
    vout_thread_t *p_vout;
};

static void PictureRelease( picture_t *p_pic )
{
    if( p_pic->p_data_orig ) free( p_pic->p_data_orig );
}

static picture_t *video_new_buffer_filter( filter_t *p_filter )
{
    picture_t *p_picture = malloc( sizeof(picture_t) );
    if( !p_picture ) return NULL;
    if( vout_AllocatePicture( p_filter, p_picture,
                              p_filter->fmt_out.video.i_chroma,
                              p_filter->fmt_out.video.i_width,
                              p_filter->fmt_out.video.i_height,
                              p_filter->fmt_out.video.i_aspect )
        != VLC_SUCCESS )
    {
        free( p_picture );
        return NULL;
    }

    p_picture->pf_release = PictureRelease;

    return p_picture;
}

static void video_del_buffer_filter( filter_t *p_filter, picture_t *p_pic )
{
    (void)p_filter;
    if( p_pic && p_pic->p_data_orig ) free( p_pic->p_data_orig );
    if( p_pic ) free( p_pic );
}

/*****************************************************************************
 * OpenChroma: allocate a chroma function
 *****************************************************************************
 * This function allocates and initializes a chroma function
 *****************************************************************************/
int E_(OpenChroma)( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    chroma_sys_t  *p_sys = p_vout->chroma.p_sys;

    p_vout->chroma.p_sys = p_sys = malloc( sizeof( chroma_sys_t ) );
    if( p_vout->chroma.p_sys == NULL )
    {
        return VLC_ENOMEM;
    }
    p_vout->chroma.pf_convert = ChromaConversion;

    p_sys->p_swscaler = vlc_object_create( p_vout, VLC_OBJECT_FILTER );
    vlc_object_attach( p_sys->p_swscaler, p_vout );

    p_sys->p_swscaler->pf_vout_buffer_new = video_new_buffer_filter;
    p_sys->p_swscaler->pf_vout_buffer_del = video_del_buffer_filter;

    p_sys->p_swscaler->fmt_out.video.i_x_offset =
        p_sys->p_swscaler->fmt_out.video.i_y_offset = 0;
    p_sys->p_swscaler->fmt_in.video = p_vout->fmt_in;
    p_sys->p_swscaler->fmt_out.video = p_vout->fmt_out;
    p_sys->p_swscaler->fmt_out.video.i_aspect = p_vout->render.i_aspect;
    p_sys->p_swscaler->fmt_in.video.i_chroma = p_vout->render.i_chroma;
    p_sys->p_swscaler->fmt_out.video.i_chroma = p_vout->output.i_chroma;

    p_sys->p_swscaler->p_module = module_Need( p_sys->p_swscaler,
                           "video filter2", 0, 0 );

    if( p_sys->p_swscaler->p_module )
    {
        p_sys->p_swscaler->p_owner =
            malloc( sizeof( filter_owner_sys_t ) );
        if( p_sys->p_swscaler->p_owner )
            p_sys->p_swscaler->p_owner->p_vout = p_vout;
    }

    if( !p_sys->p_swscaler->p_module || !p_sys->p_swscaler->p_owner )
    {
        vlc_object_detach( p_sys->p_swscaler );
        vlc_object_destroy( p_sys->p_swscaler );
        free( p_vout->chroma.p_sys );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ChromaConversion: actual chroma conversion function
 *****************************************************************************/
static void ChromaConversion( vout_thread_t *p_vout,
                              picture_t *p_src, picture_t *p_dest )
{
    chroma_sys_t *p_sys = (chroma_sys_t *) p_vout->chroma.p_sys;

    if( p_sys && p_src && p_dest && 
        p_sys->p_swscaler && p_sys->p_swscaler->p_module )
    {
        picture_t *p_pic;

        p_sys->p_swscaler->fmt_in.video = p_vout->fmt_in;
        p_sys->p_swscaler->fmt_out.video = p_vout->fmt_out;

#if 0
        msg_Dbg( p_vout, "chroma %4.4s (%d) to %4.4s (%d)",
                 (char *)&p_vout->fmt_in.i_chroma, p_src->i_planes,
                 (char *)&p_vout->fmt_out.i_chroma, p_dest->i_planes  );
#endif
        p_pic = p_sys->p_swscaler->pf_vout_buffer_new( p_sys->p_swscaler );
        if( p_pic )
        {
            picture_t *p_dst_pic;
            vout_CopyPicture( p_vout, p_pic, p_src );
            p_dst_pic = p_sys->p_swscaler->pf_video_filter( p_sys->p_swscaler, p_pic );
            vout_CopyPicture( p_vout, p_dest, p_dst_pic );
            p_dst_pic->pf_release( p_dst_pic );
        }
    }
}

/*****************************************************************************
 * CloseChroma: free the chroma function
 *****************************************************************************
 * This function frees the previously allocated chroma function
 *****************************************************************************/
void E_(CloseChroma)( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    chroma_sys_t  *p_sys  = (chroma_sys_t *)p_vout->chroma.p_sys;
    if( p_sys->p_swscaler && p_sys->p_swscaler->p_module )
    {
        free( p_sys->p_swscaler->p_owner );
        module_Unneed( p_sys->p_swscaler, p_sys->p_swscaler->p_module );
        vlc_object_detach( p_sys->p_swscaler );
        vlc_object_destroy( p_sys->p_swscaler );
        p_sys->p_swscaler= NULL;
    }
    free( p_vout->chroma.p_sys );
}

#endif /* !defined(HAVE_FFMPEG_SWSCALE_H) && !defined(HAVE_LIBSWSCALE_TREE) */
