/*****************************************************************************
 * video_yuv.c: YUV transformation functions
 * Provides functions to perform the YUV conversion. The functions provided here
 * are a complete and portable C implementation, and may be replaced in certain
 * case by optimized functions.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: video_yuv.c,v 1.14 2001/06/03 12:47:21 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

#define MODULE_NAME yuv
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <math.h>                                            /* exp(), pow() */
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "video.h"
#include "video_output.h"

#include "video_common.h"

#include "intf_msg.h"

#include "modules.h"
#include "modules_export.h"

static int     yuv_Probe      ( probedata_t *p_data );
static int     yuv_Init       ( vout_thread_t *p_vout );
static int     yuv_Reset      ( vout_thread_t *p_vout );
static void    yuv_End        ( vout_thread_t *p_vout );

static void    SetGammaTable  ( int *pi_table, double f_gamma );
static void    SetYUV         ( vout_thread_t *p_vout );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( yuv_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = yuv_Probe;
    p_function_list->functions.yuv.pf_init = yuv_Init;
    p_function_list->functions.yuv.pf_reset = yuv_Reset;
    p_function_list->functions.yuv.pf_end = yuv_End;
}

/*****************************************************************************
 * yuv_Probe: tests probe the audio device and return a score
 *****************************************************************************
 * This function tries to open the DSP and returns a score to the plugin
 * manager so that it can choose the most appropriate one.
 *****************************************************************************/
static int yuv_Probe( probedata_t *p_data )
{
    if( TestMethod( YUV_METHOD_VAR, "yuv" ) )
    {
        return( 999 );
    }

    /* This module always works */
    return( 50 );
}

/*****************************************************************************
 * yuv_Init: allocate and initialize translations tables
 *****************************************************************************
 * This function will allocate memory to store translation tables, depending
 * of the screen depth.
 *****************************************************************************/
static int yuv_Init( vout_thread_t *p_vout )
{
    size_t      tables_size;                        /* tables size, in bytes */

    /* Computes tables size - 3 Bpp use 32 bits pixel entries in tables */
    switch( p_vout->i_bytes_per_pixel )
    {
    case 1:
        tables_size = sizeof( u8 )
                * (p_vout->b_grayscale ? GRAY_TABLE_SIZE : PALETTE_TABLE_SIZE);
        break;
    case 2:
        tables_size = sizeof( u16 )
                * (p_vout->b_grayscale ? GRAY_TABLE_SIZE : RGB_TABLE_SIZE);
        break;
    case 3:
    case 4:
    default:
        tables_size = sizeof( u32 )
                * (p_vout->b_grayscale ? GRAY_TABLE_SIZE : RGB_TABLE_SIZE);
        break;
    }

    /* Allocate memory */
    p_vout->yuv.p_base = malloc( tables_size );
    if( p_vout->yuv.p_base == NULL )
    {
        intf_ErrMsg("error: %s", strerror(ENOMEM));
        return( 1 );
    }

    /* Allocate memory for conversion buffer and offset array */
    p_vout->yuv.p_buffer = malloc( VOUT_MAX_WIDTH * p_vout->i_bytes_per_pixel );
    if( p_vout->yuv.p_buffer == NULL )
    {
        intf_ErrMsg("error: %s", strerror(ENOMEM));
        free( p_vout->yuv.p_base );
        return( 1 );
    }

    /* In 8bpp we have a twice as big offset table because we also
     * need the offsets for U and V (not only Y) */
    p_vout->yuv.p_offset = malloc( p_vout->i_width * sizeof( int ) *
                             ( ( p_vout->i_bytes_per_pixel == 1 ) ? 2 : 1 ) );
    if( p_vout->yuv.p_offset == NULL )
    {
        intf_ErrMsg("error: %s", strerror(ENOMEM));
        free( p_vout->yuv.p_base );
        free( p_vout->yuv.p_buffer );
        return( 1 );
    }

    /* Initialize tables */
    SetYUV( p_vout );
    return( 0 );
}

/*****************************************************************************
 * yuv_End: destroy translations tables
 *****************************************************************************
 * Free memory allocated by yuv_CCreate.
 *****************************************************************************/
static void yuv_End( vout_thread_t *p_vout )
{
    free( p_vout->yuv.p_base );
    free( p_vout->yuv.p_buffer );
    free( p_vout->yuv.p_offset );
}

/*****************************************************************************
 * yuv_Reset: re-initialize translations tables
 *****************************************************************************
 * This function will initialize the tables allocated by vout_CreateTables and
 * set functions pointers.
 *****************************************************************************/
static int yuv_Reset( vout_thread_t *p_vout )
{
    yuv_End( p_vout );
    return( yuv_Init( p_vout ) );
}

/*****************************************************************************
 * SetGammaTable: return intensity table transformed by gamma curve.
 *****************************************************************************
 * pi_table is a table of 256 entries from 0 to 255.
 *****************************************************************************/
static void SetGammaTable( int *pi_table, double f_gamma )
{
    int         i_y;                                       /* base intensity */

    /* Use exp(gamma) instead of gamma */
    f_gamma = exp( f_gamma );

    /* Build gamma table */
    for( i_y = 0; i_y < 256; i_y++ )
    {
        pi_table[ i_y ] = pow( (double)i_y / 256, f_gamma ) * 256;
    }
 }

/*****************************************************************************
 * SetYUV: compute tables and set function pointers
 *****************************************************************************/
static void SetYUV( vout_thread_t *p_vout )
{
    int         pi_gamma[256];                                /* gamma table */
    int         i_index;                                  /* index in tables */

    /* Build gamma table */
    SetGammaTable( pi_gamma, p_vout->f_gamma );

    /*
     * Set pointers and build YUV tables
     */
    if( p_vout->b_grayscale )
    {
        /* Grayscale: build gray table */
        switch( p_vout->i_bytes_per_pixel )
        {
        case 1:
            {
                u16 bright[256], transp[256];

                p_vout->yuv.yuv.p_gray8 =  (u8 *)p_vout->yuv.p_base + GRAY_MARGIN;
                for( i_index = 0; i_index < GRAY_MARGIN; i_index++ )
                {
                    p_vout->yuv.yuv.p_gray8[ -i_index ] =      RGB2PIXEL( p_vout, pi_gamma[0], pi_gamma[0], pi_gamma[0] );
                    p_vout->yuv.yuv.p_gray8[ 256 + i_index ] = RGB2PIXEL( p_vout, pi_gamma[255], pi_gamma[255], pi_gamma[255] );
                }
                for( i_index = 0; i_index < 256; i_index++)
                {
                    p_vout->yuv.yuv.p_gray8[ i_index ] = pi_gamma[ i_index ];
                    bright[ i_index ] = i_index << 8;
                    transp[ i_index ] = 0;
                }
                /* the colors have been allocated, we can set the palette */
                p_vout->pf_setpalette( p_vout, bright, bright, bright, transp );
                p_vout->i_white_pixel = 0xff;
                p_vout->i_black_pixel = 0x00;
                p_vout->i_gray_pixel = 0x44;
                p_vout->i_blue_pixel = 0x3b;

                break;
            }
        case 2:
            p_vout->yuv.yuv.p_gray16 =  (u16 *)p_vout->yuv.p_base + GRAY_MARGIN;
            for( i_index = 0; i_index < GRAY_MARGIN; i_index++ )
            {
                p_vout->yuv.yuv.p_gray16[ -i_index ] =      RGB2PIXEL( p_vout, pi_gamma[0], pi_gamma[0], pi_gamma[0] );
                p_vout->yuv.yuv.p_gray16[ 256 + i_index ] = RGB2PIXEL( p_vout, pi_gamma[255], pi_gamma[255], pi_gamma[255] );
            }
            for( i_index = 0; i_index < 256; i_index++)
            {
                p_vout->yuv.yuv.p_gray16[ i_index ] = RGB2PIXEL( p_vout, pi_gamma[i_index], pi_gamma[i_index], pi_gamma[i_index] );
            }
            break;
        case 3:
        case 4:
            p_vout->yuv.yuv.p_gray32 =  (u32 *)p_vout->yuv.p_base + GRAY_MARGIN;
            for( i_index = 0; i_index < GRAY_MARGIN; i_index++ )
            {
                p_vout->yuv.yuv.p_gray32[ -i_index ] =      RGB2PIXEL( p_vout, pi_gamma[0], pi_gamma[0], pi_gamma[0] );
                p_vout->yuv.yuv.p_gray32[ 256 + i_index ] = RGB2PIXEL( p_vout, pi_gamma[255], pi_gamma[255], pi_gamma[255] );
            }
            for( i_index = 0; i_index < 256; i_index++)
            {
                p_vout->yuv.yuv.p_gray32[ i_index ] = RGB2PIXEL( p_vout, pi_gamma[i_index], pi_gamma[i_index], pi_gamma[i_index] );
            }
            break;
         }
    }
    else
    {
        /* Color: build red, green and blue tables */
        switch( p_vout->i_bytes_per_pixel )
        {
        case 1:
            {
                #define RGB_MIN 0
                #define RGB_MAX 255
                #define CLIP( x ) ( ((x < 0) ? 0 : (x > 255) ? 255 : x) << 8 )

                int y,u,v;
                int r,g,b;
                int uvr, uvg, uvb;
                int i = 0, j = 0;
                u16 red[256], green[256], blue[256], transp[256];
                unsigned char lookup[PALETTE_TABLE_SIZE];

                p_vout->yuv.yuv.p_rgb8 = (u8 *)p_vout->yuv.p_base;

                /* this loop calculates the intersection of an YUV box
                 * and the RGB cube. */
                for ( y = 0; y <= 256; y += 16 )
                {
                    for ( u = 0; u <= 256; u += 32 )
                    for ( v = 0; v <= 256; v += 32 )
                    {
                        uvr = (V_RED_COEF*(v-128)) >> SHIFT;
                        uvg = (U_GREEN_COEF*(u-128) + V_GREEN_COEF*(v-128)) >> SHIFT;
                        uvb = (U_BLUE_COEF*(u-128)) >> SHIFT;
                        r = y + uvr;
                        g = y + uvg;
                        b = y + uvb;

                        if( r >= RGB_MIN && g >= RGB_MIN && b >= RGB_MIN
                                && r <= RGB_MAX && g <= RGB_MAX && b <= RGB_MAX )
                        {
                            /* this one should never happen unless someone fscked up my code */
                            if(j == 256) { intf_ErrMsg( "vout error: no colors left to build palette" ); break; }

                            /* clip the colors */
                            red[j] = CLIP( r );
                            green[j] = CLIP( g );
                            blue[j] = CLIP( b );
                            transp[j] = 0;

                            /* allocate color */
                            lookup[i] = 1;
                            p_vout->yuv.yuv.p_rgb8[i++] = j;
                            j++;
                        }
                        else
                        {
                            lookup[i] = 0;
                            p_vout->yuv.yuv.p_rgb8[i++] = 0;
                        }
                    }
                    i += 128-81;
                }

                /* the colors have been allocated, we can set the palette */
                /* there will eventually be a way to know which colors
                 * couldn't be allocated and try to find a replacement */
                p_vout->pf_setpalette( p_vout, red, green, blue, transp );

                p_vout->i_white_pixel = 0xff;
                p_vout->i_black_pixel = 0x00;
                p_vout->i_gray_pixel = 0x44;
                p_vout->i_blue_pixel = 0x3b;

                i = 0;
                /* this loop allocates colors that got outside
                 * the RGB cube */
                for ( y = 0; y <= 256; y += 16 )
                {
                    for ( u = 0; u <= 256; u += 32 )
                    for ( v = 0; v <= 256; v += 32 )
                    {
                        int u2, v2;
                        int dist, mindist = 100000000;

                        if( lookup[i] || y==0)
                        {
                            i++;
                            continue;
                        }

                        /* heavy. yeah. */
                        for( u2 = 0; u2 <= 256; u2 += 32 )
                        for( v2 = 0; v2 <= 256; v2 += 32 )
                        {
                            j = ((y>>4)<<7) + (u2>>5)*9 + (v2>>5);
                            dist = (u-u2)*(u-u2) + (v-v2)*(v-v2);
                            if( lookup[j] )
                            /* find the nearest color */
                            if( dist < mindist )
                            {
                                p_vout->yuv.yuv.p_rgb8[i] = p_vout->yuv.yuv.p_rgb8[j];
                                mindist = dist;
                            }
                            j -= 128;
                            if( lookup[j] )
                            /* find the nearest color */
                            if( dist + 128 < mindist )
                            {
                                p_vout->yuv.yuv.p_rgb8[i] = p_vout->yuv.yuv.p_rgb8[j];
                                mindist = dist + 128;
                            }
                        }
                        i++;
                    }
                    i += 128-81;
                }

                break;
            }
        case 2:
            p_vout->yuv.yuv.p_rgb16 = (u16 *)p_vout->yuv.p_base;
            for( i_index = 0; i_index < RED_MARGIN; i_index++ )
            {
                p_vout->yuv.yuv.p_rgb16[RED_OFFSET - RED_MARGIN + i_index] = RGB2PIXEL( p_vout, pi_gamma[0], 0, 0 );
                p_vout->yuv.yuv.p_rgb16[RED_OFFSET + 256 + i_index] =        RGB2PIXEL( p_vout, pi_gamma[255], 0, 0 );
            }
            for( i_index = 0; i_index < GREEN_MARGIN; i_index++ )
            {
                p_vout->yuv.yuv.p_rgb16[GREEN_OFFSET - GREEN_MARGIN + i_index] = RGB2PIXEL( p_vout, 0, pi_gamma[0], 0 );
                p_vout->yuv.yuv.p_rgb16[GREEN_OFFSET + 256 + i_index] =          RGB2PIXEL( p_vout, 0, pi_gamma[255], 0 );
            }
            for( i_index = 0; i_index < BLUE_MARGIN; i_index++ )
            {
                p_vout->yuv.yuv.p_rgb16[BLUE_OFFSET - BLUE_MARGIN + i_index] = RGB2PIXEL( p_vout, 0, 0, pi_gamma[0] );
                p_vout->yuv.yuv.p_rgb16[BLUE_OFFSET + BLUE_MARGIN + i_index] = RGB2PIXEL( p_vout, 0, 0, pi_gamma[255] );
            }
            for( i_index = 0; i_index < 256; i_index++ )
            {
                p_vout->yuv.yuv.p_rgb16[RED_OFFSET + i_index] =   RGB2PIXEL( p_vout, pi_gamma[ i_index ], 0, 0 );
                p_vout->yuv.yuv.p_rgb16[GREEN_OFFSET + i_index] = RGB2PIXEL( p_vout, 0, pi_gamma[ i_index ], 0 );
                p_vout->yuv.yuv.p_rgb16[BLUE_OFFSET + i_index] =  RGB2PIXEL( p_vout, 0, 0, pi_gamma[ i_index ] );
            }
            break;
        case 3:
        case 4:
            p_vout->yuv.yuv.p_rgb32 = (u32 *)p_vout->yuv.p_base;
            for( i_index = 0; i_index < RED_MARGIN; i_index++ )
            {
                p_vout->yuv.yuv.p_rgb32[RED_OFFSET - RED_MARGIN + i_index] = RGB2PIXEL( p_vout, pi_gamma[0], 0, 0 );
                p_vout->yuv.yuv.p_rgb32[RED_OFFSET + 256 + i_index] =        RGB2PIXEL( p_vout, pi_gamma[255], 0, 0 );
            }
            for( i_index = 0; i_index < GREEN_MARGIN; i_index++ )
            {
                p_vout->yuv.yuv.p_rgb32[GREEN_OFFSET - GREEN_MARGIN + i_index] = RGB2PIXEL( p_vout, 0, pi_gamma[0], 0 );
                p_vout->yuv.yuv.p_rgb32[GREEN_OFFSET + 256 + i_index] =          RGB2PIXEL( p_vout, 0, pi_gamma[255], 0 );
            }
            for( i_index = 0; i_index < BLUE_MARGIN; i_index++ )
            {
                p_vout->yuv.yuv.p_rgb32[BLUE_OFFSET - BLUE_MARGIN + i_index] = RGB2PIXEL( p_vout, 0, 0, pi_gamma[0] );
                p_vout->yuv.yuv.p_rgb32[BLUE_OFFSET + BLUE_MARGIN + i_index] = RGB2PIXEL( p_vout, 0, 0, pi_gamma[255] );
            }
            for( i_index = 0; i_index < 256; i_index++ )
            {
                p_vout->yuv.yuv.p_rgb32[RED_OFFSET + i_index] =   RGB2PIXEL( p_vout, pi_gamma[ i_index ], 0, 0 );
                p_vout->yuv.yuv.p_rgb32[GREEN_OFFSET + i_index] = RGB2PIXEL( p_vout, 0, pi_gamma[ i_index ], 0 );
                p_vout->yuv.yuv.p_rgb32[BLUE_OFFSET + i_index] =  RGB2PIXEL( p_vout, 0, 0, pi_gamma[ i_index ] );
            }
            break;
        }
    }

    /*
     * Set functions pointers
     */
    if( p_vout->b_YCbr)
    {
        switch( p_vout->i_bytes_per_pixel)
        {
#define _X( foo ) (vout_yuv_convert_t *) _M( foo )
        case 1:
            p_vout->yuv.pf_yuv420 = _X( ConvertYUV420YCbr8 );
            p_vout->yuv.pf_yuv422 = _X( ConvertYUV422YCbr8 );
            p_vout->yuv.pf_yuv444 = _X( ConvertYUV444YCbr8 );
            break;
        
        case 2:
            p_vout->yuv.pf_yuv420 = _X( ConvertYUV420YCbr16 );
            p_vout->yuv.pf_yuv422 = _X( ConvertYUV422YCbr16 );
            p_vout->yuv.pf_yuv444 = _X( ConvertYUV444YCbr16 );
           break;
        
        case 3:
            p_vout->yuv.pf_yuv420 = _X( ConvertYUV420YCbr24 );
            p_vout->yuv.pf_yuv422 = _X( ConvertYUV422YCbr24 );
            p_vout->yuv.pf_yuv444 = _X( ConvertYUV444YCbr24 );
             break;
        
        case 4:
            p_vout->yuv.pf_yuv420 = _X( ConvertYUV420YCbr32 );
            p_vout->yuv.pf_yuv422 = _X( ConvertYUV422YCbr32 );
            p_vout->yuv.pf_yuv444 = _X( ConvertYUV444YCbr32 );
            break;
        }
#undef _X
    }    
    else if( p_vout->b_grayscale )
    {
        /* Grayscale */
        switch( p_vout->i_bytes_per_pixel )
        {
#define _X( foo ) (vout_yuv_convert_t *) _M( foo )
        case 1:
            p_vout->yuv.pf_yuv420 = _X( ConvertY4Gray8 );
            p_vout->yuv.pf_yuv422 = _X( ConvertY4Gray8 );
            p_vout->yuv.pf_yuv444 = _X( ConvertY4Gray8 );
            break;
        case 2:
            p_vout->yuv.pf_yuv420 = _X( ConvertY4Gray16 );
            p_vout->yuv.pf_yuv422 = _X( ConvertY4Gray16 );
            p_vout->yuv.pf_yuv444 = _X( ConvertY4Gray16 );
            break;
        case 3:
            p_vout->yuv.pf_yuv420 = _X( ConvertY4Gray24 );
            p_vout->yuv.pf_yuv422 = _X( ConvertY4Gray24 );
            p_vout->yuv.pf_yuv444 = _X( ConvertY4Gray24 );
            break;
        case 4:
            p_vout->yuv.pf_yuv420 = _X( ConvertY4Gray32 );
            p_vout->yuv.pf_yuv422 = _X( ConvertY4Gray32 );
            p_vout->yuv.pf_yuv444 = _X( ConvertY4Gray32 );
            break;
#undef _X
        }
    }
    else
    {
        /* Color */
        switch( p_vout->i_bytes_per_pixel )
        {
#define _X( foo ) (vout_yuv_convert_t *) _M( foo )
        case 1:
            p_vout->yuv.pf_yuv420 = _X( ConvertYUV420RGB8 );
            p_vout->yuv.pf_yuv422 = _X( ConvertYUV422RGB8 );
            p_vout->yuv.pf_yuv444 = _X( ConvertYUV444RGB8 );
            break;
        case 2:
            p_vout->yuv.pf_yuv420 = _X( ConvertYUV420RGB16 );
            p_vout->yuv.pf_yuv422 = _X( ConvertYUV422RGB16 );
            p_vout->yuv.pf_yuv444 = _X( ConvertYUV444RGB16 );
            break;
        case 3:
            p_vout->yuv.pf_yuv420 = _X( ConvertYUV420RGB24 );
            p_vout->yuv.pf_yuv422 = _X( ConvertYUV422RGB24 );
            p_vout->yuv.pf_yuv444 = _X( ConvertYUV444RGB24 );
            break;
        case 4:
            p_vout->yuv.pf_yuv420 = _X( ConvertYUV420RGB32 );
            p_vout->yuv.pf_yuv422 = _X( ConvertYUV422RGB32 );
            p_vout->yuv.pf_yuv444 = _X( ConvertYUV444RGB32 );
            break;
#undef _X
      }
 
    }
}

/*****************************************************************************
 * SetOffset: build offset array for conversion functions
 *****************************************************************************
 * This function will build an offset array used in later conversion functions.
 * It will also set horizontal and vertical scaling indicators. If b_double
 * is set, the p_offset structure has interleaved Y and U/V offsets.
 *****************************************************************************/
void _M( SetOffset )( int i_width, int i_height, int i_pic_width,
                      int i_pic_height, boolean_t *pb_h_scaling,
                      int *pi_v_scaling, int *p_offset, boolean_t b_double )
{
    int i_x;                                    /* x position in destination */
    int i_scale_count;                                     /* modulo counter */

    /*
     * Prepare horizontal offset array
     */
    if( i_pic_width - i_width == 0 )
    {
        /* No horizontal scaling: YUV conversion is done directly to picture */
        *pb_h_scaling = 0;
    }
    else if( i_pic_width - i_width > 0 )
    {
        /* Prepare scaling array for horizontal extension */
        *pb_h_scaling = 1;
        i_scale_count = i_pic_width;
        if( !b_double )
        {
            for( i_x = i_width; i_x--; )
            {
                while( (i_scale_count -= i_width) > 0 )
                {
                    *p_offset++ = 0;
                }
                *p_offset++ = 1;
                i_scale_count += i_pic_width;
            }
        }
        else
        {
            int i_dummy = 0;
            for( i_x = i_width; i_x--; )
            {
                while( (i_scale_count -= i_width) > 0 )
                {
                    *p_offset++ = 0;
                    *p_offset++ = 0;
                }
                *p_offset++ = 1;
                *p_offset++ = i_dummy;
                i_dummy = 1 - i_dummy;
                i_scale_count += i_pic_width;
            }
        }
    }
    else /* if( i_pic_width - i_width < 0 ) */
    {
        /* Prepare scaling array for horizontal reduction */
        *pb_h_scaling = 1;
        i_scale_count = i_width;
        if( !b_double )
        {
           for( i_x = i_pic_width; i_x--; )
            {
                *p_offset = 1;
                while( (i_scale_count -= i_pic_width) > 0 )
                {
                    *p_offset += 1;
                }
                p_offset++;
                i_scale_count += i_width;
            }
        }
        else
        {
            int i_remainder = 0;
            int i_jump;
            for( i_x = i_pic_width; i_x--; )
            {
                i_jump = 1;
                while( (i_scale_count -= i_pic_width) > 0 )
                {
                    i_jump += 1;
                }
                *p_offset++ = i_jump;
                *p_offset++ = ( i_jump += i_remainder ) >> 1;
                i_remainder = i_jump & 1;
                i_scale_count += i_width;
            }
        }
     }

    /*
     * Set vertical scaling indicator
     */
    if( i_pic_height - i_height == 0 )
    {
        *pi_v_scaling = 0;
    }
    else if( i_pic_height - i_height > 0 )
    {
        *pi_v_scaling = 1;
    }
    else /* if( i_pic_height - i_height < 0 ) */
    {
        *pi_v_scaling = -1;
    }
}

