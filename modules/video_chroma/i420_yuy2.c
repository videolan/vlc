/*****************************************************************************
 * i420_yuy2.c : YUV to YUV conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: i420_yuy2.c,v 1.1 2002/08/04 17:23:43 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <errno.h>                                                 /* ENOMEM */
#include <string.h>                                            /* strerror() */
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include "i420_yuy2.h"

#define SRC_FOURCC  "I420,IYUV,YV12"

#if defined (MODULE_NAME_IS_i420_yuy2)
#    define DEST_FOURCC "YUY2,YUNV,YVYU,UYVY,UYNV,Y422,IUYV,cyuv,Y211"
#else
#    define DEST_FOURCC "YUY2,YUNV,YVYU,UYVY,UYNV,Y422,IUYV,cyuv"
#endif

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static int  Activate ( vlc_object_t * );

static void I420_YUY2           ( vout_thread_t *, picture_t *, picture_t * );
static void I420_YVYU           ( vout_thread_t *, picture_t *, picture_t * );
static void I420_UYVY           ( vout_thread_t *, picture_t *, picture_t * );
static void I420_IUYV           ( vout_thread_t *, picture_t *, picture_t * );
static void I420_cyuv           ( vout_thread_t *, picture_t *, picture_t * );
#if defined (MODULE_NAME_IS_i420_yuy2)
static void I420_Y211           ( vout_thread_t *, picture_t *, picture_t * );
#endif

#ifdef MODULE_NAME_IS_i420_yuy2_mmx
static unsigned long long i_00ffw;
static unsigned long long i_80w;
#endif

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
vlc_module_begin();
#if defined (MODULE_NAME_IS_i420_yuy2)
    set_description( _("conversions from " SRC_FOURCC " to " DEST_FOURCC) );
    set_capability( "chroma", 80 );
#elif defined (MODULE_NAME_IS_i420_yuy2_mmx)
    set_description( _("MMX conversions from " SRC_FOURCC " to " DEST_FOURCC) );
    set_capability( "chroma", 100 );
    add_requirement( MMX );
    /* Initialize MMX-specific constants */
    i_00ffw = 0x00ff00ff00ff00ff;
    i_80w   = 0x0000000080808080;
#endif
    set_callbacks( Activate, NULL );
vlc_module_end();

/*****************************************************************************
 * Activate: allocate a chroma function
 *****************************************************************************
 * This function allocates and initializes a chroma function
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    if( p_vout->render.i_width & 1 || p_vout->render.i_height & 1 )
    {
        return -1;
    }

    switch( p_vout->render.i_chroma )
    {
        case VLC_FOURCC('Y','V','1','2'):
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('I','Y','U','V'):
            switch( p_vout->output.i_chroma )
            {
                case VLC_FOURCC('Y','U','Y','2'):
                case VLC_FOURCC('Y','U','N','V'):
                    p_vout->chroma.pf_convert = I420_YUY2;
                    break;

                case VLC_FOURCC('Y','V','Y','U'):
                    p_vout->chroma.pf_convert = I420_YVYU;
                    break;

                case VLC_FOURCC('U','Y','V','Y'):
                case VLC_FOURCC('U','Y','N','V'):
                case VLC_FOURCC('Y','4','2','2'):
                    p_vout->chroma.pf_convert = I420_UYVY;
                    break;

                case VLC_FOURCC('I','U','Y','V'):
                    p_vout->chroma.pf_convert = I420_IUYV;
                    break;

                case VLC_FOURCC('c','y','u','v'):
                    p_vout->chroma.pf_convert = I420_cyuv;
                    break;

#if defined (MODULE_NAME_IS_i420_yuy2)
                case VLC_FOURCC('Y','2','1','1'):
                    p_vout->chroma.pf_convert = I420_Y211;
                    break;
#endif

                default:
                    return -1;
            }
            break;

        default:
            return -1;
    }

    return 0;
}

/* Following functions are local */

/*****************************************************************************
 * I420_YUY2: planar YUV 4:2:0 to packed YUYV 4:2:2
 *****************************************************************************/
static void I420_YUY2( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    u8 *p_line1, *p_line2 = p_dest->p->p_pixels;
    u8 *p_y1, *p_y2 = p_source->Y_PIXELS;
    u8 *p_u = p_source->U_PIXELS;
    u8 *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    const int i_source_margin = p_source->p->i_pitch
                                 - p_source->p->i_visible_pitch;
    const int i_dest_margin = p_dest->p->i_pitch
                               - p_dest->p->i_visible_pitch;

    for( i_y = p_vout->render.i_height / 2 ; i_y-- ; )
    {
        p_line1 = p_line2;
        p_line2 += p_dest->p->i_pitch;

        p_y1 = p_y2;
        p_y2 += p_source->p[Y_PLANE].i_pitch;

        for( i_x = p_vout->render.i_width / 8 ; i_x-- ; )
        {
#if defined (MODULE_NAME_IS_i420_yuy2)
            C_YUV420_YUYV( );
            C_YUV420_YUYV( );
            C_YUV420_YUYV( );
            C_YUV420_YUYV( );
#else
            MMX_CALL( MMX_YUV420_YUYV );
#endif
        }

        p_y1 += i_source_margin;
        p_y2 += i_source_margin;
        p_line1 += i_dest_margin;
        p_line2 += i_dest_margin;
    }
}

/*****************************************************************************
 * I420_YVYU: planar YUV 4:2:0 to packed YVYU 4:2:2
 *****************************************************************************/
static void I420_YVYU( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    u8 *p_line1, *p_line2 = p_dest->p->p_pixels;
    u8 *p_y1, *p_y2 = p_source->Y_PIXELS;
    u8 *p_u = p_source->U_PIXELS;
    u8 *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    const int i_source_margin = p_source->p->i_pitch
                                 - p_source->p->i_visible_pitch;
    const int i_dest_margin = p_dest->p->i_pitch
                               - p_dest->p->i_visible_pitch;

    for( i_y = p_vout->render.i_height / 2 ; i_y-- ; )
    {
        p_line1 = p_line2;
        p_line2 += p_dest->p->i_pitch;

        p_y1 = p_y2;
        p_y2 += p_source->p[Y_PLANE].i_pitch;

        for( i_x = p_vout->render.i_width / 8 ; i_x-- ; )
        {
#if defined (MODULE_NAME_IS_i420_yuy2)
            C_YUV420_YVYU( );
            C_YUV420_YVYU( );
            C_YUV420_YVYU( );
            C_YUV420_YVYU( );
#else
            MMX_CALL( MMX_YUV420_YVYU );
#endif
        }

        p_y1 += i_source_margin;
        p_y2 += i_source_margin;
        p_line1 += i_dest_margin;
        p_line2 += i_dest_margin;
    }
}

/*****************************************************************************
 * I420_UYVY: planar YUV 4:2:0 to packed UYVY 4:2:2
 *****************************************************************************/
static void I420_UYVY( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    u8 *p_line1, *p_line2 = p_dest->p->p_pixels;
    u8 *p_y1, *p_y2 = p_source->Y_PIXELS;
    u8 *p_u = p_source->U_PIXELS;
    u8 *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    const int i_source_margin = p_source->p->i_pitch
                                 - p_source->p->i_visible_pitch;
    const int i_dest_margin = p_dest->p->i_pitch
                               - p_dest->p->i_visible_pitch;

    for( i_y = p_vout->render.i_height / 2 ; i_y-- ; )
    {
        p_line1 = p_line2;
        p_line2 += p_dest->p->i_pitch;

        p_y1 = p_y2;
        p_y2 += p_source->p[Y_PLANE].i_pitch;

        for( i_x = p_vout->render.i_width / 8 ; i_x-- ; )
        {
#if defined (MODULE_NAME_IS_i420_yuy2)
            C_YUV420_UYVY( );
            C_YUV420_UYVY( );
            C_YUV420_UYVY( );
            C_YUV420_UYVY( );
#else
            MMX_CALL( MMX_YUV420_UYVY );
#endif
        }

        p_y1 += i_source_margin;
        p_y2 += i_source_margin;
        p_line1 += i_dest_margin;
        p_line2 += i_dest_margin;
    }
}

/*****************************************************************************
 * I420_IUYV: planar YUV 4:2:0 to interleaved packed UYVY 4:2:2
 *****************************************************************************/
static void I420_IUYV( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    /* FIXME: TODO ! */
    msg_Err( p_vout, "I420_IUYV unimplemented, please harass <sam@zoy.org>" );
}

/*****************************************************************************
 * I420_cyuv: planar YUV 4:2:0 to upside-down packed UYVY 4:2:2
 *****************************************************************************/
static void I420_cyuv( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    u8 *p_line1 = p_dest->p->p_pixels + p_dest->p->i_lines * p_dest->p->i_pitch
                                      + p_dest->p->i_pitch;
    u8 *p_line2 = p_dest->p->p_pixels + p_dest->p->i_lines * p_dest->p->i_pitch;
    u8 *p_y1, *p_y2 = p_source->Y_PIXELS;
    u8 *p_u = p_source->U_PIXELS;
    u8 *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    const int i_source_margin = p_source->p->i_pitch
                                 - p_source->p->i_visible_pitch;
    const int i_dest_margin = p_dest->p->i_pitch
                               - p_dest->p->i_visible_pitch;

    for( i_y = p_vout->render.i_height / 2 ; i_y-- ; )
    {
        p_line1 -= 3 * p_dest->p->i_pitch;
        p_line2 -= 3 * p_dest->p->i_pitch;

        p_y1 = p_y2;
        p_y2 += p_source->p[Y_PLANE].i_pitch;

        for( i_x = p_vout->render.i_width / 8 ; i_x-- ; )
        {
#if defined (MODULE_NAME_IS_i420_yuy2)
            C_YUV420_UYVY( );
            C_YUV420_UYVY( );
            C_YUV420_UYVY( );
            C_YUV420_UYVY( );
#else
            MMX_CALL( MMX_YUV420_UYVY );
#endif
        }

        p_y1 += i_source_margin;
        p_y2 += i_source_margin;
        p_line1 += i_dest_margin;
        p_line2 += i_dest_margin;
    }
}

/*****************************************************************************
 * I420_Y211: planar YUV 4:2:0 to packed YUYV 2:1:1
 *****************************************************************************/
#if defined (MODULE_NAME_IS_i420_yuy2)
static void I420_Y211( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    u8 *p_line1, *p_line2 = p_dest->p->p_pixels;
    u8 *p_y1, *p_y2 = p_source->Y_PIXELS;
    u8 *p_u = p_source->U_PIXELS;
    u8 *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    const int i_source_margin = p_source->p->i_pitch
                                 - p_source->p->i_visible_pitch;
    const int i_dest_margin = p_dest->p->i_pitch
                               - p_dest->p->i_visible_pitch;

    for( i_y = p_vout->render.i_height / 2 ; i_y-- ; )
    {
        p_line1 = p_line2;
        p_line2 += p_dest->p->i_pitch;

        p_y1 = p_y2;
        p_y2 += p_source->p[Y_PLANE].i_pitch;

        for( i_x = p_vout->render.i_width / 8 ; i_x-- ; )
        {
            C_YUV420_Y211( );
            C_YUV420_Y211( );
        }

        p_y1 += i_source_margin;
        p_y2 += i_source_margin;
        p_line1 += i_dest_margin;
        p_line2 += i_dest_margin;
    }
}
#endif

