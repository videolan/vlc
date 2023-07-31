/*****************************************************************************
 * vobsub.h: Vobsub support
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
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

static inline void vobsub_palette_argb2ayvu( const uint32_t *src, uint32_t *dst )
{
    int i;
    for( i = 0; i < VIDEO_PALETTE_CLUT_COUNT; i++ )
    {
        uint8_t r, g, b, y, u, v;
        r = (src[i] >> 16) & 0xff;
        g = (src[i] >> 8) & 0xff;
        b = (src[i] >> 0) & 0xff;
        y = (uint8_t) __MIN(abs(r * 2104 + g * 4130 + b * 802 + 4096 + 131072) >> 13, 235);
        u = (uint8_t) __MIN(abs(r * -1214 + g * -2384 + b * 3598 + 4096 + 1048576) >> 13, 240);
        v = (uint8_t) __MIN(abs(r * 3598 + g * -3013 + b * -585 + 4096 + 1048576) >> 13, 240);
        dst[i] = (y&0xff)<<16 | (v&0xff)<<8 | (u&0xff);
    }
}

static inline int vobsub_palette_parse( const char *psz_buf, uint32_t *pu_palette )
{
    uint32_t palette[VIDEO_PALETTE_CLUT_COUNT];
    if( sscanf( psz_buf, "palette: "
                "%" SCNx32", %" SCNx32 ", %" SCNx32 ", %" SCNx32 ", "
                "%" SCNx32", %" SCNx32 ", %" SCNx32 ", %" SCNx32 ", "
                "%" SCNx32", %" SCNx32 ", %" SCNx32 ", %" SCNx32 ", "
                "%" SCNx32", %" SCNx32 ", %" SCNx32 ", %" SCNx32 "",
                &palette[0], &palette[1], &palette[2], &palette[3],
                &palette[4], &palette[5], &palette[6], &palette[7],
                &palette[8], &palette[9], &palette[10], &palette[11],
                &palette[12], &palette[13], &palette[14], &palette[15] ) == ARRAY_SIZE(palette) )
    {
        vobsub_palette_argb2ayvu( palette, pu_palette );
        return VLC_SUCCESS;
    }
    else
    {
        return VLC_EGENERIC;
    }
}

static inline int vobsub_size_parse( const char *psz_buf,
                                     unsigned *pi_original_frame_width,
                                     unsigned *pi_original_frame_height )
{
    unsigned w, h;
    if( sscanf( psz_buf, "size: %ux%u", &w, &h ) == 2 )
    {
        *pi_original_frame_width = w;
        *pi_original_frame_height = h;
        return VLC_SUCCESS;
    }
    else
    {
        return VLC_EGENERIC;
    }
}

static inline void vobsub_extra_parse(vlc_object_t *o, subs_format_t *subs,
                                      const uint8_t *buf, size_t buf_size)
{
    char *psz_start;
    char *psz_buf = (char*)malloc( buf_size + 1);
    if( unlikely( psz_buf == NULL ) )
        return;

    memcpy( psz_buf, buf, buf_size );
    psz_buf[buf_size] = '\0';

    psz_start = strstr( psz_buf, "size:" );
    if( psz_start &&
        vobsub_size_parse( psz_start,
                            &subs->spu.i_original_frame_width,
                            &subs->spu.i_original_frame_height ) == VLC_SUCCESS )
    {
        msg_Dbg( o, "original frame size: %ux%u",
                    subs->spu.i_original_frame_width,
                    subs->spu.i_original_frame_height );
    }
    else
    {
        msg_Warn( o, "reading original frame size failed" );
    }

    psz_start = strstr( psz_buf, "palette:" );
    if( psz_start &&
        vobsub_palette_parse( psz_start, subs->spu.palette ) == VLC_SUCCESS )
    {
        subs->spu.b_palette = true;
        msg_Dbg( o, "vobsub palette read" );
    }
    else
    {
        msg_Warn( o, "reading original palette failed" );
    }
    free( psz_buf );
}
