/*****************************************************************************
 * chroma.c: chroma conversion using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: chroma.c,v 1.1 2003/04/27 15:25:11 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/vout.h>

/* ffmpeg header */
#ifdef HAVE_FFMPEG_AVCODEC_H
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#include "ffmpeg.h"

void ChromaConversion( vout_thread_t *, picture_t *, picture_t * );

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
        switch( i_vlc_chroma[i] )
        {

        /* Planar YUV formats */
        case VLC_FOURCC('I','4','4','4'):
            i_ffmpeg_chroma[i] = PIX_FMT_YUV444P;
            break;

        case VLC_FOURCC('I','4','2','2'):
            i_ffmpeg_chroma[i] = PIX_FMT_YUV422P;
            break;

        case VLC_FOURCC('Y','V','1','2'):
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('I','Y','U','V'):
            i_ffmpeg_chroma[i] = PIX_FMT_YUV420P;
            break;

        case VLC_FOURCC('I','4','1','1'):
            i_ffmpeg_chroma[i] = PIX_FMT_YUV411P;
            break;

        case VLC_FOURCC('I','4','1','0'):
            i_ffmpeg_chroma[i] = PIX_FMT_YUV410P;
            break;

        /* Packed YUV formats */

        case VLC_FOURCC('Y','U','Y','2'):
        case VLC_FOURCC('U','Y','V','Y'):
            i_ffmpeg_chroma[i] = PIX_FMT_YUV422;
            break;
           
        /* Packed RGB formats */

        case VLC_FOURCC('R','V','3','2'):
            i_ffmpeg_chroma[i] = PIX_FMT_RGBA32;
            break;

        case VLC_FOURCC('R','V','2','4'):
            i_ffmpeg_chroma[i] = PIX_FMT_RGB24;
            //i_ffmpeg_chroma[i] = PIX_FMT_BGR24;
            break;

        case VLC_FOURCC('R','V','1','6'):
            i_ffmpeg_chroma[i] = PIX_FMT_RGB565;
            break;

        case VLC_FOURCC('R','V','1','5'):
            i_ffmpeg_chroma[i] = PIX_FMT_RGB555;
            break;

        case VLC_FOURCC('R','G','B','2'):
            i_ffmpeg_chroma[i] = PIX_FMT_GRAY8;
            break;

        default:
            return VLC_EGENERIC;
            break;
        }
    }

    p_vout->chroma.pf_convert = ChromaConversion;

    p_vout->chroma.p_sys = malloc( sizeof( chroma_sys_t ) );
    if( p_vout->chroma.p_sys == NULL )
    {
        return VLC_EGENERIC;
    }

    p_vout->chroma.p_sys->i_src_vlc_chroma = p_vout->render.i_chroma;
    p_vout->chroma.p_sys->i_dst_vlc_chroma = p_vout->output.i_chroma;
    p_vout->chroma.p_sys->i_src_ffmpeg_chroma = i_ffmpeg_chroma[0];
    p_vout->chroma.p_sys->i_dst_ffmpeg_chroma = i_ffmpeg_chroma[1];

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ChromaConversion: actual chroma conversion function
 *****************************************************************************/
void ChromaConversion( vout_thread_t *p_vout,
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
    if( p_vout->chroma.p_sys->i_src_vlc_chroma == VLC_FOURCC('Y','V','1','2') )
    {
        /* Invert U and V */
        src_pic.data[1] = p_src->p[2].p_pixels;
        src_pic.data[2] = p_src->p[1].p_pixels;
    }
    if( p_vout->chroma.p_sys->i_dst_vlc_chroma == VLC_FOURCC('Y','V','1','2') )
    {
        /* Invert U and V */
        dest_pic.data[1] = p_dest->p[2].p_pixels;
        dest_pic.data[2] = p_dest->p[1].p_pixels;
    }

    img_convert( &dest_pic, p_vout->chroma.p_sys->i_dst_ffmpeg_chroma,
                 &src_pic, p_vout->chroma.p_sys->i_src_ffmpeg_chroma,
                 p_vout->render.i_width, p_vout->render.i_height );
}
