/*****************************************************************************
 * vout_spuregion_helper.h : vout subpicture region helpers
 *****************************************************************************
 * Copyright (C) 2017 VLC authors, VideoLAN and VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include <vlc_image.h>

#define RGB2YUV( R, G, B ) \
    ((0.257 * R) + (0.504 * G) + (0.098 * B) + 16), \
    (-(0.148 * R) - (0.291 * G) + (0.439 * B) + 128),\
    ((0.439 * R) - (0.368 * G) - (0.071 * B) + 128)

#define HEX2YUV( rgb ) \
    RGB2YUV( (rgb >> 16), ((rgb & 0xFF00) >> 8), (rgb & 0xFF) )

static inline void
spuregion_CreateVGradientPalette( video_palette_t *p_palette, uint8_t i_splits,
                                  uint32_t argb1, uint32_t argb2 )
{
    for( uint8_t i = 0; i<i_splits; i++ )
    {
        uint32_t rgb1 = argb1 & 0x00FFFFFF;
        uint32_t rgb2 = argb2 & 0x00FFFFFF;

        uint32_t r = ((((rgb1 >> 16) * (i_splits - i)) + (rgb2 >> 16) * i)) / i_splits;
        uint32_t g = (((((rgb1 >> 8) & 0xFF) * (i_splits - i)) + ((rgb2 >> 8) & 0xFF) * i)) / i_splits;
        uint32_t b = ((((rgb1 & 0xFF) * (i_splits - i)) + (rgb2 & 0xFF) * i)) / i_splits;
        uint8_t entry[4] = { RGB2YUV( r,g,b ), argb1 >> 24 };
        memcpy( p_palette->palette[i], entry, 4 );
    }
    p_palette->i_entries = i_splits;
}

static inline void
spuregion_CreateVGradientFill( plane_t *p, uint8_t i_splits )
{
    const int i_split = p->i_visible_lines / i_splits;
    const int i_left = p->i_visible_lines % i_splits + p->i_lines - p->i_visible_lines;
    for( int i = 0; i<i_splits; i++ )
    {
        memset( &p->p_pixels[p->i_pitch * (i * i_split)],
                i,
                p->i_pitch * i_split );
    }
    memset( &p->p_pixels[p->i_pitch * (i_splits - 1) * i_split],
            i_splits - 1,
            p->i_pitch * i_left );
}


static inline subpicture_region_t *
spuregion_CreateFromPicture( vlc_object_t *p_this, video_format_t *p_fmt,
                             const char *psz_uri )
{
    picture_t *p_pic = NULL;
    struct vlc_logger *logger = p_this->logger;
    bool no_interact = p_this->no_interact;
    p_this->logger = NULL;
    p_this->no_interact = true;
    image_handler_t *p_image = image_HandlerCreate( p_this );
    if( p_image )
    {
        p_pic = image_ReadUrl( p_image, psz_uri, p_fmt );
        image_HandlerDelete( p_image );
    }
    p_this->no_interact = no_interact;
    p_this->logger = logger;

    if(!p_pic)
        return NULL;

    subpicture_region_t *region = subpicture_region_New(p_fmt);
    if (!region)
    {
        picture_Release( p_pic );
        return NULL;
    }

    picture_Release( region->p_picture );
    region->p_picture = p_pic;

    return region;
}
