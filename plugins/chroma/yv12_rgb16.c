/*****************************************************************************
 * yv12_rgb16.c : YUV to paletted RGB16 conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 * $Id: yv12_rgb16.c,v 1.1 2001/12/30 07:09:54 sam Exp $
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

#include "chroma_common.h"
#include "transforms.h"

/*****************************************************************************
 * chroma_sys_t: chroma method descriptor
 *****************************************************************************
 * This structure is part of the chroma transformation descriptor, it
 * describes the yuv2rgb16 specific properties.
 *****************************************************************************/
typedef struct chroma_sys_s
{
    u8 *p_tables;
    u8 *p_buffer;
    u8 *p_offset;
} chroma_sys_t;

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static void chroma_getfunctions ( function_list_t * p_function_list );

static int  chroma_Probe        ( probedata_t *p_data );
static int  chroma_Init         ( vout_thread_t *p_vout );
static void chroma_End          ( vout_thread_t *p_vout );

static void ConvertYUV420RGB16  ( vout_thread_t *, picture_t *, picture_t * );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "YV12 to RGB16 conversion module" )
    ADD_CAPABILITY( CHROMA, 80 )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( chroma_getfunctions )( &p_module->p_functions->chroma );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( chroma_getfunctions )( function_list_t * p_function_list )
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
    if( p_data->chroma.p_render->i_chroma != YUV_420_PICTURE
         || p_data->chroma.p_output->i_chroma != RGB_16BPP_PICTURE )
    {
        return 0;
    }

    return( 100 );
}

/*****************************************************************************
 * chroma_Init: allocate a chroma function
 *****************************************************************************
 * This function allocates and initializes a chroma function
 *****************************************************************************/
static int chroma_Init( vout_thread_t *p_vout )
{
    if( p_vout->render.i_chroma != YUV_420_PICTURE
         || p_vout->output.i_chroma != RGB_16BPP_PICTURE )
    {
        return -1;
    }

#if 0
    p_vout->chroma.p_sys = malloc( sizeof( chroma_sys_t ) );
    if( p_vout->chroma.p_sys == NULL )
    {
        return -1;
    }
#endif

    /* FIXME: this is really suboptimal ! */
    p_vout->chroma.pf_convert = ConvertYUV420RGB16;

    return 0; 
}

/*****************************************************************************
 * chroma_End: free the chroma function
 *****************************************************************************
 * This function frees the previously allocated chroma function
 *****************************************************************************/
static void chroma_End( vout_thread_t *p_vout )
{
#if 0
    free( p_vout->chroma.p_sys );
#endif

    return; 
}

/* Following functions are local */

/*****************************************************************************
 * ConvertYUV420RGB16: color YUV 4:2:0 to RGB 8 bpp
 *****************************************************************************/
static void ConvertYUV420RGB16( vout_thread_t *p_vout, picture_t *p_source,
                                                       picture_t *p_dest )
{
    /**********************************************************************
     *          XXX   XXX    FIXME      FIXME   XXX  XXX    XXX           *
     *          XXX   XXX   XXX TODO  TODO XXX  XXX  XXX   TODO           *
     *          XXX   XXX  XXX   XXX  XXX       XXX XXX    XXX            *
     *          XXX FIXME  XXX FIXME  XXX       FIXME     XXX             *
     *          XXX   XXX  XXX   XXX  XXX       XXX XXX   XXX             *
     *          XXX   XXX  XXX   XXX  TODO XXX  XXX  XXX                  *
     *          XXX   XXX  XXX   XXX    FIXME   XXX  XXX  XXX             *
     **********************************************************************/

    pixel_data_t *p_in, *p_in_end, *p_out, *p_out_end;

    p_in = p_source->planes[ Y_PLANE ].p_data;
    p_in_end = p_in + p_source->planes[ Y_PLANE ].i_bytes;

    p_out = p_dest->planes[ RGB_PLANE ].p_data;
    p_out_end = p_out + p_dest->planes[ RGB_PLANE ].i_bytes;

    while( p_in < p_in_end && p_out < p_out_end )
    {
        int i_src = p_source->planes[ Y_PLANE ].i_line_bytes;
        int i_dst = p_dest->planes[ RGB_PLANE ].i_line_bytes / 2;

        /* Masks: 0xf800 0x07e0 0x001f */
#define RED ((u16*)p_out)[i_dst--] = (u16)(p_in[i_src--]>>3) << 11;
#define GREEN ((u16*)p_out)[i_dst--] = (u16)(p_in[i_src--]>>2) << 5;
#define BLUE ((u16*)p_out)[i_dst--] = (u16)(p_in[i_src--]>>3) << 0;
#define WHITE ((u16*)p_out)[i_dst--] = ((u16)(p_in[i_src]>>3) << 11) | ((u16)(p_in[i_src]>>2) << 5) | ((u16)(p_in[i_src]>>3) << 0); i_src--;
#define BLACK ((u16*)p_out)[i_dst--] = 0; i_src--;
        
        while( i_src && i_dst )
        {
            BLACK; BLUE; GREEN; RED; GREEN; BLUE; WHITE; RED;
            GREEN; BLUE; WHITE; RED; BLACK; BLUE; GREEN; RED;
        }

        p_in += p_source->planes[ Y_PLANE ].i_line_bytes;
        p_out += p_dest->planes[ RGB_PLANE ].i_line_bytes;

        i_src = p_source->planes[ Y_PLANE ].i_line_bytes;
        i_dst = p_dest->planes[ RGB_PLANE ].i_line_bytes / 2;

        while( i_src && i_dst )
        {
            GREEN; RED; WHITE; BLUE; BLACK; RED; GREEN; BLUE;
            BLACK; RED; GREEN; BLUE; GREEN; RED; WHITE; BLUE;
        }

        p_in += p_source->planes[ Y_PLANE ].i_line_bytes;
        p_out += p_dest->planes[ RGB_PLANE ].i_line_bytes;
    }

    /**********************************************************************
     *          XXX   XXX  XXX   XXX    FIXME   XXX  XXX    XXX           *
     *          XXX   XXX  XXX   XXX  TODO XXX  XXX  XXX   TODO           *
     *           XXX XXX   XXX   XXX  XXX       XXX XXX    XXX            *
     *             TODO    XXX   XXX  XXX       FIXME     XXX             *
     *            TODO     XXX   XXX  XXX       XXX XXX   XXX             *
     *           XXX       TODO  XXX  TODO XXX  XXX  XXX                  *
     *          XXX          FIXME      FIXME   XXX  XXX  XXX             *
     **********************************************************************/
}

