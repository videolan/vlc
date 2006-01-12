/*****************************************************************************
 * i420_rgb.c : YUV to bitmap RGB conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001, 2004 the VideoLAN team
 * $Id$
 *
 * Author: Sam Hocevar <sam@zoy.org>
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
#include <math.h>                                            /* exp(), pow() */
#include <string.h>                                            /* strerror() */
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include "i420_rgb.h"
#if defined (MODULE_NAME_IS_i420_rgb)
#   include "i420_rgb_c.h"
#endif

/*****************************************************************************
 * RGB2PIXEL: assemble RGB components to a pixel value, returns a uint32_t
 *****************************************************************************/
#define RGB2PIXEL( p_vout, i_r, i_g, i_b )          \
    (((((uint32_t)i_r) >> p_vout->output.i_rrshift) \
                       << p_vout->output.i_lrshift) \
   | ((((uint32_t)i_g) >> p_vout->output.i_rgshift) \
                       << p_vout->output.i_lgshift) \
   | ((((uint32_t)i_b) >> p_vout->output.i_rbshift) \
                       << p_vout->output.i_lbshift))

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static int  Activate   ( vlc_object_t * );
static void Deactivate ( vlc_object_t * );

#if defined (MODULE_NAME_IS_i420_rgb)
static void SetGammaTable       ( int *pi_table, double f_gamma );
static void SetYUV              ( vout_thread_t * );
static void Set8bppPalette      ( vout_thread_t *, uint8_t * );
#endif

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
vlc_module_begin();
#if defined (MODULE_NAME_IS_i420_rgb)
    set_description( _("I420,IYUV,YV12 to "
                       "RGB2,RV15,RV16,RV24,RV32 conversions") );
    set_capability( "chroma", 80 );
#elif defined (MODULE_NAME_IS_i420_rgb_mmx)
    set_description( _( "MMX I420,IYUV,YV12 to "
                        "RV15,RV16,RV24,RV32 conversions") );
    set_capability( "chroma", 100 );
    add_requirement( MMX );
#endif
    set_callbacks( Activate, Deactivate );
vlc_module_end();

/*****************************************************************************
 * Activate: allocate a chroma function
 *****************************************************************************
 * This function allocates and initializes a chroma function
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
#if defined (MODULE_NAME_IS_i420_rgb)
    size_t i_tables_size;
#endif

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
#if defined (MODULE_NAME_IS_i420_rgb)
                case VLC_FOURCC('R','G','B','2'):
                    p_vout->chroma.pf_convert = E_(I420_RGB8);
                    break;
#endif
                case VLC_FOURCC('R','V','1','5'):
                case VLC_FOURCC('R','V','1','6'):
#if defined (MODULE_NAME_IS_i420_rgb_mmx)
                    /* If we don't have support for the bitmasks, bail out */
                    if( ( p_vout->output.i_rmask != 0x7c00
                           || p_vout->output.i_gmask != 0x03e0
                           || p_vout->output.i_bmask != 0x001f )
                     && ( p_vout->output.i_rmask != 0xf800
                           || p_vout->output.i_gmask != 0x07e0
                           || p_vout->output.i_bmask != 0x001f ) )
                    {
                        return -1;
                    }
#endif
                    p_vout->chroma.pf_convert = E_(I420_RGB16);
                    break;

#if 0
                /* Hmmm, is there only X11 using 32bits per pixel for RV24 ? */
                case VLC_FOURCC('R','V','2','4'):
#endif

                case VLC_FOURCC('R','V','3','2'):
#if defined (MODULE_NAME_IS_i420_rgb_mmx)
                    /* If we don't have support for the bitmasks, bail out */
                    if( p_vout->output.i_rmask != 0x00ff0000
                         || p_vout->output.i_gmask != 0x0000ff00
                         || p_vout->output.i_bmask != 0x000000ff )
                    {
                        return -1;
                    }
#endif
                    p_vout->chroma.pf_convert = E_(I420_RGB32);
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
#if defined (MODULE_NAME_IS_i420_rgb)
        case VLC_FOURCC('R','G','B','2'):
            p_vout->chroma.p_sys->p_buffer = malloc( VOUT_MAX_WIDTH );
            break;
#endif

        case VLC_FOURCC('R','V','1','5'):
        case VLC_FOURCC('R','V','1','6'):
            p_vout->chroma.p_sys->p_buffer = malloc( VOUT_MAX_WIDTH * 2 );
            break;

        case VLC_FOURCC('R','V','2','4'):
        case VLC_FOURCC('R','V','3','2'):
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
                    * ( ( p_vout->output.i_chroma
                           == VLC_FOURCC('R','G','B','2') ) ? 2 : 1 )
                    * sizeof( int ) );
    if( p_vout->chroma.p_sys->p_offset == NULL )
    {
        free( p_vout->chroma.p_sys->p_buffer );
        free( p_vout->chroma.p_sys );
        return -1;
    }

#if defined (MODULE_NAME_IS_i420_rgb)
    switch( p_vout->output.i_chroma )
    {
    case VLC_FOURCC('R','G','B','2'):
        i_tables_size = sizeof( uint8_t ) * PALETTE_TABLE_SIZE;
        break;
    case VLC_FOURCC('R','V','1','5'):
    case VLC_FOURCC('R','V','1','6'):
        i_tables_size = sizeof( uint16_t ) * RGB_TABLE_SIZE;
        break;
    default: /* RV24, RV32 */
        i_tables_size = sizeof( uint32_t ) * RGB_TABLE_SIZE;
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
 * Deactivate: free the chroma function
 *****************************************************************************
 * This function frees the previously allocated chroma function
 *****************************************************************************/
static void Deactivate( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

#if defined (MODULE_NAME_IS_i420_rgb)
    free( p_vout->chroma.p_sys->p_base );
#endif
    free( p_vout->chroma.p_sys->p_offset );
    free( p_vout->chroma.p_sys->p_buffer );
    free( p_vout->chroma.p_sys );
}

#if defined (MODULE_NAME_IS_i420_rgb)
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
        pi_table[ i_y ] = (int)( pow( (double)i_y / 256, f_gamma ) * 256 );
    }
}

/*****************************************************************************
 * SetYUV: compute tables and set function pointers
 *****************************************************************************/
static void SetYUV( vout_thread_t *p_vout )
{
    int          pi_gamma[256];                               /* gamma table */
    volatile int i_index;                                 /* index in tables */
                   /* We use volatile here to work around a strange gcc-3.3.4
                    * optimization bug */

    /* Build gamma table */
    SetGammaTable( pi_gamma, p_vout->f_gamma );

    /*
     * Set pointers and build YUV tables
     */

    /* Color: build red, green and blue tables */
    switch( p_vout->output.i_chroma )
    {
    case VLC_FOURCC('R','G','B','2'):
        p_vout->chroma.p_sys->p_rgb8 = (uint8_t *)p_vout->chroma.p_sys->p_base;
        Set8bppPalette( p_vout, p_vout->chroma.p_sys->p_rgb8 );
        break;

    case VLC_FOURCC('R','V','1','5'):
    case VLC_FOURCC('R','V','1','6'):
        p_vout->chroma.p_sys->p_rgb16 = (uint16_t *)p_vout->chroma.p_sys->p_base;
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

    case VLC_FOURCC('R','V','2','4'):
    case VLC_FOURCC('R','V','3','2'):
        p_vout->chroma.p_sys->p_rgb32 = (uint32_t *)p_vout->chroma.p_sys->p_base;
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

static void Set8bppPalette( vout_thread_t *p_vout, uint8_t *p_rgb8 )
{
    #define CLIP( x ) ( ((x < 0) ? 0 : (x > 255) ? 255 : x) << 8 )

    int y,u,v;
    int r,g,b;
    int i = 0, j = 0;
    uint16_t *p_cmap_r=p_vout->chroma.p_sys->p_rgb_r;
    uint16_t *p_cmap_g=p_vout->chroma.p_sys->p_rgb_g;
    uint16_t *p_cmap_b=p_vout->chroma.p_sys->p_rgb_b;

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
                        msg_Err( p_vout, "no colors left in palette" );
                        break;
                    }

                    /* Clip the colors */
                    p_cmap_r[ j ] = CLIP( r );
                    p_cmap_g[ j ] = CLIP( g );
                    p_cmap_b[ j ] = CLIP( b );

#if 0
		    printf("+++Alloc RGB cmap %d (%d, %d, %d)\n", j,
			   p_cmap_r[ j ] >>8, p_cmap_g[ j ] >>8, 
			   p_cmap_b[ j ] >>8);
#endif

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
    p_vout->output.pf_setpalette( p_vout, p_cmap_r, p_cmap_g, p_cmap_b );

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

