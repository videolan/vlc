/*****************************************************************************
 * i420_rgb.c : YUV to bitmap RGB conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: i420_rgb.c,v 1.8 2002/04/19 13:56:10 sam Exp $
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
#if defined (MODULE_NAME_IS_chroma_i420_rgb)
#   include "i420_rgb_c.h"
#endif

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static void chroma_getfunctions ( function_list_t * p_function_list );

static int  chroma_Init         ( vout_thread_t * );
static void chroma_End          ( vout_thread_t * );

#if defined (MODULE_NAME_IS_chroma_i420_rgb)
static void SetGammaTable       ( int *pi_table, double f_gamma );
static void SetYUV              ( vout_thread_t * );
static void Set8bppPalette      ( vout_thread_t *, u8 * );
#endif

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
#if defined (MODULE_NAME_IS_chroma_i420_rgb)
    SET_DESCRIPTION( _("I420,IYUV,YV12 to "
                       "RGB,RV15,RV16,RV24,RV32 conversions") )
    ADD_CAPABILITY( CHROMA, 80 )
#elif defined (MODULE_NAME_IS_chroma_i420_rgb_mmx)
    SET_DESCRIPTION( _( "MMX I420,IYUV,YV12 to "
                        "RV15,RV16,RV24,RV32 conversions") )
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
#if defined (MODULE_NAME_IS_chroma_i420_rgb)
    size_t i_tables_size;
#endif

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
                case FOURCC_RGB2:
                    p_vout->chroma.pf_convert = _M( I420_RGB8 );
                    break;
#endif
                case FOURCC_RV15:
                    p_vout->chroma.pf_convert = _M( I420_RGB15 );
                    break;

                case FOURCC_RV16:
                    p_vout->chroma.pf_convert = _M( I420_RGB16 );
                    break;

                case FOURCC_RV24:
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
        case FOURCC_RGB2:
            p_vout->chroma.p_sys->p_buffer = malloc( VOUT_MAX_WIDTH );
            break;
#endif

        case FOURCC_RV15:
        case FOURCC_RV16:
            p_vout->chroma.p_sys->p_buffer = malloc( VOUT_MAX_WIDTH * 2 );
            break;

        case FOURCC_RV24:
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
                    * ( ( p_vout->output.i_chroma == FOURCC_RGB2 ) ? 2 : 1 )
                    * sizeof( int ) );
    if( p_vout->chroma.p_sys->p_offset == NULL )
    {
        free( p_vout->chroma.p_sys->p_buffer );
        free( p_vout->chroma.p_sys );
        return -1;
    }

#if defined (MODULE_NAME_IS_chroma_i420_rgb)
    switch( p_vout->output.i_chroma )
    {
    case FOURCC_RGB2:
        i_tables_size = sizeof( u8 ) * PALETTE_TABLE_SIZE;
        break;
    case FOURCC_RV15:
    case FOURCC_RV16:
        i_tables_size = sizeof( u16 ) * RGB_TABLE_SIZE;
        break;
    default: /* RV24, RV32 */
        i_tables_size = sizeof( u32 ) * RGB_TABLE_SIZE;
        break;
    }

    p_vout->chroma.p_sys->p_base = malloc( i_tables_size );
    if( p_vout->chroma.p_sys->p_base == NULL )
    {
        free( p_vout->chroma.p_sys->p_offset );
        free( p_vout->chroma.p_sys->p_buffer );
        free( p_vout->chroma.p_sys );
        return -1;
    }

    SetYUV( p_vout );
#endif

    return 0; 
}

/*****************************************************************************
 * chroma_End: free the chroma function
 *****************************************************************************
 * This function frees the previously allocated chroma function
 *****************************************************************************/
static void chroma_End( vout_thread_t *p_vout )
{
#if defined (MODULE_NAME_IS_chroma_i420_rgb)
    free( p_vout->chroma.p_sys->p_base );
#endif
    free( p_vout->chroma.p_sys->p_offset );
    free( p_vout->chroma.p_sys->p_buffer );
    free( p_vout->chroma.p_sys );
}

#if defined (MODULE_NAME_IS_chroma_i420_rgb)
/*****************************************************************************
 * SetGammaTable: return intensity table transformed by gamma curve.
 *****************************************************************************
 * pi_table is a table of 256 entries from 0 to 255.
 *****************************************************************************/
static void SetGammaTable( int *pi_table, double f_gamma )
{
    int i_y;                                               /* base intensity */

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

    /* Color: build red, green and blue tables */
    switch( p_vout->output.i_chroma )
    {
    case FOURCC_RGB2:
        p_vout->chroma.p_sys->p_rgb8 = (u8 *)p_vout->chroma.p_sys->p_base;
        Set8bppPalette( p_vout, p_vout->chroma.p_sys->p_rgb8 );
        break;

    case FOURCC_RV15:
    case FOURCC_RV16:
        p_vout->chroma.p_sys->p_rgb16 = (u16 *)p_vout->chroma.p_sys->p_base;
        for( i_index = 0; i_index < RED_MARGIN; i_index++ )
        {
            p_vout->chroma.p_sys->p_rgb16[RED_OFFSET - RED_MARGIN + i_index] = RGB2PIXEL( p_vout, pi_gamma[0], 0, 0 );
            p_vout->chroma.p_sys->p_rgb16[RED_OFFSET + 256 + i_index] =        RGB2PIXEL( p_vout, pi_gamma[255], 0, 0 );
        }
        for( i_index = 0; i_index < GREEN_MARGIN; i_index++ )
        {
            p_vout->chroma.p_sys->p_rgb16[GREEN_OFFSET - GREEN_MARGIN + i_index] = RGB2PIXEL( p_vout, 0, pi_gamma[0], 0 );
            p_vout->chroma.p_sys->p_rgb16[GREEN_OFFSET + 256 + i_index] =          RGB2PIXEL( p_vout, 0, pi_gamma[255], 0 );
        }
        for( i_index = 0; i_index < BLUE_MARGIN; i_index++ )
        {
            p_vout->chroma.p_sys->p_rgb16[BLUE_OFFSET - BLUE_MARGIN + i_index] = RGB2PIXEL( p_vout, 0, 0, pi_gamma[0] );
            p_vout->chroma.p_sys->p_rgb16[BLUE_OFFSET + BLUE_MARGIN + i_index] = RGB2PIXEL( p_vout, 0, 0, pi_gamma[255] );
        }
        for( i_index = 0; i_index < 256; i_index++ )
        {
            p_vout->chroma.p_sys->p_rgb16[RED_OFFSET + i_index] =   RGB2PIXEL( p_vout, pi_gamma[ i_index ], 0, 0 );
            p_vout->chroma.p_sys->p_rgb16[GREEN_OFFSET + i_index] = RGB2PIXEL( p_vout, 0, pi_gamma[ i_index ], 0 );
            p_vout->chroma.p_sys->p_rgb16[BLUE_OFFSET + i_index] =  RGB2PIXEL( p_vout, 0, 0, pi_gamma[ i_index ] );
        }
        break;

    case FOURCC_RV24:
    case FOURCC_RV32:
        p_vout->chroma.p_sys->p_rgb32 = (u32 *)p_vout->chroma.p_sys->p_base;
        for( i_index = 0; i_index < RED_MARGIN; i_index++ )
        {
            p_vout->chroma.p_sys->p_rgb32[RED_OFFSET - RED_MARGIN + i_index] = RGB2PIXEL( p_vout, pi_gamma[0], 0, 0 );
            p_vout->chroma.p_sys->p_rgb32[RED_OFFSET + 256 + i_index] =        RGB2PIXEL( p_vout, pi_gamma[255], 0, 0 );
        }
        for( i_index = 0; i_index < GREEN_MARGIN; i_index++ )
        {
            p_vout->chroma.p_sys->p_rgb32[GREEN_OFFSET - GREEN_MARGIN + i_index] = RGB2PIXEL( p_vout, 0, pi_gamma[0], 0 );
            p_vout->chroma.p_sys->p_rgb32[GREEN_OFFSET + 256 + i_index] =          RGB2PIXEL( p_vout, 0, pi_gamma[255], 0 );
        }
        for( i_index = 0; i_index < BLUE_MARGIN; i_index++ )
        {
            p_vout->chroma.p_sys->p_rgb32[BLUE_OFFSET - BLUE_MARGIN + i_index] = RGB2PIXEL( p_vout, 0, 0, pi_gamma[0] );
            p_vout->chroma.p_sys->p_rgb32[BLUE_OFFSET + BLUE_MARGIN + i_index] = RGB2PIXEL( p_vout, 0, 0, pi_gamma[255] );
        }
        for( i_index = 0; i_index < 256; i_index++ )
        {
            p_vout->chroma.p_sys->p_rgb32[RED_OFFSET + i_index] =   RGB2PIXEL( p_vout, pi_gamma[ i_index ], 0, 0 );
            p_vout->chroma.p_sys->p_rgb32[GREEN_OFFSET + i_index] = RGB2PIXEL( p_vout, 0, pi_gamma[ i_index ], 0 );
            p_vout->chroma.p_sys->p_rgb32[BLUE_OFFSET + i_index] =  RGB2PIXEL( p_vout, 0, 0, pi_gamma[ i_index ] );
        }
        break;
    }
}

static void Set8bppPalette( vout_thread_t *p_vout, u8 *p_rgb8 )
{
    #define CLIP( x ) ( ((x < 0) ? 0 : (x > 255) ? 255 : x) << 8 )

    int y,u,v;
    int r,g,b;
    int i = 0, j = 0;
    u16 red[ 256 ], green[ 256 ], blue[ 256 ];
    unsigned char p_lookup[PALETTE_TABLE_SIZE];

    /* This loop calculates the intersection of an YUV box and the RGB cube. */
    for ( y = 0; y <= 256; y += 16, i += 128 - 81 )
    {
        for ( u = 0; u <= 256; u += 32 )
        {
            for ( v = 0; v <= 256; v += 32 )
            {
                r = y + ( (V_RED_COEF*(v-128)) >> SHIFT );
                g = y + ( (U_GREEN_COEF*(u-128)
                         + V_GREEN_COEF*(v-128)) >> SHIFT );
                b = y + ( (U_BLUE_COEF*(u-128)) >> SHIFT );

                if( r >= 0x00 && g >= 0x00 && b >= 0x00
                        && r <= 0xff && g <= 0xff && b <= 0xff )
                {
                    /* This one should never happen unless someone
                     * fscked up my code */
                    if( j == 256 )
                    {
                        intf_ErrMsg( "vout error: no colors left in palette" );
                        break;
                    }

                    /* Clip the colors */
                    red[ j ] = CLIP( r );
                    green[ j ] = CLIP( g );
                    blue[ j ] = CLIP( b );

                    /* Allocate color */
                    p_lookup[ i ] = 1;
                    p_rgb8[ i++ ] = j;
                    j++;
                }
                else
                {
                    p_lookup[ i ] = 0;
                    p_rgb8[ i++ ] = 0;
                }
            }
        }
    }

    /* The colors have been allocated, we can set the palette */
    p_vout->output.pf_setpalette( p_vout, red, green, blue );

#if 0
    /* There will eventually be a way to know which colors
     * couldn't be allocated and try to find a replacement */
    p_vout->i_white_pixel = 0xff;
    p_vout->i_black_pixel = 0x00;
    p_vout->i_gray_pixel = 0x44;
    p_vout->i_blue_pixel = 0x3b;
#endif

    /* This loop allocates colors that got outside the RGB cube */
    for ( i = 0, y = 0; y <= 256; y += 16, i += 128 - 81 )
    {
        for ( u = 0; u <= 256; u += 32 )
        {
            for ( v = 0; v <= 256; v += 32, i++ )
            {
                int u2, v2, dist, mindist = 100000000;

                if( p_lookup[ i ] || y == 0 )
                {
                    continue;
                }

                /* Heavy. yeah. */
                for( u2 = 0; u2 <= 256; u2 += 32 )
                {
                    for( v2 = 0; v2 <= 256; v2 += 32 )
                    {
                        j = ((y>>4)<<7) + (u2>>5)*9 + (v2>>5);
                        dist = (u-u2)*(u-u2) + (v-v2)*(v-v2);

                        /* Find the nearest color */
                        if( p_lookup[ j ] && dist < mindist )
                        {
                            p_rgb8[ i ] = p_rgb8[ j ];
                            mindist = dist;
                        }

                        j -= 128;

                        /* Find the nearest color */
                        if( p_lookup[ j ] && dist + 128 < mindist )
                        {
                            p_rgb8[ i ] = p_rgb8[ j ];
                            mindist = dist + 128;
                        }
                    }
                }
            }
        }
    }
}

#endif

