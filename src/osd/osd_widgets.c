/*****************************************************************************
 * osd_widgets.c : OSD widgets manipulation functions
 *****************************************************************************
 * Copyright (C) 2004-2005 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                                /* free() */
#include <vlc/vout.h>
#include <vlc_osd.h>

#include <vlc_video.h>
#include <vlc_filter.h>

#define STYLE_EMPTY 0
#define STYLE_FILLED 1

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void DrawRect( subpicture_t *, int, int, int, int, short );
static void DrawTriangle( subpicture_t *, int, int, int, int, short );
static int  CreatePicture( spu_t *, subpicture_t *, int, int, int, int );
static subpicture_t *osd_CreateWidget( spu_t *, int );

/*****************************************************************************
 * Draws a rectangle at the given position in the subpic.
 * It may be filled (fill == STYLE_FILLED) or empty (fill == STYLE_EMPTY).
 *****************************************************************************/
static void DrawRect( subpicture_t *p_subpic, int i_x1, int i_y1,
                      int i_x2, int i_y2, short fill )
{
    int x, y;
    uint8_t *p_a = p_subpic->p_region->picture.A_PIXELS;
    int i_pitch = p_subpic->p_region->picture.Y_PITCH;

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
static void DrawTriangle( subpicture_t *p_subpic, int i_x1, int i_y1,
                          int i_x2, int i_y2, short fill )
{
    int x, y, i_mid, h;
    uint8_t *p_a = p_subpic->p_region->picture.A_PIXELS;
    int i_pitch = p_subpic->p_region->picture.Y_PITCH;

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
 * Create Picture: creates subpicture region and picture
 *****************************************************************************/
static int CreatePicture( spu_t *p_spu, subpicture_t *p_subpic,
                           int i_x, int i_y, int i_width, int i_height )
{
    uint8_t *p_y, *p_u, *p_v, *p_a;
    video_format_t fmt;
    int i_pitch;

    /* Create a new subpicture region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('Y','U','V','A');
    fmt.i_aspect = 0;
    fmt.i_width = fmt.i_visible_width = i_width;
    fmt.i_height = fmt.i_visible_height = i_height;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_subpic->p_region = p_subpic->pf_create_region( VLC_OBJECT(p_spu), &fmt );
    if( !p_subpic->p_region )
    {
        msg_Err( p_spu, "cannot allocate SPU region" );
        return VLC_EGENERIC;
    }

    p_subpic->p_region->i_x = i_x;
    p_subpic->p_region->i_y = i_y;
    p_y = p_subpic->p_region->picture.Y_PIXELS;
    p_u = p_subpic->p_region->picture.U_PIXELS;
    p_v = p_subpic->p_region->picture.V_PIXELS;
    p_a = p_subpic->p_region->picture.A_PIXELS;
    i_pitch = p_subpic->p_region->picture.Y_PITCH;

    /* Initialize the region pixels (only the alpha will be changed later) */
    memset( p_y, 0xff, i_pitch * p_subpic->p_region->fmt.i_height );
    memset( p_u, 0x80, i_pitch * p_subpic->p_region->fmt.i_height );
    memset( p_v, 0x80, i_pitch * p_subpic->p_region->fmt.i_height );
    memset( p_a, 0x00, i_pitch * p_subpic->p_region->fmt.i_height );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Creates and initializes an OSD widget.
 *****************************************************************************/
subpicture_t *osd_CreateWidget( spu_t *p_spu, int i_channel )
{
    subpicture_t *p_subpic;
    mtime_t i_now = mdate();

    /* Create and initialize a subpicture */
    p_subpic = spu_CreateSubpicture( p_spu );
    if( p_subpic == NULL ) return NULL;

    p_subpic->i_channel = i_channel;
    p_subpic->i_start = i_now;
    p_subpic->i_stop = i_now + 1200000;
    p_subpic->b_ephemer = VLC_TRUE;
    p_subpic->b_fade = VLC_TRUE;

    return p_subpic;
}

/*****************************************************************************
 * Displays an OSD slider.
 * Types are: OSD_HOR_SLIDER and OSD_VERT_SLIDER.
 *****************************************************************************/
int osd_Slider( vlc_object_t *p_this, spu_t *p_spu,
    int i_render_width, int i_render_height,
    int i_channel, int i_position, short i_type )
{
    subpicture_t *p_subpic;
    int i_x_margin, i_y_margin, i_x, i_y, i_width, i_height;

    p_subpic = osd_CreateWidget( p_spu, i_channel );
    if( p_subpic == NULL )
    {
        return VLC_EGENERIC;
    }

    i_y_margin = i_render_height / 10;
    i_x_margin = i_y_margin;
    if( i_type == OSD_HOR_SLIDER )
    {
        i_width = i_render_width - 2 * i_x_margin;
        i_height = i_render_height / 20;
        i_x = i_x_margin;
        i_y = i_render_height - i_y_margin - i_height;
    }
    else
    {
        i_width = i_render_width / 40;
        i_height = i_render_height - 2 * i_y_margin;
        i_x = i_render_width - i_x_margin - i_width;
        i_y = i_y_margin;
    }

    /* Create subpicture region and picture */
    CreatePicture( p_spu, p_subpic, i_x, i_y, i_width, i_height );

    if( i_type == OSD_HOR_SLIDER )
    {
        int i_x_pos = ( i_width - 2 ) * i_position / 100;
        DrawRect( p_subpic, i_x_pos - 1, 2, i_x_pos + 1,
                  i_height - 3, STYLE_FILLED );
        DrawRect( p_subpic, 0, 0, i_width - 1, i_height - 1, STYLE_EMPTY );
    }
    else if( i_type == OSD_VERT_SLIDER )
    {
        int i_y_pos = i_height / 2;
        DrawRect( p_subpic, 2, i_height - ( i_height - 2 ) * i_position / 100,
                  i_width - 3, i_height - 3, STYLE_FILLED );
        DrawRect( p_subpic, 1, i_y_pos, 1, i_y_pos, STYLE_FILLED );
        DrawRect( p_subpic, i_width - 2, i_y_pos,
                  i_width - 2, i_y_pos, STYLE_FILLED );
        DrawRect( p_subpic, 0, 0, i_width - 1, i_height - 1, STYLE_EMPTY );
    }

    spu_DisplaySubpicture( p_spu, p_subpic );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Displays an OSD icon.
 * Types are: OSD_PLAY_ICON, OSD_PAUSE_ICON, OSD_SPEAKER_ICON, OSD_MUTE_ICON
 *****************************************************************************/
int osd_Icon( vlc_object_t *p_this, spu_t *p_spu,
    int i_render_width, int i_render_height, int i_channel, short i_type )
{
    subpicture_t *p_subpic;
    int i_x_margin, i_y_margin, i_x, i_y, i_width, i_height;

    p_subpic = osd_CreateWidget( p_spu, i_channel );
    if( p_subpic == NULL )
    {
        return VLC_EGENERIC;
    }

    i_y_margin = i_render_height / 15;
    i_x_margin = i_y_margin;
    i_width = i_render_width / 20;
    i_height = i_width;
    i_x = i_render_width - i_x_margin - i_width;
    i_y = i_y_margin;

    /* Create subpicture region and picture */
    CreatePicture( p_spu, p_subpic, i_x, i_y, i_width, i_height );

    if( i_type == OSD_PAUSE_ICON )
    {
        int i_bar_width = i_width / 3;
        DrawRect( p_subpic, 0, 0, i_bar_width - 1, i_height -1, STYLE_FILLED );
        DrawRect( p_subpic, i_width - i_bar_width, 0,
                  i_width - 1, i_height - 1, STYLE_FILLED );
    }
    else if( i_type == OSD_PLAY_ICON )
    {
        int i_mid = i_height >> 1;
        int i_delta = ( i_width - i_mid ) >> 1;
        int i_y2 = ( ( i_height - 1 ) >> 1 ) * 2;
        DrawTriangle( p_subpic, i_delta, 0, i_width - i_delta, i_y2,
                      STYLE_FILLED );
    }
    else if( i_type == OSD_SPEAKER_ICON || i_type == OSD_MUTE_ICON )
    {
        int i_mid = i_height >> 1;
        int i_delta = ( i_width - i_mid ) >> 1;
        int i_y2 = ( ( i_height - 1 ) >> 1 ) * 2;
        DrawRect( p_subpic, i_delta, i_mid / 2, i_width - i_delta,
                  i_height - 1 - i_mid / 2, STYLE_FILLED );
        DrawTriangle( p_subpic, i_width - i_delta, 0, i_delta, i_y2,
                      STYLE_FILLED );
        if( i_type == OSD_MUTE_ICON )
        {
            uint8_t *p_a = p_subpic->p_region->picture.A_PIXELS;
            int i_pitch = p_subpic->p_region->picture.Y_PITCH;
            int i;
            for( i = 1; i < i_pitch; i++ )
            {
                int k = i + ( i_height - i - 1 ) * i_pitch;
                p_a[ k ] = 0xff - p_a[ k ];
            }
        }
    }

    spu_DisplaySubpicture( p_spu, p_subpic );

    return VLC_SUCCESS;
}
