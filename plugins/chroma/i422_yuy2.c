/*****************************************************************************
 * i422_yuy2.c : YUV to YUV conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: i422_yuy2.c,v 1.1 2002/01/04 14:01:34 sam Exp $
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
#include <math.h>                                            /* exp(), pow() */
#include <errno.h>                                                 /* ENOMEM */
#include <string.h>                                            /* strerror() */
#include <stdlib.h>                                      /* malloc(), free() */

#include <videolan/vlc.h>

#include "video.h"
#include "video_output.h"

#include "i422_yuy2.h"

#define SRC_FOURCC  "I422"
#if defined (MODULE_NAME_IS_chroma_i422_yuy2)
#    define DEST_FOURCC "YUY2/YUNV/YVYU/UYVY/UYNV/Y422/IUYV/cyuv/Y211"
#else
#    define DEST_FOURCC "YUY2/YUNV/YVYU/UYVY/UYNV/Y422/IUYV/cyuv"
#endif

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static void chroma_getfunctions ( function_list_t * p_function_list );

static int  chroma_Probe        ( probedata_t *p_data );
static int  chroma_Init         ( vout_thread_t *p_vout );
static void chroma_End          ( vout_thread_t *p_vout );

static void I422_YUY2           ( vout_thread_t *, picture_t *, picture_t * );
static void I422_YVYU           ( vout_thread_t *, picture_t *, picture_t * );
static void I422_UYVY           ( vout_thread_t *, picture_t *, picture_t * );
static void I422_IUYV           ( vout_thread_t *, picture_t *, picture_t * );
static void I422_cyuv           ( vout_thread_t *, picture_t *, picture_t * );
#if defined (MODULE_NAME_IS_chroma_i422_yuy2)
static void I422_Y211           ( vout_thread_t *, picture_t *, picture_t * );
#endif

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
#if defined (MODULE_NAME_IS_chroma_i422_yuy2)
    SET_DESCRIPTION( "conversions from " SRC_FOURCC " to " DEST_FOURCC )
    ADD_CAPABILITY( CHROMA, 80 )
#elif defined (MODULE_NAME_IS_chroma_i422_yuy2_mmx)
    SET_DESCRIPTION( "MMX conversions from " SRC_FOURCC " to " DEST_FOURCC )
    ADD_CAPABILITY( CHROMA, 100 )
    ADD_REQUIREMENT( MMX )
#endif
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    chroma_getfunctions( &p_module->p_functions->chroma );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void chroma_getfunctions( function_list_t * p_function_list )
{
    p_function_list->pf_probe = chroma_Probe;
    p_function_list->functions.chroma.pf_init = chroma_Init;
    p_function_list->functions.chroma.pf_end  = chroma_End;
}

/*****************************************************************************
 * chroma_Probe: return a score
 *****************************************************************************
 * This function checks that we can handle the required data
 *****************************************************************************/
static int chroma_Probe( probedata_t *p_data )
{
    if( p_data->chroma.p_render->i_width & 1
         || p_data->chroma.p_render->i_height & 1 )
    {
        return 0;
    }

    switch( p_data->chroma.p_render->i_chroma )
    {
        case FOURCC_I422:
            switch( p_data->chroma.p_output->i_chroma )
            {
                case FOURCC_YUY2:
                case FOURCC_YUNV:
                    break;

                case FOURCC_YVYU:
                    break;

                case FOURCC_UYVY:
                case FOURCC_UYNV:
                case FOURCC_Y422:
                    break;

                case FOURCC_IUYV:
                    break;

                case FOURCC_cyuv:
                    break;

#if defined (MODULE_NAME_IS_chroma_i422_yuy2)
                case FOURCC_Y211:
                    break;
#endif

                default:
                    return 0;
            }
            break;

        default:
            return 0;
    }

    return 100;
}

/*****************************************************************************
 * chroma_Init: allocate a chroma function
 *****************************************************************************
 * This function allocates and initializes a chroma function
 *****************************************************************************/
static int chroma_Init( vout_thread_t *p_vout )
{
    if( p_vout->render.i_width & 1 || p_vout->render.i_height & 1 )
    {
        return -1;
    }

    switch( p_vout->render.i_chroma )
    {
        case FOURCC_I422:
            switch( p_vout->output.i_chroma )
            {
                case FOURCC_YUY2:
                case FOURCC_YUNV:
                    p_vout->chroma.pf_convert = I422_YUY2;
                    break;

                case FOURCC_YVYU:
                    p_vout->chroma.pf_convert = I422_YVYU;
                    break;

                case FOURCC_UYVY:
                case FOURCC_UYNV:
                case FOURCC_Y422:
                    p_vout->chroma.pf_convert = I422_UYVY;
                    break;

                case FOURCC_IUYV:
                    p_vout->chroma.pf_convert = I422_IUYV;
                    break;

                case FOURCC_cyuv:
                    p_vout->chroma.pf_convert = I422_cyuv;
                    break;

#if defined (MODULE_NAME_IS_chroma_i422_yuy2)
                case FOURCC_Y211:
                    p_vout->chroma.pf_convert = I422_Y211;
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

/*****************************************************************************
 * chroma_End: free the chroma function
 *****************************************************************************
 * This function frees the previously allocated chroma function
 *****************************************************************************/
static void chroma_End( vout_thread_t *p_vout )
{
    ;
}

/* Following functions are local */

/*****************************************************************************
 * I422_YUY2: planar YUV 4:2:2 to packed YUY2 4:2:2
 *****************************************************************************/
static void I422_YUY2( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    u8 *p_line = p_dest->p->p_pixels;
    u8 *p_y = p_source->Y_PIXELS;
    u8 *p_u = p_source->U_PIXELS;
    u8 *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    for( i_y = p_vout->render.i_height ; i_y-- ; )
    {
        for( i_x = p_vout->render.i_width / 8 ; i_x-- ; )
        {
#if defined (MODULE_NAME_IS_chroma_i422_yuy2)
            C_YUV422_YUYV( p_line, p_y, p_u, p_v );
            C_YUV422_YUYV( p_line, p_y, p_u, p_v );
            C_YUV422_YUYV( p_line, p_y, p_u, p_v );
            C_YUV422_YUYV( p_line, p_y, p_u, p_v );
#else
            __asm__( ".align 8" MMX_YUV422_YUYV
                     : : "r" (p_line), "r" (p_y), "r" (p_u), "r" (p_v) ); 

            p_line += 8; p_y += 4; p_u += 2; p_v += 2;

            __asm__( ".align 8" MMX_YUV422_YUYV
                     : : "r" (p_line), "r" (p_y), "r" (p_u), "r" (p_v) ); 

            p_line += 8; p_y += 4; p_u += 2; p_v += 2;
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
    u8 *p_line = p_dest->p->p_pixels;
    u8 *p_y = p_source->Y_PIXELS;
    u8 *p_u = p_source->U_PIXELS;
    u8 *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    for( i_y = p_vout->render.i_height ; i_y-- ; )
    {
        for( i_x = p_vout->render.i_width / 8 ; i_x-- ; )
        {
#if defined (MODULE_NAME_IS_chroma_i422_yuy2)
            C_YUV422_YVYU( p_line, p_y, p_u, p_v );
            C_YUV422_YVYU( p_line, p_y, p_u, p_v );
            C_YUV422_YVYU( p_line, p_y, p_u, p_v );
            C_YUV422_YVYU( p_line, p_y, p_u, p_v );
#else
            __asm__( ".align 8" MMX_YUV422_YVYU
                     : : "r" (p_line), "r" (p_y), "r" (p_u), "r" (p_v) ); 

            p_line += 8; p_y += 4; p_u += 2; p_v += 2;

            __asm__( ".align 8" MMX_YUV422_YVYU
                     : : "r" (p_line), "r" (p_y), "r" (p_u), "r" (p_v) ); 

            p_line += 8; p_y += 4; p_u += 2; p_v += 2;
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
    u8 *p_line = p_dest->p->p_pixels;
    u8 *p_y = p_source->Y_PIXELS;
    u8 *p_u = p_source->U_PIXELS;
    u8 *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    for( i_y = p_vout->render.i_height ; i_y-- ; )
    {
        for( i_x = p_vout->render.i_width / 8 ; i_x-- ; )
        {
#if defined (MODULE_NAME_IS_chroma_i422_yuy2)
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
#else
            __asm__( ".align 8" MMX_YUV422_UYVY
                     : : "r" (p_line), "r" (p_y), "r" (p_u), "r" (p_v) ); 

            p_line += 8; p_y += 4; p_u += 2; p_v += 2;

            __asm__( ".align 8" MMX_YUV422_UYVY
                     : : "r" (p_line), "r" (p_y), "r" (p_u), "r" (p_v) ); 

            p_line += 8; p_y += 4; p_u += 2; p_v += 2;
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
    intf_ErrMsg( "chroma error: I422_IUYV unimplemented, "
                 "please harass <sam@zoy.org>" );
}

/*****************************************************************************
 * I422_cyuv: planar YUV 4:2:2 to upside-down packed UYVY 4:2:2
 *****************************************************************************/
static void I422_cyuv( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    u8 *p_line = p_dest->p->p_pixels + p_dest->p->i_lines * p_dest->p->i_pitch;
    u8 *p_y = p_source->Y_PIXELS;
    u8 *p_u = p_source->U_PIXELS;
    u8 *p_v = p_source->V_PIXELS;

    int i_x, i_y;

    for( i_y = p_vout->render.i_height ; i_y-- ; )
    {
        for( i_x = p_vout->render.i_width / 8 ; i_x-- ; )
        {
            p_line -= 2 * p_dest->p->i_pitch;

#if defined (MODULE_NAME_IS_chroma_i422_yuy2)
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
            C_YUV422_UYVY( p_line, p_y, p_u, p_v );
#else
            __asm__( ".align 8" MMX_YUV422_UYVY
                     : : "r" (p_line), "r" (p_y), "r" (p_u), "r" (p_v) ); 

            p_line += 8; p_y += 4; p_u += 2; p_v += 2;

            __asm__( ".align 8" MMX_YUV422_UYVY
                     : : "r" (p_line), "r" (p_y), "r" (p_u), "r" (p_v) ); 

            p_line += 8; p_y += 4; p_u += 2; p_v += 2;
#endif
        }
    }
}

/*****************************************************************************
 * I422_Y211: planar YUV 4:2:2 to packed YUYV 2:1:1
 *****************************************************************************/
#if defined (MODULE_NAME_IS_chroma_i422_yuy2)
static void I422_Y211( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    u8 *p_line = p_dest->p->p_pixels + p_dest->p->i_lines * p_dest->p->i_pitch;
    u8 *p_y = p_source->Y_PIXELS;
    u8 *p_u = p_source->U_PIXELS;
    u8 *p_v = p_source->V_PIXELS;

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

