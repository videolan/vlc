/*****************************************************************************
 * video_widgets.c : OSD widgets manipulation functions
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id$
 *
 * Author: Yoann Peronneau <yoann@videolan.org>
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
#include <stdlib.h>                                                /* free() */
#include <vlc/vout.h>
#include <osd.h>

#define STYLE_EMPTY 0
#define STYLE_FILLED 1

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void DrawRect( vout_thread_t *, subpicture_t *, int, int, int, int,
                      short );
static void DrawTriangle( vout_thread_t *, subpicture_t *, int, int, int, int,
                          short );
static void Render    ( vout_thread_t *, picture_t *, const subpicture_t * );
static void RenderI420( vout_thread_t *, picture_t *, const subpicture_t *,
                        int );
static void RenderYUY2( vout_thread_t *, picture_t *, const subpicture_t *,
                        int );
static void RenderRV32( vout_thread_t *, picture_t *, const subpicture_t *,
                        int );
static subpicture_t *vout_CreateWidget( vout_thread_t *, int );
static void FreeWidget( subpicture_t * );

/**
 * Private data in a subpicture.
 */
struct subpicture_sys_t
{
    int     i_x;
    int     i_y;
    int     i_width;
    int     i_height;
    uint8_t *p_pic;
};

/*****************************************************************************
 * Draws a rectangle at the given position in the subpic.
 * It may be filled (fill == STYLE_FILLED) or empty (fill == STYLE_EMPTY).
 *****************************************************************************/
static void DrawRect( vout_thread_t *p_vout, subpicture_t *p_subpic,
                      int i_x1, int i_y1, int i_x2, int i_y2, short fill )
{
    int x, y;
    subpicture_sys_t *p_widget = p_subpic->p_sys;

    if( fill == STYLE_FILLED )
    {
        for( y = i_y1; y <= i_y2; y++ )
        {
            for( x = i_x1; x <= i_x2; x++ )
            {
                p_widget->p_pic[ x + p_widget->i_width * y ] = 1;
            }
        }
    }
    else
    {
        for( y = i_y1; y <= i_y2; y++ )
        {
            p_widget->p_pic[ i_x1 + p_widget->i_width * y ] = 1;
            p_widget->p_pic[ i_x2 + p_widget->i_width * y ] = 1;
        }
        for( x = i_x1; x <= i_x2; x++ )
        {
            p_widget->p_pic[ x + p_widget->i_width * i_y1 ] = 1;
            p_widget->p_pic[ x + p_widget->i_width * i_y2 ] = 1;
        }
    }
}

/*****************************************************************************
 * Draws a triangle at the given position in the subpic.
 * It may be filled (fill == STYLE_FILLED) or empty (fill == STYLE_EMPTY).
 *****************************************************************************/
static void DrawTriangle( vout_thread_t *p_vout, subpicture_t *p_subpic,
                          int i_x1, int i_y1, int i_x2, int i_y2, short fill )
{
    int x, y, i_mid, h;
    subpicture_sys_t *p_widget = p_subpic->p_sys;

    i_mid = i_y1 + ( ( i_y2 - i_y1 ) >> 1 );

    if( i_x2 >= i_x1 )
    {
        if( fill == STYLE_FILLED )
        {
            for( y = i_y1; y <= i_mid; y++ )
            {
                h = y - i_y1;
                for( x = i_x1; x <= i_x1 + h && x <= i_x2; x++ )
                {
                    p_widget->p_pic[ x + p_widget->i_width * y ] = 1;
                    p_widget->p_pic[ x + p_widget->i_width * ( i_y2 - h ) ] = 1;
                }
            }
        }
        else
        {
            for( y = i_y1; y <= i_mid; y++ )
            {
                h = y - i_y1;
                p_widget->p_pic[ i_x1 + p_widget->i_width * y ] = 1;
                p_widget->p_pic[ i_x1 + h + p_widget->i_width * y ] = 1;
                p_widget->p_pic[ i_x1 + p_widget->i_width * ( i_y2 - h ) ] = 1;
                p_widget->p_pic[ i_x1 + h + p_widget->i_width * ( i_y2 - h ) ] = 1;
            }
        }
    }
    else
    {
        if( fill == STYLE_FILLED )
        {
            for( y = i_y1; y <= i_mid; y++ )
            {
                h = y - i_y1;
                for( x = i_x1; x >= i_x1 - h && x >= i_x2; x-- )
                {
                    p_widget->p_pic[ x + p_widget->i_width * y ] = 1;
                    p_widget->p_pic[ x + p_widget->i_width * ( i_y2 - h ) ] = 1;
                }
            }
        }
        else
        {
            for( y = i_y1; y <= i_mid; y++ )
            {
                h = y - i_y1;
                p_widget->p_pic[ i_x1 + p_widget->i_width * y ] = 1;
                p_widget->p_pic[ i_x1 - h + p_widget->i_width * y ] = 1;
                p_widget->p_pic[ i_x1 + p_widget->i_width * ( i_y2 - h ) ] = 1;
                p_widget->p_pic[ i_x1 - h + p_widget->i_width * ( i_y2 - h ) ] = 1;
            }
        }
    }
}

/*****************************************************************************
 * Render: place widget in picture
 *****************************************************************************
 * This function merges the previously drawn widget into a picture
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic,
                    const subpicture_t *p_subpic )
{
    int i_fade_alpha = 255;
    mtime_t i_fade_start = ( p_subpic->i_stop + p_subpic->i_start ) / 2;
    mtime_t i_now = mdate();

    if( i_now >= i_fade_start )
    {
        i_fade_alpha = 255 * ( p_subpic->i_stop - i_now ) /
                       ( p_subpic->i_stop - i_fade_start );
    }

    switch( p_vout->output.i_chroma )
    {
        /* I420 target, no scaling */
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('I','Y','U','V'):
        case VLC_FOURCC('Y','V','1','2'):
            RenderI420( p_vout, p_pic, p_subpic, i_fade_alpha );
            break;
        /* RV32 target, scaling */
        case VLC_FOURCC('R','V','2','4'):
        case VLC_FOURCC('R','V','3','2'):
            RenderRV32( p_vout, p_pic, p_subpic, i_fade_alpha );
            break;
        /* NVidia or BeOS overlay, no scaling */
        case VLC_FOURCC('Y','U','Y','2'):
            RenderYUY2( p_vout, p_pic, p_subpic, i_fade_alpha );
            break;

        default:
            msg_Err( p_vout, "unknown chroma, can't render SPU" );
            break;
    }
}

/**
 * Draw a widget on a I420 (or similar) picture
 */
static void RenderI420( vout_thread_t *p_vout, picture_t *p_pic,
                    const subpicture_t *p_subpic, int i_fade_alpha )
{
    subpicture_sys_t *p_widget = p_subpic->p_sys;
    int i_plane, x, y, pen_x, pen_y;

    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        uint8_t *p_in = p_pic->p[ i_plane ].p_pixels;
        int i_pic_pitch = p_pic->p[ i_plane ].i_pitch;

        if ( i_plane == 0 )
        {
            pen_x = p_widget->i_x;
            pen_y = p_widget->i_y;
#define alpha p_widget->p_pic[ x + y * p_widget->i_width ] * i_fade_alpha
#define pixel p_in[ ( pen_y + y ) * i_pic_pitch + pen_x + x ]
            for( y = 0; y < p_widget->i_height; y++ )
            {
                for( x = 0; x < p_widget->i_width; x++ )
                {
                    if( alpha == 0 ) continue;
                    pen_y--;
                    pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 );
                    pen_y++; pen_x--;
                    pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 );
                    pen_x += 2;
                    pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 );
                    pen_y++; pen_x--;
                    pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 );
                    pen_y--;
                }
            }
            for( y = 0; y < p_widget->i_height; y++ )
            {
                for( x = 0; x < p_widget->i_width; x++ )
                {
                    pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 ) +
                            ( 255 * alpha >> 8 );
                 }
             }
#undef alpha
#undef pixel
        }
        else
        {
            pen_x = p_widget->i_x >> 1;
            pen_y = p_widget->i_y >> 1;
#define alpha p_widget->p_pic[ x + y * p_widget->i_width ] * i_fade_alpha
#define pixel p_in[ ( pen_y + (y >> 1) ) * i_pic_pitch + pen_x + (x >> 1) ]
            for( y = 0; y < p_widget->i_height; y+=2 )
            {
                for( x = 0; x < p_widget->i_width; x+=2 )
                {
                    if( alpha == 0 ) continue;
                    pixel = ( ( pixel * ( 0xFF - alpha ) ) >> 8 ) +
                        ( 0x80 * alpha >> 8 );
                }
            }
#undef alpha
#undef pixel
        }
    }

}

/**
 * Draw a widget on a YUY2 picture
 */
static void RenderYUY2( vout_thread_t *p_vout, picture_t *p_pic,
                        const subpicture_t *p_subpic, int i_fade_alpha )
{
    subpicture_sys_t *p_widget = p_subpic->p_sys;
    int x, y, pen_x, pen_y;
    uint8_t *p_in = p_pic->p[0].p_pixels;
    int i_pic_pitch = p_pic->p[0].i_pitch;

    pen_x = p_widget->i_x;
    pen_y = p_widget->i_y;
#define alpha p_widget->p_pic[ x + y * p_widget->i_width ] * i_fade_alpha
#define pixel p_in[ ( pen_y + y ) * i_pic_pitch + 2 * ( pen_x + x ) ]
    for( y = 0; y < p_widget->i_height; y++ )
    {
        for( x = 0; x < p_widget->i_width; x++ )
        {
            pen_y--;
            pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 );
            pen_y++; pen_x--;
            pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 );
            pen_x += 2;
            pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 );
            pen_y++; pen_x--;
            pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 );
            pen_y--;
        }
    }
    for( y = 0; y < p_widget->i_height; y++ )
    {
        for( x = 0; x < p_widget->i_width; x++ )
        {
            pixel = ( ( pixel * ( 255 - alpha ) ) >> 8 ) +
                    ( 255 * alpha >> 8 );
         }
     }
#undef alpha
#undef pixel
}

/**
 * Draw a widget on a RV32 picture
 */
static void RenderRV32( vout_thread_t *p_vout, picture_t *p_pic,
                    const subpicture_t *p_subpic, int i_fade_alpha )
{
    subpicture_sys_t *p_widget = p_subpic->p_sys;
    int x, y, pen_x, pen_y;
    uint8_t *p_in = p_pic->p[0].p_pixels;
    int i_pic_pitch = p_pic->p[0].i_pitch;

    pen_x = p_widget->i_x;
    pen_y = p_widget->i_y;

#define alpha p_widget->p_pic[ x + y * p_widget->i_width ] * i_fade_alpha
#define pixel( c ) p_in[ ( pen_y + y ) * i_pic_pitch + 4 * ( pen_x + x ) + c ]
    for(y = 0; y < p_widget->i_height; y++ )
    {
        for( x = 0; x < p_widget->i_width; x++ )
        {
            pen_y--;
            pixel( 0 ) = ( ( pixel( 0 ) * ( 255 - alpha ) ) >> 8 );
            pixel( 1 ) = ( ( pixel( 1 ) * ( 255 - alpha ) ) >> 8 );
            pixel( 2 ) = ( ( pixel( 2 ) * ( 255 - alpha ) ) >> 8 );
            pen_y++; pen_x--;
            pixel( 0 ) = ( ( pixel( 0 ) * ( 255 - alpha ) ) >> 8 );
            pixel( 1 ) = ( ( pixel( 1 ) * ( 255 - alpha ) ) >> 8 );
            pixel( 2 ) = ( ( pixel( 2 ) * ( 255 - alpha ) ) >> 8 );
            pen_x += 2;
            pixel( 0 ) = ( ( pixel( 0 ) * ( 255 - alpha ) ) >> 8 );
            pixel( 1 ) = ( ( pixel( 1 ) * ( 255 - alpha ) ) >> 8 );
            pixel( 2 ) = ( ( pixel( 2 ) * ( 255 - alpha ) ) >> 8 );
            pen_y++; pen_x--;
            pixel( 0 ) = ( ( pixel( 0 ) * ( 255 - alpha ) ) >> 8 );
            pixel( 1 ) = ( ( pixel( 1 ) * ( 255 - alpha ) ) >> 8 );
            pixel( 2 ) = ( ( pixel( 2 ) * ( 255 - alpha ) ) >> 8 );
            pen_y--;
        }
    }
    for(y = 0; y < p_widget->i_height; y++ )
    {
        for( x = 0; x < p_widget->i_width; x++ )
        {
            pixel( 0 ) = ( ( pixel( 0 ) * ( 255 - alpha ) ) >> 8 ) +
                ( 255 * alpha >> 8 );
            pixel( 1 ) = ( ( pixel( 1 ) * ( 255 - alpha ) ) >> 8 ) +
                ( 255 * alpha >> 8 );
            pixel( 2 ) = ( ( pixel( 2 ) * ( 255 - alpha ) ) >> 8 ) +
                ( 255 * alpha >> 8 );
        }
    }
#undef alpha
#undef pixel
}

/*****************************************************************************
 * Creates and initializes an OSD widget.
 *****************************************************************************/
subpicture_t *vout_CreateWidget( vout_thread_t *p_vout, int i_channel )
{
    subpicture_t *p_subpic;
    subpicture_sys_t *p_widget;
    mtime_t i_now = mdate();

    p_subpic = 0;
    p_widget = 0;

    /* Create and initialize a subpicture */
    p_subpic = vout_CreateSubPicture( p_vout, i_channel, GRAPH_CONTENT,
                                      MEMORY_SUBPICTURE );
    if( p_subpic == NULL )
    {
        return NULL;
    }
    p_subpic->pf_render = Render;
    p_subpic->pf_destroy = FreeWidget;
    p_subpic->i_start = i_now;
    p_subpic->i_stop = i_now + 1200000;
    p_subpic->b_ephemer = VLC_FALSE;

    p_widget = malloc( sizeof(subpicture_sys_t) );
    if( p_widget == NULL )
    {
        FreeWidget( p_subpic );
        vout_DestroySubPicture( p_vout, p_subpic );
        return NULL;
    }
    p_subpic->p_sys = p_widget;

    return p_subpic;
}

/*****************************************************************************
 * Displays an OSD slider.
 * Types are: OSD_HOR_SLIDER and OSD_VERT_SLIDER.
 *****************************************************************************/
void vout_OSDSlider( vlc_object_t *p_caller, int i_channel, int i_position,
                     short i_type )
{
    vout_thread_t *p_vout = vlc_object_find( p_caller, VLC_OBJECT_VOUT,
                                             FIND_ANYWHERE );
    subpicture_t *p_subpic;
    subpicture_sys_t *p_widget;
    int i_x_margin, i_y_margin;

    if( p_vout == NULL || !config_GetInt( p_caller, "osd" ) || i_position < 0 )
    {
        return;
    }

    p_subpic = vout_CreateWidget( p_vout, i_channel );
    if( p_subpic == NULL )
    {
        return;
    }
    p_widget = p_subpic->p_sys;

    i_y_margin = p_vout->render.i_height / 10;
    i_x_margin = i_y_margin;
    if( i_type == OSD_HOR_SLIDER )
    {
        p_widget->i_width = p_vout->render.i_width - 2 * i_x_margin;
        p_widget->i_height = p_vout->render.i_height / 20;
        p_widget->i_x = i_x_margin;
        p_widget->i_y = p_vout->render.i_height - i_y_margin -
                        p_widget->i_height;
    }
    else
    {
        p_widget->i_width = p_vout->render.i_width / 40;
        p_widget->i_height = p_vout->render.i_height - 2 * i_y_margin;
        p_widget->i_x = p_vout->render.i_width - i_x_margin -
                        p_widget->i_width;
        p_widget->i_y = i_y_margin;
    }

    p_widget->p_pic = (uint8_t *)malloc( p_widget->i_width *
                                         p_widget->i_height );
    if( p_widget->p_pic == NULL )
    {
        FreeWidget( p_subpic );
        vout_DestroySubPicture( p_vout, p_subpic );
        return;
    }
    memset( p_widget->p_pic, 0, p_widget->i_width * p_widget->i_height );

    if( i_type == OSD_HOR_SLIDER )
    {
        int i_x_pos = ( p_widget->i_width - 2 ) * i_position / 100;
        int i_y_pos = p_widget->i_height / 2;
        DrawRect( p_vout, p_subpic, i_x_pos - 1, 2, i_x_pos + 1,
                  p_widget->i_height - 3, STYLE_FILLED );
        DrawRect( p_vout, p_subpic, 0, 0, p_widget->i_width - 1,
                  p_widget->i_height - 1, STYLE_EMPTY );
    }
    else if( i_type == OSD_VERT_SLIDER )
    {
        int i_y_pos = p_widget->i_height / 2;
        DrawRect( p_vout, p_subpic, 2, p_widget->i_height -
                  ( p_widget->i_height - 2 ) * i_position / 100,
                  p_widget->i_width - 3, p_widget->i_height - 3,
                  STYLE_FILLED );
        DrawRect( p_vout, p_subpic, 1, i_y_pos, 1, i_y_pos, STYLE_FILLED );
        DrawRect( p_vout, p_subpic, p_widget->i_width - 2, i_y_pos,
                  p_widget->i_width - 2, i_y_pos, STYLE_FILLED );
        DrawRect( p_vout, p_subpic, 0, 0, p_widget->i_width - 1,
                  p_widget->i_height - 1, STYLE_EMPTY );
    }

    vout_DisplaySubPicture( p_vout, p_subpic );

    vlc_object_release( p_vout );
    return;
}

/*****************************************************************************
 * Displays an OSD icon.
 * Types are: OSD_PLAY_ICON, OSD_PAUSE_ICON, OSD_SPEAKER_ICON, OSD_MUTE_ICON
 *****************************************************************************/
void vout_OSDIcon( vlc_object_t *p_caller, short i_type )
{
    vout_thread_t *p_vout = vlc_object_find( p_caller, VLC_OBJECT_VOUT,
                                             FIND_ANYWHERE );
    subpicture_t *p_subpic;
    subpicture_sys_t *p_widget;
    int i_x_margin, i_y_margin, i_channel;

    if( p_vout == NULL || !config_GetInt( p_caller, "osd" ) )
    {
        return;
    }

    switch( i_type )
    {
        case OSD_SPEAKER_ICON:
            i_channel = VOLUME_CHAN;
            break;
        default:
            i_channel = SOLO_CHAN;
            break;
    }

    p_subpic = vout_CreateWidget( p_vout, i_channel );
    if( p_subpic == NULL )
    {
        return;
    }
    p_widget = p_subpic->p_sys;

    i_y_margin = p_vout->render.i_height / 15;
    i_x_margin = i_y_margin;
    p_widget->i_width = p_vout->render.i_width / 20;
    p_widget->i_height = p_widget->i_width;
    p_widget->i_x = p_vout->render.i_width - i_x_margin -
                    p_widget->i_width;
    p_widget->i_y = i_y_margin;

    p_widget->p_pic = (uint8_t *)malloc( p_widget->i_width *
                                         p_widget->i_height );
    if( p_widget->p_pic == NULL )
    {
        FreeWidget( p_subpic );
        vout_DestroySubPicture( p_vout, p_subpic );
        return;
    }
    memset( p_widget->p_pic, 0, p_widget->i_width * p_widget->i_height );

    if( i_type == OSD_PAUSE_ICON )
    {
        int i_bar_width = p_widget->i_width / 3;
        DrawRect( p_vout, p_subpic, 0, 0, i_bar_width - 1,
                  p_widget->i_height - 1, STYLE_FILLED );
        DrawRect( p_vout, p_subpic, p_widget->i_width - i_bar_width, 0,
                  p_widget->i_width - 1, p_widget->i_height - 1, STYLE_FILLED );
    }
    else if( i_type == OSD_PLAY_ICON )
    {
        int i_mid = p_widget->i_height >> 1;
        int i_delta = ( p_widget->i_width - i_mid ) >> 1;
        int i_y2 = ( ( p_widget->i_height - 1 ) >> 1 ) * 2;
        DrawTriangle( p_vout, p_subpic, i_delta, 0,
                      p_widget->i_width - i_delta, i_y2, STYLE_FILLED );
    }
    else if( i_type == OSD_SPEAKER_ICON || i_type == OSD_MUTE_ICON )
    {
        int i_mid = p_widget->i_height >> 1;
        int i_delta = ( p_widget->i_width - i_mid ) >> 1;
        int i_y2 = ( ( p_widget->i_height - 1 ) >> 1 ) * 2;
        DrawRect( p_vout, p_subpic, i_delta, i_mid / 2,
                  p_widget->i_width - i_delta,
                  p_widget->i_height - 1 - i_mid / 2, STYLE_FILLED );
        DrawTriangle( p_vout, p_subpic, p_widget->i_width - i_delta, 0,
                      i_delta, i_y2, STYLE_FILLED );
        if( i_type == OSD_MUTE_ICON )
        {
            int i;
            for( i = 1; i < p_widget->i_width; i++ )
            {
                int k = i + ( p_widget->i_height - i - 1 ) * p_widget->i_width;
                p_widget->p_pic[ k ] = 1 - p_widget->p_pic[ k ];
            }
        }
    }

    vout_DisplaySubPicture( p_vout, p_subpic );

    vlc_object_release( p_vout );
    return;
}

/**
 * Frees the widget.
 */
static void FreeWidget( subpicture_t *p_subpic )
{
    subpicture_sys_t *p_widget = p_subpic->p_sys;

    if( p_subpic->p_sys == NULL ) return;

    if( p_widget->p_pic != NULL )
    {
        free( p_widget->p_pic );
    }
    free( p_widget );
}
