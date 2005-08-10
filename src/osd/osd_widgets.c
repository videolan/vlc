/*****************************************************************************
 * osd_widgets.c : OSD widgets manipulation functions
 *****************************************************************************
 * Copyright (C) 2005 M2X
 * Copyright (C) 2004 VideoLAN (Centrale Réseaux) and its contributors
 *
 * $Id: osd_widgets.c 9274 2004-11-10 15:16:51Z gbazin $
 *
 * Author: Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
 *
 * Based on: src/video_output/video_widgets.c
 *     from: Yoann Peronneau <yoann@videolan.org>
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
#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <osd.h>

#define STYLE_EMPTY 0
#define STYLE_FILLED 1

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void DrawRect( picture_t *, int, int, int, int, short );
static void DrawTriangle( picture_t *, int, int, int, int, short );
static picture_t *osd_CreatePicture( int, int );

/*****************************************************************************
 * Draws a rectangle at the given position in the subpic.
 * It may be filled (fill == STYLE_FILLED) or empty (fill == STYLE_EMPTY).
 *****************************************************************************/
static void DrawRect( picture_t *p_picture, int i_x1, int i_y1,
                      int i_x2, int i_y2, short fill )
{
    int x, y;
    uint8_t *p_a = p_picture->A_PIXELS;
    int i_pitch = p_picture->Y_PITCH;

    if( fill == STYLE_FILLED )
    {
        for( y = i_y1; y <= i_y2; y++ )
        {
            for( x = i_x1; x <= i_x2; x++ )
            {
                p_a[ x + i_pitch * y ] = 0xff;
            }
        }
    }
    else
    {
        for( y = i_y1; y <= i_y2; y++ )
        {
            p_a[ i_x1 + i_pitch * y ] = 0xff;
            p_a[ i_x2 + i_pitch * y ] = 0xff;
        }
        for( x = i_x1; x <= i_x2; x++ )
        {
            p_a[ x + i_pitch * i_y1 ] = 0xff;
            p_a[ x + i_pitch * i_y2 ] = 0xff;
        }
    }
}

/*****************************************************************************
 * Draws a triangle at the given position in the subpic.
 * It may be filled (fill == STYLE_FILLED) or empty (fill == STYLE_EMPTY).
 *****************************************************************************/
static void DrawTriangle( picture_t *p_picture, int i_x1, int i_y1,
                          int i_x2, int i_y2, short fill )
{
    int x, y, i_mid, h;
    uint8_t *p_a = p_picture->A_PIXELS;
    int i_pitch = p_picture->Y_PITCH;

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
                    p_a[ x + i_pitch * y ] = 0xff;
                    p_a[ x + i_pitch * ( i_y2 - h ) ] = 0xff;
                }
            }
        }
        else
        {
            for( y = i_y1; y <= i_mid; y++ )
            {
                h = y - i_y1;
                p_a[ i_x1 + i_pitch * y ] = 0xff;
                p_a[ i_x1 + h + i_pitch * y ] = 0xff;
                p_a[ i_x1 + i_pitch * ( i_y2 - h ) ] = 0xff;
                p_a[ i_x1 + h + i_pitch * ( i_y2 - h ) ] = 0xff;
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
                    p_a[ x + i_pitch * y ] = 0xff;
                    p_a[ x + i_pitch * ( i_y2 - h ) ] = 0xff;
                }
            }
        }
        else
        {
            for( y = i_y1; y <= i_mid; y++ )
            {
                h = y - i_y1;
                p_a[ i_x1 + i_pitch * y ] = 0xff;
                p_a[ i_x1 - h + i_pitch * y ] = 0xff;
                p_a[ i_x1 + i_pitch * ( i_y2 - h ) ] = 0xff;
                p_a[ i_x1 - h + i_pitch * ( i_y2 - h ) ] = 0xff;
            }
        }
    }
}

/*****************************************************************************
 * Create Picture: creates picture
 *****************************************************************************/
static picture_t *osd_CreatePicture( int i_width, int i_height )
{
    picture_t *p_picture = NULL;
    uint8_t *p_y, *p_u, *p_v, *p_a;
    int i_pitch;

    p_picture = (picture_t*) malloc( sizeof(picture_t) );
    if( p_picture == NULL )
    {
        return NULL;
    }
    /* Clear the memory */
    memset( p_picture, 0, sizeof(picture_t) );

    p_y = p_picture->Y_PIXELS;
    p_u = p_picture->U_PIXELS;
    p_v = p_picture->V_PIXELS;
    p_a = p_picture->A_PIXELS;
    i_pitch = p_picture->Y_PITCH;

    /* Initialize the region pixels (only the alpha will be changed later) */
    memset( p_y, 0xff, i_pitch * i_height );
    memset( p_u, 0x80, i_pitch * i_height );
    memset( p_v, 0x80, i_pitch * i_height );
    memset( p_a, 0x00, i_pitch * i_height );

    return p_picture;
}

/*****************************************************************************
 * Displays an OSD slider.
 * Types are: OSD_HOR_SLIDER and OSD_VERT_SLIDER.
 *****************************************************************************/
picture_t *osd_Slider( int i_width, int i_height, int i_position, short i_type )
{
    picture_t *p_picture;
    int i_x_margin, i_y_margin, i_x, i_y;

    p_picture = osd_CreatePicture( i_width, i_height );
    if( p_picture == NULL )
        return NULL;

    i_y_margin = i_height / 10;
    i_x_margin = i_y_margin;
    if( i_type == OSD_HOR_SLIDER )
    {
        i_width = i_width - 2 * i_x_margin;
        i_height = i_height / 20;
        i_x = i_x_margin;
        i_y = i_height - i_y_margin - i_height;
    }
    else
    {
        i_width = i_width / 40;
        i_height = i_height - 2 * i_y_margin;
        i_x = i_width - i_x_margin - i_width;
        i_y = i_y_margin;
    }

    if( i_type == OSD_HOR_SLIDER )
    {
        int i_x_pos = ( i_width - 2 ) * i_position / 100;
        DrawRect( p_picture, i_x_pos - 1, 2, i_x_pos + 1,
                  i_height - 3, STYLE_FILLED );
        DrawRect( p_picture, 0, 0, i_width - 1, i_height - 1, STYLE_EMPTY );
    }
    else if( i_type == OSD_VERT_SLIDER )
    {
        int i_y_pos = i_height / 2;
        DrawRect( p_picture, 2, i_height - ( i_height - 2 ) * i_position / 100,
                  i_width - 3, i_height - 3, STYLE_FILLED );
        DrawRect( p_picture, 1, i_y_pos, 1, i_y_pos, STYLE_FILLED );
        DrawRect( p_picture, i_width - 2, i_y_pos,
                  i_width - 2, i_y_pos, STYLE_FILLED );
        DrawRect( p_picture, 0, 0, i_width - 1, i_height - 1, STYLE_EMPTY );
    }

    return p_picture;
}

/*****************************************************************************
 * Displays an OSD icon.
 * Types are: OSD_PLAY_ICON, OSD_PAUSE_ICON, OSD_SPEAKER_ICON, OSD_MUTE_ICON
 *****************************************************************************/
picture_t *osd_Icon( int i_width, int i_height, short i_type )
{
    picture_t *p_picture = NULL;
    int i_x_margin, i_y_margin, i_x, i_y;

    p_picture = osd_CreatePicture( i_width, i_height );
    if( p_picture == NULL )
        return NULL;

    i_y_margin = i_height / 15;
    i_x_margin = i_y_margin;
    i_x = i_width - i_x_margin - i_width;
    i_y = i_y_margin;

    if( i_type == OSD_PAUSE_ICON )
    {
        int i_bar_width = i_width / 3;
        DrawRect( p_picture, 0, 0, i_bar_width - 1, i_height -1, STYLE_FILLED );
        DrawRect( p_picture, i_width - i_bar_width, 0,
                  i_width - 1, i_height - 1, STYLE_FILLED );
    }
    else if( i_type == OSD_PLAY_ICON )
    {
        int i_mid = i_height >> 1;
        int i_delta = ( i_width - i_mid ) >> 1;
        int i_y2 = ( ( i_height - 1 ) >> 1 ) * 2;
        DrawTriangle( p_picture, i_delta, 0, i_width - i_delta, i_y2,
                      STYLE_FILLED );
    }
    else if( i_type == OSD_SPEAKER_ICON || i_type == OSD_MUTE_ICON )
    {
        int i_mid = i_height >> 1;
        int i_delta = ( i_width - i_mid ) >> 1;
        int i_y2 = ( ( i_height - 1 ) >> 1 ) * 2;
        DrawRect( p_picture, i_delta, i_mid / 2, i_width - i_delta,
                  i_height - 1 - i_mid / 2, STYLE_FILLED );
        DrawTriangle( p_picture, i_width - i_delta, 0, i_delta, i_y2,
                      STYLE_FILLED );
        if( i_type == OSD_MUTE_ICON )
        {
            uint8_t *p_a = p_picture->A_PIXELS;
            int i_pitch = p_picture->Y_PITCH;
            int i;
            for( i = 1; i < i_pitch; i++ )
            {
                int k = i + ( i_height - i - 1 ) * i_pitch;
                p_a[ k ] = 0xff - p_a[ k ];
            }
        }
    }
    
    return p_picture;
}
