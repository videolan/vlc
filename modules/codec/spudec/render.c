/*****************************************************************************
 * render.c : SPU renderer
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: render.c,v 1.3 2002/11/06 18:07:57 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Rudolf Cornelissen <rag.cornelissen@inter.nl.net>
 *          Roine Gustafsson <roine@popstar.com>
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
#include <string.h>                                    /* memcpy(), memset() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>                                           /* getpid() */
#endif

#ifdef WIN32                   /* getpid() for win32 is located in process.h */
#   include <process.h>
#endif

#include "spudec.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void RenderI420( vout_thread_t *, picture_t *, const subpicture_t *,
                        vlc_bool_t );
static void RenderRV16( vout_thread_t *, picture_t *, const subpicture_t *,
                        vlc_bool_t );
static void RenderRV32( vout_thread_t *, picture_t *, const subpicture_t *,
                        vlc_bool_t );
static void RenderYUY2( vout_thread_t *, picture_t *, const subpicture_t *,
                        vlc_bool_t );

/*****************************************************************************
 * RenderSPU: draw an SPU on a picture
 *****************************************************************************
 * This is a fast implementation of the subpicture drawing code. The data
 * has been preprocessed once, so we don't need to parse the RLE buffer again
 * and again. Most sanity checks are already done so that this routine can be
 * as fast as possible.
 *****************************************************************************/
void E_(RenderSPU)( vout_thread_t *p_vout, picture_t *p_pic,
                    const subpicture_t *p_spu )
{
    switch( p_vout->output.i_chroma )
    {
        /* I420 target, no scaling */
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('I','Y','U','V'):
        case VLC_FOURCC('Y','V','1','2'):
            RenderI420( p_vout, p_pic, p_spu, p_spu->p_sys->b_crop );
            break;

        /* RV16 target, scaling */
        case VLC_FOURCC('R','V','1','6'):
            RenderRV16( p_vout, p_pic, p_spu, p_spu->p_sys->b_crop );
            break;

        /* RV32 target, scaling */
        case VLC_FOURCC('R','V','2','4'):
        case VLC_FOURCC('R','V','3','2'):
            RenderRV32( p_vout, p_pic, p_spu, p_spu->p_sys->b_crop );
            break;

        /* NVidia overlay, no scaling */
        case VLC_FOURCC('Y','U','Y','2'):
            RenderYUY2( p_vout, p_pic, p_spu, p_spu->p_sys->b_crop );
            break;

        default:
            msg_Err( p_vout, "unknown chroma, can't render SPU" );
            break;
    }
}

/* Following functions are local */

static void RenderI420( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu, vlc_bool_t b_crop )
{
    /* Common variables */
    u8  *p_dest;
    u8  *p_destptr;
    u16 *p_source = (u16 *)p_spu->p_sys->p_data;

    int i_x, i_y;
    int i_len, i_color;
    u16 i_colprecomp, i_destalpha;

    /* Crop-specific */
    int i_x_start, i_y_start, i_x_end, i_y_end;

    p_dest = p_pic->Y_PIXELS + p_spu->i_x + p_spu->i_width
              + p_pic->Y_PITCH * ( p_spu->i_y + p_spu->i_height );

    i_x_start = p_spu->i_width - p_spu->p_sys->i_x_end;
    i_y_start = p_pic->Y_PITCH * (p_spu->i_height - p_spu->p_sys->i_y_end );
    i_x_end = p_spu->i_width - p_spu->p_sys->i_x_start;
    i_y_end = p_pic->Y_PITCH * (p_spu->i_height - p_spu->p_sys->i_y_start );

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = p_spu->i_height * p_pic->Y_PITCH ;
         i_y ;
         i_y -= p_pic->Y_PITCH )
    {
        /* Draw until we reach the end of the line */
        for( i_x = p_spu->i_width ; i_x ; i_x -= i_len )
        {
            /* Get the RLE part, then draw the line */
            i_color = *p_source & 0x3;
            i_len = *p_source++ >> 2;

            if( b_crop
                 && ( i_x < i_x_start || i_x > i_x_end
                       || i_y < i_y_start || i_y > i_y_end ) )
            {
                continue;
            }

            switch( p_spu->p_sys->pi_alpha[i_color] )
            {
                case 0x00:
                    break;

                case 0x0f:
                    memset( p_dest - i_x - i_y,
                            p_spu->p_sys->pi_yuv[i_color][0], i_len );
                    break;

                default:
                    /* To be able to divide by 16 (>>4) we add 1 to the alpha.
                     * This means Alpha 0 won't be completely transparent, but
                     * that's handled in a special case above anyway. */
                    i_colprecomp = (u16)p_spu->p_sys->pi_yuv[i_color][0]
                                 * (u16)(p_spu->p_sys->pi_alpha[i_color] + 1);
                    i_destalpha = 15 - p_spu->p_sys->pi_alpha[i_color];

                    for ( p_destptr = p_dest - i_x - i_y;
                          p_destptr < p_dest - i_x - i_y + i_len;
                          p_destptr++ )
                    {
                        *p_destptr = ( i_colprecomp +
                                        (u16)*p_destptr * i_destalpha ) >> 4;
                    }
                    break;
            }
        }
    }
}

static void RenderRV16( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu, vlc_bool_t b_crop )
{
    /* Common variables */
    u16  p_clut16[4];
    u8  *p_dest;
    u16 *p_source = (u16 *)p_spu->p_sys->p_data;

    int i_x, i_y;
    int i_len, i_color;

    /* RGB-specific */
    int i_xscale, i_yscale, i_width, i_height, i_ytmp, i_yreal, i_ynext;

    /* Crop-specific */
    int i_x_start, i_y_start, i_x_end, i_y_end;

    /* XXX: this is a COMPLETE HACK, memcpy is unable to do u16s anyway */
    /* FIXME: get this from the DVD */
    for( i_color = 0; i_color < 4; i_color++ )
    {
        p_clut16[i_color] = 0x1111
                             * ( (u16)p_spu->p_sys->pi_yuv[i_color][0] >> 4 );
    }

    i_xscale = ( p_vout->output.i_width << 6 ) / p_vout->render.i_width;
    i_yscale = ( p_vout->output.i_height << 6 ) / p_vout->render.i_height;

    i_width  = p_spu->i_width  * i_xscale;
    i_height = p_spu->i_height * i_yscale;

    p_dest = p_pic->p->p_pixels + ( i_width >> 6 ) * 2
              /* Add the picture coordinates and the SPU coordinates */
              + ( (p_spu->i_x * i_xscale) >> 6 ) * 2
              + ( (p_spu->i_y * i_yscale) >> 6 ) * p_pic->p->i_pitch;

    i_x_start = i_width - i_xscale * p_spu->p_sys->i_x_end;
    i_y_start = i_yscale * p_spu->p_sys->i_y_start;
    i_x_end = i_width - i_xscale * p_spu->p_sys->i_x_start;
    i_y_end = i_yscale * p_spu->p_sys->i_y_end;

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0 ; i_y < i_height ; )
    {
        i_ytmp = i_y >> 6;
        i_y += i_yscale;

        /* Check whether we need to draw one line or more than one */
        if( i_ytmp + 1 >= ( i_y >> 6 ) )
        {
            /* Just one line : we precalculate i_y >> 6 */
            i_yreal = p_pic->p->i_pitch * i_ytmp;

            /* Draw until we reach the end of the line */
            for( i_x = i_width ; i_x ; i_x -= i_len )
            {
                /* Get the RLE part, then draw the line */
                i_color = *p_source & 0x3;
                i_len = i_xscale * ( *p_source++ >> 2 );

                if( b_crop
                     && ( i_x < i_x_start || i_x > i_x_end
                           || i_y < i_y_start || i_y > i_y_end ) )
                {
                    continue;
                }

                switch( p_spu->p_sys->pi_alpha[ i_color ] )
                {
                case 0x00:
                    break;

                case 0x0f:
                    memset( p_dest - 2 * ( i_x >> 6 ) + i_yreal,
                            p_clut16[ i_color ],
                            2 * ( ( i_len >> 6 ) + 1 ) );
                    break;

                default:
                    /* FIXME: we should do transparency */
                    memset( p_dest - 2 * ( i_x >> 6 ) + i_yreal,
                            p_clut16[ i_color ],
                            2 * ( ( i_len >> 6 ) + 1 ) );
                    break;
                }
            }
        }
        else
        {
            i_yreal = p_pic->p->i_pitch * i_ytmp;
            i_ynext = p_pic->p->i_pitch * i_y >> 6;

            /* Draw until we reach the end of the line */
            for( i_x = i_width ; i_x ; i_x -= i_len )
            {
                /* Get the RLE part, then draw as many lines as needed */
                i_color = *p_source & 0x3;
                i_len = i_xscale * ( *p_source++ >> 2 );

                if( b_crop
                     && ( i_x < i_x_start || i_x > i_x_end
                           || i_y < i_y_start || i_y > i_y_end ) )
                {
                    continue;
                }

                switch( p_spu->p_sys->pi_alpha[ i_color ] )
                {
                case 0x00:
                    break;

                case 0x0f:
                    for( i_ytmp = i_yreal ; i_ytmp < i_ynext ;
                         i_ytmp += p_pic->p->i_pitch )
                    {
                        memset( p_dest - 2 * ( i_x >> 6 ) + i_ytmp,
                                p_clut16[ i_color ],
                                2 * ( ( i_len >> 6 ) + 1 ) );
                    }
                    break;

                default:
                    /* FIXME: we should do transparency */
                    for( i_ytmp = i_yreal ; i_ytmp < i_ynext ;
                         i_ytmp += p_pic->p->i_pitch )
                    {
                        memset( p_dest - 2 * ( i_x >> 6 ) + i_ytmp,
                                p_clut16[ i_color ],
                                2 * ( ( i_len >> 6 ) + 1 ) );
                    }
                    break;
                }
            }
        }
    }
}

static void RenderRV32( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu, vlc_bool_t b_crop )
{
    /* Common variables */
    u32  p_clut32[4];
    u8  *p_dest;
    u16 *p_source = (u16 *)p_spu->p_sys->p_data;

    int i_x, i_y;
    int i_len, i_color;

    /* RGB-specific */
    int i_xscale, i_yscale, i_width, i_height, i_ytmp, i_yreal, i_ynext;

    /* Crop-specific */
    int i_x_start, i_y_start, i_x_end, i_y_end;

    /* XXX: this is a COMPLETE HACK, memcpy is unable to do u32s anyway */
    /* FIXME: get this from the DVD */
    for( i_color = 0; i_color < 4; i_color++ )
    {
        p_clut32[i_color] = 0x11111111
                             * ( (u16)p_spu->p_sys->pi_yuv[i_color][0] >> 4 );
    }

    i_xscale = ( p_vout->output.i_width << 6 ) / p_vout->render.i_width;
    i_yscale = ( p_vout->output.i_height << 6 ) / p_vout->render.i_height;

    i_width  = p_spu->i_width  * i_xscale;
    i_height = p_spu->i_height * i_yscale;

    i_x_start = i_width - i_xscale * p_spu->p_sys->i_x_end;
    i_y_start = i_yscale * p_spu->p_sys->i_y_start;
    i_x_end = i_width - i_xscale * p_spu->p_sys->i_x_start;
    i_y_end = i_yscale * p_spu->p_sys->i_y_end;

    p_dest = p_pic->p->p_pixels + ( i_width >> 6 ) * 4
              /* Add the picture coordinates and the SPU coordinates */
              + ( (p_spu->i_x * i_xscale) >> 6 ) * 4
              + ( (p_spu->i_y * i_yscale) >> 6 ) * p_pic->p->i_pitch;

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = 0 ; i_y < i_height ; )
    {
        i_ytmp = i_y >> 6;
        i_y += i_yscale;

        /* Check whether we need to draw one line or more than one */
        if( i_ytmp + 1 >= ( i_y >> 6 ) )
        {
            /* Just one line : we precalculate i_y >> 6 */
            i_yreal = p_pic->p->i_pitch * i_ytmp;

            /* Draw until we reach the end of the line */
            for( i_x = i_width ; i_x ; i_x -= i_len )
            {
                /* Get the RLE part, then draw the line */
                i_color = *p_source & 0x3;
                i_len = i_xscale * ( *p_source++ >> 2 );

                if( b_crop
                     && ( i_x < i_x_start || i_x > i_x_end
                           || i_y < i_y_start || i_y > i_y_end ) )
                {
                    continue;
                }

                switch( p_spu->p_sys->pi_alpha[ i_color ] )
                {
                case 0x00:
                    break;

                case 0x0f:
                    memset( p_dest - 4 * ( i_x >> 6 ) + i_yreal,
                            p_clut32[ i_color ], 4 * ( ( i_len >> 6 ) + 1 ) );
                    break;

                default:
                    /* FIXME: we should do transparency */
                    memset( p_dest - 4 * ( i_x >> 6 ) + i_yreal,
                            p_clut32[ i_color ], 4 * ( ( i_len >> 6 ) + 1 ) );
                    break;
                }
            }
        }
        else
        {
            i_yreal = p_pic->p->i_pitch * i_ytmp;
            i_ynext = p_pic->p->i_pitch * i_y >> 6;

            /* Draw until we reach the end of the line */
            for( i_x = i_width ; i_x ; i_x -= i_len )
            {
                /* Get the RLE part, then draw as many lines as needed */
                i_color = *p_source & 0x3;
                i_len = i_xscale * ( *p_source++ >> 2 );

                if( b_crop
                     && ( i_x < i_x_start || i_x > i_x_end
                           || i_y < i_y_start || i_y > i_y_end ) )
                {
                    continue;
                }

                switch( p_spu->p_sys->pi_alpha[ i_color ] )
                {
                case 0x00:
                    break;

                case 0x0f:
                    for( i_ytmp = i_yreal ; i_ytmp < i_ynext ;
                         i_ytmp += p_pic->p->i_pitch )
                    {
                        memset( p_dest - 4 * ( i_x >> 6 ) + i_ytmp,
                                p_clut32[ i_color ],
                                4 * ( ( i_len >> 6 ) + 1 ) );
                    }
                    break;

                default:
                    /* FIXME: we should do transparency */
                    for( i_ytmp = i_yreal ; i_ytmp < i_ynext ;
                         i_ytmp += p_pic->p->i_pitch )
                    {
                        memset( p_dest - 4 * ( i_x >> 6 ) + i_ytmp,
                                p_clut32[ i_color ],
                                4 * ( ( i_len >> 6 ) + 1 ) );
                    }
                    break;
                }
            }
        }
    }
}

static void RenderYUY2( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_spu, vlc_bool_t b_crop )
{
    /* Common variables */
    u8  *p_dest;
    u16 *p_source = (u16 *)p_spu->p_sys->p_data;

    int i_x, i_y;
    int i_len, i_color;
    u8  i_cnt;

    /* Crop-specific */
    int i_x_start, i_y_start, i_x_end, i_y_end;

    p_dest = p_pic->p->p_pixels +
              + ( p_spu->i_y + p_spu->i_height ) * p_pic->p->i_pitch  // * bytes per line
              + ( p_spu->i_x + p_spu->i_width ) * 2;  // * bytes per pixel

    i_x_start = p_spu->i_width - p_spu->p_sys->i_x_end;
    i_y_start = (p_spu->i_height - p_spu->p_sys->i_y_end)
                  * p_pic->p->i_pitch / 2;
    i_x_end = p_spu->i_width - p_spu->p_sys->i_x_start;
    i_y_end = (p_spu->i_height - p_spu->p_sys->i_y_start)
                * p_pic->p->i_pitch / 2;

    /* Draw until we reach the bottom of the subtitle */
    for( i_y = p_spu->i_height * p_pic->p->i_pitch / 2;
         i_y ;
         i_y -= p_pic->p->i_pitch / 2 )
    {
        /* Draw until we reach the end of the line */
        for( i_x = p_spu->i_width ; i_x ; i_x -= i_len )
        {
            /* Get the RLE part, then draw the line */
            i_color = *p_source & 0x3;
            i_len = *p_source++ >> 2;

            if( b_crop
                 && ( i_x < i_x_start || i_x > i_x_end
                       || i_y < i_y_start || i_y > i_y_end ) )
            {
                continue;
            }

            switch( p_spu->p_sys->pi_alpha[ i_color ] )
            {
            case 0x00:
                break;

            case 0x0f:
                for( i_cnt = 0; i_cnt < i_len; i_cnt++ )
                {
                    /* draw a pixel */
                    /* Y */
                    memset( p_dest - i_x * 2 - i_y * 2 + i_cnt * 2,
                            p_spu->p_sys->pi_yuv[i_color][0], 1);

                    if (!(i_cnt & 0x01))
                    {
                        /* U and V */
                        memset( p_dest - i_x * 2 - i_y * 2 + i_cnt * 2 + 1,
                                0x80, 1);
                        memset( p_dest - i_x * 2 - i_y * 2 + i_cnt * 2 + 3,
                                0x80, 1);
                    }
                }
                break;

            default:
                /* FIXME: we should do transparency */
                for( i_cnt = 0; i_cnt < i_len; i_cnt++ )
                {
                    /* draw a pixel */
                    /* Y */
                    memset( p_dest - i_x * 2 - i_y * 2 + i_cnt * 2,
                            p_spu->p_sys->pi_yuv[i_color][0], 1);

                    if (!(i_cnt & 0x01))
                    {
                        /* U and V */
                        memset( p_dest - i_x * 2 - i_y * 2 + i_cnt * 2 + 1,
                                0x80, 1);
                        memset( p_dest - i_x * 2 - i_y * 2 + i_cnt * 2 + 3,
                                0x80, 1);
                    }
                }
                break;
            }
        }
    }
}

