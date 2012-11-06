/*****************************************************************************
 * vobsub.h: Vobsub support
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: John Stebbins
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

static inline void vobsub_palette_rgb2yuv( uint32_t *pu_palette )
{
    int i;
    for( i = 0; i < 16; i++ )
    {
        uint8_t r, g, b, y, u, v;
        r = (pu_palette[i] >> 16) & 0xff;
        g = (pu_palette[i] >> 8) & 0xff;
        b = (pu_palette[i] >> 0) & 0xff;
        y = (uint8_t) __MIN(abs(r * 2104 + g * 4130 + b * 802 + 4096 + 131072) >> 13, 235);
        u = (uint8_t) __MIN(abs(r * -1214 + g * -2384 + b * 3598 + 4096 + 1048576) >> 13, 240);
        v = (uint8_t) __MIN(abs(r * 3598 + g * -3013 + b * -585 + 4096 + 1048576) >> 13, 240);
        pu_palette[i] = (y&0xff)<<16 | (v&0xff)<<8 | (u&0xff);
    }
}

static inline int vobsub_palette_parse( const char *psz_buf, uint32_t *pu_palette )
{
    if( sscanf( psz_buf, "palette: "
                "%"PRIx32", %"PRIx32", %"PRIx32", %"PRIx32", "
                "%"PRIx32", %"PRIx32", %"PRIx32", %"PRIx32", "
                "%"PRIx32", %"PRIx32", %"PRIx32", %"PRIx32", "
                "%"PRIx32", %"PRIx32", %"PRIx32", %"PRIx32"",
                &pu_palette[0], &pu_palette[1], &pu_palette[2], &pu_palette[3],
                &pu_palette[4], &pu_palette[5], &pu_palette[6], &pu_palette[7],
                &pu_palette[8], &pu_palette[9], &pu_palette[10], &pu_palette[11],
                &pu_palette[12], &pu_palette[13], &pu_palette[14], &pu_palette[15] ) == 16 )
    {
        vobsub_palette_rgb2yuv( pu_palette );
        return VLC_SUCCESS;
    }
    else
    {
        return VLC_EGENERIC;
    }
}

static inline int vobsub_size_parse( const char *psz_buf,
                                     int *pi_original_frame_width,
                                     int *pi_original_frame_height )
{
    if( sscanf( psz_buf, "size: %dx%d",
                pi_original_frame_width, pi_original_frame_height ) == 2 )
    {
        return VLC_SUCCESS;
    }
    else
    {
        return VLC_EGENERIC;
    }
}

