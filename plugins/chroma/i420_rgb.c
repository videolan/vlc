/*****************************************************************************
 * i420_rgb.c : YUV to bitmap RGB conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: i420_rgb.c,v 1.5 2002/02/15 13:32:53 sam Exp $
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

#include "i420_rgb.h"

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static void chroma_getfunctions ( function_list_t * p_function_list );

static int  chroma_Init         ( vout_thread_t *p_vout );
static void chroma_End          ( vout_thread_t *p_vout );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
#if defined (MODULE_NAME_IS_chroma_i420_rgb)
    SET_DESCRIPTION( "I420/IYUV/YV12 to RGB 8/15/16/24/32 conversions" )
    ADD_CAPABILITY( CHROMA, 80 )
#elif defined (MODULE_NAME_IS_chroma_i420_rgb_mmx)
    SET_DESCRIPTION( "MMX I420/IYUV/YV12 to RGB 15/16/24/32 conversions" )
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
    p_function_list->functions.chroma.pf_init = chroma_Init;
    p_function_list->functions.chroma.pf_end  = chroma_End;
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
        case FOURCC_YV12:
        case FOURCC_I420:
        case FOURCC_IYUV:
            switch( p_vout->output.i_chroma )
            {
#if defined (MODULE_NAME_IS_chroma_i420_rgb)
                case FOURCC_BI_RGB:
                    p_vout->chroma.pf_convert = _M( I420_RGB8 );
                    break;
#endif
                case FOURCC_RV15:
                    p_vout->chroma.pf_convert = _M( I420_RGB15 );
                    break;

                case FOURCC_RV16:
                    p_vout->chroma.pf_convert = _M( I420_RGB16 );
                    break;

                case FOURCC_RV32:
                    p_vout->chroma.pf_convert = _M( I420_RGB32 );
                    break;

                default:
                    return -1;
            }
            break;

        default:
            return -1;
    }
    
    p_vout->chroma.p_sys = malloc( sizeof( chroma_sys_t ) );
    if( p_vout->chroma.p_sys == NULL )
    {
        return -1;
    }

    switch( p_vout->output.i_chroma )
    {
#if defined (MODULE_NAME_IS_chroma_i420_rgb)
        case FOURCC_BI_RGB:
            p_vout->chroma.p_sys->p_buffer = malloc( VOUT_MAX_WIDTH );
            break;
#endif

        case FOURCC_RV15:
        case FOURCC_RV16:
            p_vout->chroma.p_sys->p_buffer = malloc( VOUT_MAX_WIDTH * 2 );
            break;

        case FOURCC_RV32:
            p_vout->chroma.p_sys->p_buffer = malloc( VOUT_MAX_WIDTH * 4 );
            break;

        default:
            p_vout->chroma.p_sys->p_buffer = NULL;
            break;
    }

    if( p_vout->chroma.p_sys->p_buffer == NULL )
    {
        free( p_vout->chroma.p_sys );
        return -1;
    }

    p_vout->chroma.p_sys->p_offset = malloc( p_vout->output.i_width
                                              * sizeof( int ) );
    if( p_vout->chroma.p_sys->p_offset == NULL )
    {
        free( p_vout->chroma.p_sys->p_buffer );
        free( p_vout->chroma.p_sys );
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
    free( p_vout->chroma.p_sys->p_offset );
    free( p_vout->chroma.p_sys->p_buffer );
    free( p_vout->chroma.p_sys );
}

