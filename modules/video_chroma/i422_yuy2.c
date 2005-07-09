/*****************************************************************************
 * i422_yuy2.c : YUV to YUV conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 the VideoLAN team
 * $Id$
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
#include <string.h>                                            /* strerror() */
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include "i422_yuy2.h"

#define SRC_FOURCC  "I422"
#if defined (MODULE_NAME_IS_i422_yuy2)
#    define DEST_FOURCC "YUY2,YUNV,YVYU,UYVY,UYNV,Y422,IUYV,cyuv,Y211"
#else
#    define DEST_FOURCC "YUY2,YUNV,YVYU,UYVY,UYNV,Y422,IUYV,cyuv"
#endif

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static int  Activate ( vlc_object_t * );

static void I422_YUY2           ( vout_thread_t *, picture_t *, picture_t * );
static void I422_YVYU           ( vout_thread_t *, picture_t *, picture_t * );
static void I422_UYVY           ( vout_thread_t *, picture_t *, picture_t * );
static void I422_IUYV           ( vout_thread_t *, picture_t *, picture_t * );
static void I422_cyuv           ( vout_thread_t *, picture_t *, picture_t * );
#if defined (MODULE_NAME_IS_i422_yuy2)
static void I422_Y211           ( vout_thread_t *, picture_t *, picture_t * );
static void I422_Y211           ( vout_thread_t *, picture_t *, picture_t * );
static void I422_YV12           ( vout_thread_t *, picture_t *, picture_t * );
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
#if defined (MODULE_NAME_IS_i422_yuy2)
    set_description( _("Conversions from " SRC_FOURCC " to " DEST_FOURCC) );
    set_capability( "chroma", 80 );
#elif defined (MODULE_NAME_IS_i422_yuy2_mmx)
    set_description( _("MMX conversions from " SRC_FOURCC " to " DEST_FOURCC) );
    set_capability( "chroma", 100 );
    add_requirement( MMX );
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
        case VLC_FOURCC('I','4','2','2'):
            switch( p_vout->output.i_chroma )
            {
                case VLC_FOURCC('Y','U','Y','2'):
                case VLC_FOURCC('Y','U','N','V'):
                    p_vout->chroma.pf_convert = I422_YUY2;
                    break;

                case VLC_FOURCC('Y','V','Y','U'):
                    p_vout->chroma.pf_convert = I422_YVYU;
                    break;

                case VLC_FOURCC('U','Y','V','Y'):
                case VLC_FOURCC('U','Y','N','V'):
                case VLC_FOURCC('Y','4','2','2'):
                    p_vout->chroma.pf_convert = I422_UYVY;
                    break;

                case VLC_FOURCC('I','U','Y','V'):
                    p_vout->chroma.pf_convert = I422_IUYV;
                    break;

                case VLC_FOURCC('c','y','u','v'):
                    p_vout->chroma.pf_convert = I422_cyuv;
                    break;

#if defined (MODULE_NAME_IS_i422_yuy2)
                case VLC_FOURCC('Y','2','1','1'):
                    p_vout->chroma.pf_convert = I422_Y211;
                    break;

                case VLC_FOURCC('Y','V','1','2'):
                    p_vout->chroma.pf_convert = I422_YV12;
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
 * I422_YUY2: planar YUV 4:2:2 to packed YUY2 4:2:2
 *****************************************************************************/
static void I422_YUY2( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    uint8_t *p_line = p_dest->p->p_pixels;
    uint8_t *p_y = p_source->Y_PIXELS;
    uint8_t *p_u = p_source->U_PIXELS;
    uint8_t *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    for( i_y = p_vout->render.i_height ; i_y-- ; )
    {
        for( i_x = p_vout->render.i_width / 8 ; i_x-- ; )
        {
#if defined (MODULE_NAME_IS_i422_yuy2)
            C_YUV422_YUYV( p_line, p_y, p_u, p_v );
            C_YUV422_YUYV( p_line, p_y, p_u, p_v );
            C_YUV422_YUYV( p_line, p_y, p_u, p_v );
            C_YUV422_YUYV( p_line, p_y, p_u, p_v );
#else
            __asm__( ".align 8" MMX_YUV422_YUYV
                     : : "r" (p_line), "r" (p_y), "r" (p_u), "r" (p_v) ); 

            p_line += 16; p_y += 8; p_u += 4; p_v += 4;
#endif
        }
    }
}

/*****************************************************************************
 * I422_YVYU: planar YUV 4:2:2 to packed YVYU 4:2:2
 *****************************************************************************/
static void I422_YVYU( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    uint8_t *p_line = p_dest->p->p_pixels;
    uint8_t *p_y = p_source->Y_PIXELS;
    uint8_t *p_u = p_source->U_PIXELS;
    uint8_t *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    for( i_y = p_vout->render.i_height ; i_y-- ; )
    {
        for( i_x = p_vout->render.i_width / 8 ; i_x-- ; )
        {
#if defined (MODULE_NAME_IS_i422_yuy2)
            C_YUV422_YVYU( p_line, p_y, p_u, p_v );
            C_YUV422_YVYU( p_line, p_y, p_u, p_v );
            C_YUV422_YVYU( p_line, p_y, p_u, p_v );
            C_YUV422_YVYU( p_line, p_y, p_u, p_v );
#else
            __asm__( ".align 8" MMX_YUV422_YVYU
                     : : "r" (p_line), "r" (p_y), "r" (p_u), "r" (p_v) ); 

            p_line += 16; p_y += 8; p_u += 4; p_v += 4;
#endif
        }
    }
}

/*****************************************************************************
 * I422_UYVY: planar YUV 4:2:2 to packed UYVY 4:2:2
 *****************************************************************************/
static void I422_UYVY( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    uint8_t *p_line = p_dest->p->p_pixels;
    uint8_t *p_y = p_source->Y_PIXELS;
    uint8_t *p_u = p_source->U_PIXELS;
    uint8_t *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    for( i_y = p_vout->render.i_height ; i_y-- ; )
    {
        for( i_x = p_vout->render.i_width / 8 ; i_x-- ; )
        {
#if defined (MODULE_NAME_IS_i422_yuy2)
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
#else
            __asm__( ".align 8" MMX_YUV422_UYVY
                     : : "r" (p_line), "r" (p_y), "r" (p_u), "r" (p_v) ); 

            p_line += 16; p_y += 8; p_u += 4; p_v += 4;
#endif
        }
    }
}

/*****************************************************************************
 * I422_IUYV: planar YUV 4:2:2 to interleaved packed IUYV 4:2:2
 *****************************************************************************/
static void I422_IUYV( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    /* FIXME: TODO ! */
    msg_Err( p_vout, "I422_IUYV unimplemented, please harass <sam@zoy.org>" );
}

/*****************************************************************************
 * I422_cyuv: planar YUV 4:2:2 to upside-down packed UYVY 4:2:2
 *****************************************************************************/
static void I422_cyuv( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    uint8_t *p_line = p_dest->p->p_pixels + p_dest->p->i_visible_lines * p_dest->p->i_pitch;
    uint8_t *p_y = p_source->Y_PIXELS;
    uint8_t *p_u = p_source->U_PIXELS;
    uint8_t *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    for( i_y = p_vout->render.i_height ; i_y-- ; )
    {
        for( i_x = p_vout->render.i_width / 8 ; i_x-- ; )
        {
            p_line -= 2 * p_dest->p->i_pitch;

#if defined (MODULE_NAME_IS_i422_yuy2)
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
#else
            __asm__( ".align 8" MMX_YUV422_UYVY
                     : : "r" (p_line), "r" (p_y), "r" (p_u), "r" (p_v) ); 

            p_line += 16; p_y += 8; p_u += 4; p_v += 4;
#endif
        }
    }
}

/*****************************************************************************
 * I422_Y211: planar YUV 4:2:2 to packed YUYV 2:1:1
 *****************************************************************************/
#if defined (MODULE_NAME_IS_i422_yuy2)
static void I422_Y211( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    uint8_t *p_line = p_dest->p->p_pixels + p_dest->p->i_visible_lines * p_dest->p->i_pitch;
    uint8_t *p_y = p_source->Y_PIXELS;
    uint8_t *p_u = p_source->U_PIXELS;
    uint8_t *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    for( i_y = p_vout->render.i_height ; i_y-- ; )
    {
        for( i_x = p_vout->render.i_width / 8 ; i_x-- ; )
        {
            C_YUV422_Y211( p_line, p_y, p_u, p_v );
            C_YUV422_Y211( p_line, p_y, p_u, p_v );
        }
    }
}
#endif


/*****************************************************************************
 * I422_YV12: planar YUV 4:2:2 to planar YV12
 *****************************************************************************/
#if defined (MODULE_NAME_IS_i422_yuy2)
static void I422_YV12( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    uint16_t i_dpy = p_dest->p[Y_PLANE].i_pitch;
    uint16_t i_spy = p_source->p[Y_PLANE].i_pitch;
    uint16_t i_dpuv = p_dest->p[U_PLANE].i_pitch;
    uint16_t i_spuv = p_source->p[U_PLANE].i_pitch;
    uint16_t i_width = p_vout->render.i_width;
    uint16_t i_y = p_vout->render.i_height;
    uint8_t *p_dy = p_dest->Y_PIXELS + (i_y-1)*i_dpy;
    uint8_t *p_y = p_source->Y_PIXELS + (i_y-1)*i_spy;
    uint8_t *p_du = p_dest->U_PIXELS + (i_y/2-1)*i_dpuv;
    uint8_t *p_u = p_source->U_PIXELS + (i_y-1)*i_spuv;
    uint8_t *p_dv = p_dest->V_PIXELS + (i_y/2-1)*i_dpuv;
    uint8_t *p_v = p_source->V_PIXELS + (i_y-1)*i_spuv;
    i_y /= 2;

    for ( ; i_y--; )
    {
        memcpy(p_dy, p_y, i_width); p_dy -= i_dpy; p_y -= i_spy;
        memcpy(p_dy, p_y, i_width); p_dy -= i_dpy; p_y -= i_spy;
        memcpy(p_du, p_u, i_width/2); p_du -= i_dpuv; p_u -= 2*i_spuv;
        memcpy(p_dv, p_v, i_width/2); p_dv -= i_dpuv; p_v -= 2*i_spuv;
    }

}
#endif
