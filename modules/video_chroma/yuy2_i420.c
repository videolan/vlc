/*****************************************************************************
 * yuy2_i420.c : Packed YUV 4:2:2 to Planar YUV conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea at videolan dot org>
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

#include <vlc/vlc.h>
#include <vlc_vout.h>

#define SRC_FOURCC "YUY2,YUNV,YVYU,UYVY,UYNV,Y422,cyuv"
#define DEST_FOURCC  "I420"

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static int  Activate ( vlc_object_t * );

static void YUY2_I420           ( vout_thread_t *, picture_t *, picture_t * );
static void YVYU_I420           ( vout_thread_t *, picture_t *, picture_t * );
static void UYVY_I420           ( vout_thread_t *, picture_t *, picture_t * );
static void cyuv_I420           ( vout_thread_t *, picture_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Conversions from " SRC_FOURCC " to " DEST_FOURCC) );
    set_capability( "chroma", 80 );
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

    switch( p_vout->output.i_chroma )
    {
        case VLC_FOURCC('I','4','2','0'):
            switch( p_vout->render.i_chroma )
            {
                case VLC_FOURCC('Y','U','Y','2'):
                case VLC_FOURCC('Y','U','N','V'):
                    p_vout->chroma.pf_convert = YUY2_I420;
                    break;

                case VLC_FOURCC('Y','V','Y','U'):
                    p_vout->chroma.pf_convert = YVYU_I420;
                    break;

                case VLC_FOURCC('U','Y','V','Y'):
                case VLC_FOURCC('U','Y','N','V'):
                case VLC_FOURCC('Y','4','2','2'):
                    p_vout->chroma.pf_convert = UYVY_I420;
                    break;

                case VLC_FOURCC('c','y','u','v'):
                    p_vout->chroma.pf_convert = cyuv_I420;
                    break;

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
 * YUY2_I420: packed YUY2 4:2:2 to planar YUV 4:2:0
 *****************************************************************************/
static void YUY2_I420( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    uint8_t *p_line = p_source->p->p_pixels;

    uint8_t *p_y = p_dest->Y_PIXELS;
    uint8_t *p_u = p_dest->U_PIXELS;
    uint8_t *p_v = p_dest->V_PIXELS;

    int i_x, i_y;

    const int i_dest_margin = p_dest->p[0].i_pitch
                                 - p_dest->p[0].i_visible_pitch;
    const int i_dest_margin_c = p_dest->p[1].i_pitch
                                 - p_dest->p[1].i_visible_pitch;
    const int i_source_margin = p_source->p->i_pitch
                               - p_source->p->i_visible_pitch;

    vlc_bool_t b_skip = VLC_FALSE;

    for( i_y = p_vout->output.i_height ; i_y-- ; )
    {
        if( b_skip )
        {
            for( i_x = p_vout->output.i_width / 8 ; i_x-- ; )
            {
    #define C_YUYV_YUV422_skip( p_line, p_y, p_u, p_v )      \
                *p_y++ = *p_line++; p_line++; \
                *p_y++ = *p_line++; p_line++
                C_YUYV_YUV422_skip( p_line, p_y, p_u, p_v );
                C_YUYV_YUV422_skip( p_line, p_y, p_u, p_v );
                C_YUYV_YUV422_skip( p_line, p_y, p_u, p_v );
                C_YUYV_YUV422_skip( p_line, p_y, p_u, p_v );
            }
            for( i_x = ( p_vout->output.i_width % 8 ) / 2; i_x-- ; )
            {
                C_YUYV_YUV422_skip( p_line, p_y, p_u, p_v );
            }
        }
        else
        {
            for( i_x = p_vout->output.i_width / 8 ; i_x-- ; )
            {
    #define C_YUYV_YUV422( p_line, p_y, p_u, p_v )      \
                *p_y++ = *p_line++; *p_u++ = *p_line++; \
                *p_y++ = *p_line++; *p_v++ = *p_line++
                C_YUYV_YUV422( p_line, p_y, p_u, p_v );
                C_YUYV_YUV422( p_line, p_y, p_u, p_v );
                C_YUYV_YUV422( p_line, p_y, p_u, p_v );
                C_YUYV_YUV422( p_line, p_y, p_u, p_v );
            }
            for( i_x = ( p_vout->output.i_width % 8 ) / 2; i_x-- ; )
            {
                C_YUYV_YUV422( p_line, p_y, p_u, p_v );
            }
        }
        p_line += i_source_margin;
        p_y += i_dest_margin;
        p_u += i_dest_margin_c;
        p_v += i_dest_margin_c;

        b_skip = !b_skip;
    }
}

/*****************************************************************************
 * YVYU_I420: packed YVYU 4:2:2 to planar YUV 4:2:0
 *****************************************************************************/
static void YVYU_I420( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    uint8_t *p_line = p_source->p->p_pixels;

    uint8_t *p_y = p_dest->Y_PIXELS;
    uint8_t *p_u = p_dest->U_PIXELS;
    uint8_t *p_v = p_dest->V_PIXELS;

    int i_x, i_y;

    const int i_dest_margin = p_dest->p[0].i_pitch
                                 - p_dest->p[0].i_visible_pitch;
    const int i_dest_margin_c = p_dest->p[1].i_pitch
                                 - p_dest->p[1].i_visible_pitch;
    const int i_source_margin = p_source->p->i_pitch
                               - p_source->p->i_visible_pitch;

    vlc_bool_t b_skip = VLC_FALSE;

    for( i_y = p_vout->output.i_height ; i_y-- ; )
    {
        if( b_skip )
        {
            for( i_x = p_vout->output.i_width / 8 ; i_x-- ; )
            {
    #define C_YVYU_YUV422_skip( p_line, p_y, p_u, p_v )      \
                *p_y++ = *p_line++; p_line++; \
                *p_y++ = *p_line++; p_line++
                C_YVYU_YUV422_skip( p_line, p_y, p_u, p_v );
                C_YVYU_YUV422_skip( p_line, p_y, p_u, p_v );
                C_YVYU_YUV422_skip( p_line, p_y, p_u, p_v );
                C_YVYU_YUV422_skip( p_line, p_y, p_u, p_v );
            }
            for( i_x = ( p_vout->output.i_width % 8 ) / 2; i_x-- ; )
            {
                C_YVYU_YUV422_skip( p_line, p_y, p_u, p_v );
            }
        }
        else
        {
            for( i_x = p_vout->output.i_width / 8 ; i_x-- ; )
            {
    #define C_YVYU_YUV422( p_line, p_y, p_u, p_v )      \
                *p_y++ = *p_line++; *p_v++ = *p_line++; \
                *p_y++ = *p_line++; *p_u++ = *p_line++
                C_YVYU_YUV422( p_line, p_y, p_u, p_v );
                C_YVYU_YUV422( p_line, p_y, p_u, p_v );
                C_YVYU_YUV422( p_line, p_y, p_u, p_v );
                C_YVYU_YUV422( p_line, p_y, p_u, p_v );
            }
            for( i_x = ( p_vout->output.i_width % 8 ) / 2; i_x-- ; )
            {
                C_YVYU_YUV422( p_line, p_y, p_u, p_v );
            }
        }
        p_line += i_source_margin;
        p_y += i_dest_margin;
        p_u += i_dest_margin_c;
        p_v += i_dest_margin_c;

        b_skip = !b_skip;
    }
}

/*****************************************************************************
 * UYVY_I420: packed UYVY 4:2:2 to planar YUV 4:2:0
 *****************************************************************************/
static void UYVY_I420( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    uint8_t *p_line = p_source->p->p_pixels;

    uint8_t *p_y = p_dest->Y_PIXELS;
    uint8_t *p_u = p_dest->U_PIXELS;
    uint8_t *p_v = p_dest->V_PIXELS;

    int i_x, i_y;

    const int i_dest_margin = p_dest->p[0].i_pitch
                                 - p_dest->p[0].i_visible_pitch;
    const int i_dest_margin_c = p_dest->p[1].i_pitch
                                 - p_dest->p[1].i_visible_pitch;
    const int i_source_margin = p_source->p->i_pitch
                               - p_source->p->i_visible_pitch;

    vlc_bool_t b_skip = VLC_FALSE;

    for( i_y = p_vout->output.i_height ; i_y-- ; )
    {
        if( b_skip )
        {
            for( i_x = p_vout->output.i_width / 8 ; i_x-- ; )
            {
    #define C_UYVY_YUV422_skip( p_line, p_y, p_u, p_v )      \
                *p_u++ = *p_line++; p_line++; \
                *p_v++ = *p_line++; p_line++
                C_UYVY_YUV422_skip( p_line, p_y, p_u, p_v );
                C_UYVY_YUV422_skip( p_line, p_y, p_u, p_v );
                C_UYVY_YUV422_skip( p_line, p_y, p_u, p_v );
                C_UYVY_YUV422_skip( p_line, p_y, p_u, p_v );
            }
            for( i_x = ( p_vout->output.i_width % 8 ) / 2; i_x-- ; )
            {
                C_UYVY_YUV422_skip( p_line, p_y, p_u, p_v );
            }
        }
        else
        {
            for( i_x = p_vout->output.i_width / 8 ; i_x-- ; )
            {
    #define C_UYVY_YUV422( p_line, p_y, p_u, p_v )      \
                *p_u++ = *p_line++; *p_y++ = *p_line++; \
                *p_v++ = *p_line++; *p_y++ = *p_line++
                C_UYVY_YUV422( p_line, p_y, p_u, p_v );
                C_UYVY_YUV422( p_line, p_y, p_u, p_v );
                C_UYVY_YUV422( p_line, p_y, p_u, p_v );
                C_UYVY_YUV422( p_line, p_y, p_u, p_v );
            }
            for( i_x = ( p_vout->output.i_width % 8 ) / 2; i_x-- ; )
            {
                C_UYVY_YUV422( p_line, p_y, p_u, p_v );
            }
        }
        p_line += i_source_margin;
        p_y += i_dest_margin;
        p_u += i_dest_margin_c;
        p_v += i_dest_margin_c;

        b_skip = !b_skip;
    }
}

/*****************************************************************************
 * cyuv_I420: upside-down packed UYVY 4:2:2 to planar YUV 4:2:0
 * FIXME
 *****************************************************************************/
static void cyuv_I420( vout_thread_t *p_vout, picture_t *p_source,
                                              picture_t *p_dest )
{
    uint8_t *p_line = p_source->p->p_pixels;

    uint8_t *p_y = p_dest->Y_PIXELS;
    uint8_t *p_u = p_dest->U_PIXELS;
    uint8_t *p_v = p_dest->V_PIXELS;

    int i_x, i_y;

    const int i_dest_margin = p_dest->p[0].i_pitch
                                 - p_dest->p[0].i_visible_pitch;
    const int i_dest_margin_c = p_dest->p[1].i_pitch
                                 - p_dest->p[1].i_visible_pitch;
    const int i_source_margin = p_source->p->i_pitch
                               - p_source->p->i_visible_pitch;

    vlc_bool_t b_skip = VLC_FALSE;

    for( i_y = p_vout->output.i_height ; i_y-- ; )
    {
        if( b_skip )
        {
            for( i_x = p_vout->output.i_width / 8 ; i_x-- ; )
            {
    #define C_cyuv_YUV422_skip( p_line, p_y, p_u, p_v )      \
                *p_y++ = *p_line++; p_line++; \
                *p_y++ = *p_line++; p_line++
                C_cyuv_YUV422_skip( p_line, p_y, p_u, p_v );
                C_cyuv_YUV422_skip( p_line, p_y, p_u, p_v );
                C_cyuv_YUV422_skip( p_line, p_y, p_u, p_v );
                C_cyuv_YUV422_skip( p_line, p_y, p_u, p_v );
            }
            for( i_x = ( p_vout->output.i_width % 8 ) / 2; i_x-- ; )
            {
                C_cyuv_YUV422_skip( p_line, p_y, p_u, p_v );
            }
        }
        else
        {
            for( i_x = p_vout->output.i_width / 8 ; i_x-- ; )
            {
    #define C_cyuv_YUV422( p_line, p_y, p_u, p_v )      \
                *p_y++ = *p_line++; *p_v++ = *p_line++; \
                *p_y++ = *p_line++; *p_u++ = *p_line++
                C_cyuv_YUV422( p_line, p_y, p_u, p_v );
                C_cyuv_YUV422( p_line, p_y, p_u, p_v );
                C_cyuv_YUV422( p_line, p_y, p_u, p_v );
                C_cyuv_YUV422( p_line, p_y, p_u, p_v );
            }
            for( i_x = ( p_vout->output.i_width % 8 ) / 2; i_x-- ; )
            {
                C_cyuv_YUV422( p_line, p_y, p_u, p_v );
            }
        }
        p_line += i_source_margin;
        p_y += i_dest_margin;
        p_u += i_dest_margin_c;
        p_v += i_dest_margin_c;

        b_skip = !b_skip;
    }
}
