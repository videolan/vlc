/*****************************************************************************
 * yv12_rgb8.c : YUV to paletted RGB8 conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 * $Id: yv12_rgb8.c,v 1.3 2001/12/30 07:09:54 sam Exp $
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
 * describes the yuv2rgb8 specific properties.
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
static int  chroma_Init         ( vout_thread_t *p_vout, probedata_t *p_data );
static int  chroma_End          ( vout_thread_t *p_vout );

static void ConvertY4Gray8      ( vout_thread_t *, picture_t *, picture_t * );
static void ConvertYUV420RGB8   ( vout_thread_t *, picture_t *, picture_t * );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "YV12 to RGB8 conversion module" )
    ADD_CAPABILITY( CHROMA, 50 )
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
    if( p_data->chroma.source.i_chroma != YUV_420_PICTURE
         || p_data->chroma.dest.i_chroma != RGB_8BPP_PICTURE )
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
static int chroma_Init( vout_thread_t *p_vout, probedata_t *p_data )
{
    if( p_data->chroma.source.i_chroma != YUV_420_PICTURE
         || p_data->chroma.dest.i_chroma != RGB_8BPP_PICTURE )
    {
        return 0;
    }

    p_vout->chroma.p_sys = malloc( sizeof( chroma_sys_t ) );
    if( p_vout->chroma.p_sys == NULL )
    {
        return 0;
    }

    p_vout->chroma.p_sys->p_tables = malloc( sizeof( u8 ) *
                (p_vout->b_grayscale ? GRAY_TABLE_SIZE : PALETTE_TABLE_SIZE) );
    if( p_vout->chroma.p_sys->p_tables == NULL )
    {
        free( p_vout->chroma.p_sys );
        return 0;
    }

    p_vout->chroma.p_sys->p_buffer = malloc( sizeof( u8 ) * VOUT_MAX_WIDTH );
    if( p_vout->chroma.p_sys->p_buffer == NULL )
    {
        free( p_vout->chroma.p_sys->p_tables );
        free( p_vout->chroma.p_sys );
        return 0;
    }

    p_vout->chroma.p_sys->p_offset = malloc( sizeof( int ) *
                                           2 * p_data->chroma.dest.i_width );
    if( p_vout->chroma.p_sys->p_buffer == NULL )
    {
        free( p_vout->chroma.p_sys->p_offset );
        free( p_vout->chroma.p_sys->p_tables );
        free( p_vout->chroma.p_sys );
        return 0;
    }

    //SetYUV( p_vout );

    return 0; 
}

/*****************************************************************************
 * chroma_End: free the chroma function
 *****************************************************************************
 * This function frees the previously allocated chroma function
 *****************************************************************************/
static int chroma_End( vout_thread_t *p_vout )
{
    return 0; 
}

/* Following functions are local */

#if 0
/*****************************************************************************
 * ConvertY4Gray8: grayscale YUV 4:x:x to RGB 8 bpp
 *****************************************************************************/
static void ConvertY4Gray8( vout_thread_t *p_vout, picture_t *p_source,
                                                   picture_t *p_dest )
{
    boolean_t   b_horizontal_scaling;             /* horizontal scaling type */
    int         i_vertical_scaling;                 /* vertical scaling type */
    int         i_x, i_y;                 /* horizontal and vertical indexes */
    int         i_scale_count;                       /* scale modulo counter */
    int         i_chroma_width;                    /* chroma width, not used */
    u8 *        p_gray;                             /* base conversion table */
    u8 *        p_pic_start;       /* beginning of the current line for copy */
    u8 *        p_buffer_start;                   /* conversion buffer start */
    u8 *        p_buffer;                       /* conversion buffer pointer */
    int *       p_offset_start;                        /* offset array start */
    int *       p_offset;                            /* offset array pointer */

    /*
     * Initialize some values  - i_pic_line_width will store the line skip
     */
//    i_pic_line_width -= i_pic_width;
    p_gray =            p_vout->chroma.p_sys->p_gray8;
    p_buffer_start =    p_vout->chroma.p_sys->p_buffer;
    p_offset_start =    p_vout->chroma.p_sys->p_offset;
    _M( SetOffset )( i_width, i_height, i_pic_width, i_pic_height,
               &b_horizontal_scaling, &i_vertical_scaling, p_offset_start, 0 );

    /*
     * Perform conversion
     */
    i_scale_count = ( i_vertical_scaling == 1 ) ? i_pic_height : i_height;
    for( i_y = 0; i_y < i_height; i_y++ )
    {
        /* Mark beginnning of line for possible later line copy, and initialize
         * buffer */
        p_pic_start =   p_pic;
        p_buffer =      b_horizontal_scaling ? p_buffer_start : p_pic;

        /* Do YUV conversion to buffer - YUV picture is always formed of 16
         * pixels wide blocks */
        for( i_x = i_width / 16; i_x--;  )
        {
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
        }

        /* Do horizontal and vertical scaling */
        SCALE_WIDTH;
        SCALE_HEIGHT(400, 1);
    }
}
#endif

/*****************************************************************************
 * ConvertYUV420RGB8: color YUV 4:2:0 to RGB 8 bpp
 *****************************************************************************/
static void ConvertYUV420RGB8( vout_thread_t *p_vout, picture_t *p_source,
                                                      picture_t *p_dest )
{
    printf( "Colorspace transformation, YV12 to RGB8\n");
#if 0
    boolean_t   b_horizontal_scaling;             /* horizontal scaling type */
    int         i_vertical_scaling;                 /* vertical scaling type */
    int         i_x, i_y;                 /* horizontal and vertical indexes */
    int         i_scale_count;                       /* scale modulo counter */
    int         i_real_y;                                           /* y % 4 */
    u8 *        p_lookup;                                    /* lookup table */
    int         i_chroma_width;                              /* chroma width */
    u8 *        p_offset_start;                        /* offset array start */
    u8 *        p_offset;                            /* offset array pointer */

    int i_pic_line_width = p_source->i_width; /* XXX not sure */
    int i_width = p_source->i_width;
    int i_height = p_source->i_height;
    int i_pic_width = p_dest->i_width;
    int i_pic_height = p_dest->i_height;
    u8* p_y = p_source->planes[ Y_PLANE ].p_data;
    u8* p_u = p_source->planes[ U_PLANE ].p_data;
    u8* p_v = p_source->planes[ V_PLANE ].p_data;
    u8* p_pic = p_dest->planes[ RGB_PLANE ].p_data;

    /* 
     * The dithering matrices
     */
    static int dither10[4] = {  0x0,  0x8,  0x2,  0xa };
    static int dither11[4] = {  0xc,  0x4,  0xe,  0x6 };
    static int dither12[4] = {  0x3,  0xb,  0x1,  0x9 };
    static int dither13[4] = {  0xf,  0x7,  0xd,  0x5 };

    static int dither20[4] = {  0x0, 0x10,  0x4, 0x14 };
    static int dither21[4] = { 0x18,  0x8, 0x1c,  0xc };
    static int dither22[4] = {  0x6, 0x16,  0x2, 0x12 };
    static int dither23[4] = { 0x1e,  0xe, 0x1a,  0xa };

    /*
     * Initialize some values  - i_pic_line_width will store the line skip
     */
    i_pic_line_width -= p_dest->i_width; /*i_pic_width*/
    i_chroma_width =    p_source->i_width / 2;
    p_offset_start =    p_vout->chroma.p_sys->p_offset;
    p_lookup =          p_vout->chroma.p_sys->p_tables;
    _M( SetOffset )( p_source->i_width, p_source->i_height,
                     p_dest->i_width, p_dest->i_height,
                     &b_horizontal_scaling, &i_vertical_scaling,
                     p_offset_start, 1 );

    /*
     * Perform conversion
     */
    i_scale_count = ( i_vertical_scaling == 1 ) ? p_dest->i_height : p_source->i_height;
    i_real_y = 0;
    for( i_y = 0; i_y < p_source->i_height; i_y++ )
    {
        /* Do horizontal and vertical scaling */
        SCALE_WIDTH_DITHER( 420 );
        SCALE_HEIGHT_DITHER( 420 );
    }
#endif
}

