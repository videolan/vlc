/*****************************************************************************
 * bitmapinfoheader.h : BITMAPINFOHEADER handling
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 *               2018      VideoLabs, VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc_common.h>
#include <vlc_es.h>
#include <vlc_codecs.h>
#include <limits.h>

/* biCompression / Others are FourCC */
#ifndef BI_RGB
 #define BI_RGB             0x0000
 #define BI_RLE8            0x0001
 #define BI_RLE4            0x0002
 #define BI_BITFIELDS       0x0003
 #define BI_JPEG            0x0004
 #define BI_PNG             0x0005
#endif
#define BI_ALPHAFIELDS      0x0006
#define BI_CMYK             0x000B
#define BI_CMYKRLE8         0x000C
#define BI_CMYKRLE4         0x000D

static const struct
{
    vlc_fourcc_t codec;
    uint32_t i_rmask, i_gmask, i_bmask;
} bitmap_rgb_masks[] = {
    { VLC_CODEC_RGB15,      0x7c00, /* R5G5B5 */
                            0x03e0,
                            0x001f, },
    { VLC_CODEC_RGB16,      0xf800, /* R5G6B5 */
                            0x07e0,
                            0x001f, },
    { VLC_CODEC_RGB24,      0x000000ff, /* BGR (see biBitCount) */
                            0x0000ff00,
                            0x00ff0000, },
    { VLC_CODEC_RGB32,      0x0000ff00, /* This is in BGR0 format */
                            0x00ff0000,
                            0xff000000U, },
};

static inline void SetBitmapRGBMasks( vlc_fourcc_t i_fourcc, video_format_t *fmt )
{
    for( size_t i=0; i<ARRAY_SIZE(bitmap_rgb_masks); i++ )
    {
        if( bitmap_rgb_masks[i].codec == i_fourcc )
        {
            fmt->i_rmask = bitmap_rgb_masks[i].i_rmask;
            fmt->i_gmask = bitmap_rgb_masks[i].i_gmask;
            fmt->i_bmask = bitmap_rgb_masks[i].i_bmask;
            fmt->i_chroma = i_fourcc;
            video_format_FixRgb( fmt );
            break;
        }
    }
}

static inline bool MatchBitmapRGBMasks( const es_format_t *fmt )
{
    for( size_t i=0; i<ARRAY_SIZE(bitmap_rgb_masks); i++ )
    {
        if( bitmap_rgb_masks[i].codec == fmt->i_codec )
        {
            return fmt->video.i_rmask == bitmap_rgb_masks[i].i_rmask &&
                   fmt->video.i_gmask == bitmap_rgb_masks[i].i_gmask &&
                   fmt->video.i_bmask == bitmap_rgb_masks[i].i_bmask;
        }
    }
    return false;
}

struct bitmapinfoheader_properties
{
    bool b_flipped;
    unsigned i_stride;
};

static inline int ParseBitmapInfoHeader( VLC_BITMAPINFOHEADER *p_bih, size_t i_bih,
                                         es_format_t *fmt,
                                         struct bitmapinfoheader_properties *p_props )
{
    /* Extradata is the remainder of the chunk less the BIH */
    const uint8_t *p_bihextra = (const uint8_t *) &p_bih[1];
    size_t i_bihextra;
    if( i_bih <= INT_MAX - sizeof(VLC_BITMAPINFOHEADER) &&
            i_bih >= sizeof(VLC_BITMAPINFOHEADER) )
        i_bihextra = i_bih - sizeof(VLC_BITMAPINFOHEADER);
    else
        i_bihextra = 0;

    if( p_bih->biCompression == BI_RGB ||
        p_bih->biCompression == BI_BITFIELDS )
    {
        switch( p_bih->biBitCount )
        {
            case 32:
                fmt->video.i_chroma = fmt->i_codec = VLC_CODEC_RGB32;
                break;
            case 24:
                fmt->video.i_chroma = fmt->i_codec = VLC_CODEC_RGB24; /* BGR (see biBitCount) */
                break;
            case 16:
                fmt->video.i_chroma = fmt->i_codec = VLC_CODEC_RGB16; /* RGB (5,6,5 bits) */
                break;
            case 15: /* RGB (B least 5 bits) */
                fmt->video.i_chroma = fmt->i_codec = VLC_CODEC_RGB15;
                break;
            case 9: /* <- TODO check that */
                fmt->video.i_chroma = fmt->i_codec = VLC_CODEC_I410;
                break;
            case 8:
                if ( p_bih->biClrUsed )
                    fmt->video.i_chroma = fmt->i_codec = VLC_CODEC_RGBP;
                else
                    fmt->video.i_chroma = fmt->i_codec = VLC_CODEC_GREY;
                break;
            default:
                if( p_bih->biClrUsed < 8 )
                    fmt->video.i_chroma = fmt->i_codec = VLC_CODEC_RGBP;
                break;
        }

        if( p_bih->biCompression == BI_BITFIELDS ) /* Only 16 & 32 */
        {
            if( i_bihextra >= 3 * sizeof(uint32_t) )
            {
                fmt->video.i_rmask = GetDWLE( &p_bihextra[0] );
                fmt->video.i_gmask = GetDWLE( &p_bihextra[4] );
                fmt->video.i_bmask = GetDWLE( &p_bihextra[8] );
                video_format_FixRgb( &fmt->video );
                if( i_bihextra >= 4 * sizeof(uint32_t) ) /* Alpha channel ? */
                {
                    uint32_t i_alpha = GetDWLE( &p_bihextra[8] );
                    if( i_alpha == 0xFF )
                    {
                        switch (fmt->i_codec)
                        {
                            case VLC_CODEC_RGB32: // unknown mask
                            case VLC_CODEC_BGRX:
                                fmt->i_codec = fmt->video.i_chroma = VLC_CODEC_BGRA;
                                break;
                            case VLC_CODEC_RGBX:
                                fmt->i_codec = fmt->video.i_chroma = VLC_CODEC_RGBA;
                                break;
                            case VLC_CODEC_XBGR:
                                fmt->i_codec = fmt->video.i_chroma = VLC_CODEC_ABGR;
                                break;
                            case VLC_CODEC_XRGB:
                                fmt->i_codec = fmt->video.i_chroma = VLC_CODEC_ARGB;
                                break;
                        }
                    }
                }
            }
            else
            {
                if (p_bih->biBitCount == 32)
                    fmt->i_codec = VLC_CODEC_BGRX;
                else
                    SetBitmapRGBMasks( fmt->i_codec, &fmt->video );
            }
        }
        else if( fmt->i_codec == VLC_CODEC_RGBP )
        {
            /* The palette should not be included in biSize, but come
             * directly after BITMAPINFORHEADER in the BITMAPINFO structure */
            fmt->video.p_palette = malloc( sizeof(video_palette_t) );
            if ( fmt->video.p_palette )
            {
                fmt->video.p_palette->i_entries = __MIN(i_bihextra/4, 256);
                for( int k = 0; k < fmt->video.p_palette->i_entries; k++ )
                {
                    for( int j = 0; j < 4; j++ )
                        fmt->video.p_palette->palette[k][j] = p_bihextra[4*k+j];
                }
            }
        }
        else
        {
            if (p_bih->biBitCount == 32)
                fmt->i_codec = VLC_CODEC_BGRX;
            else
                SetBitmapRGBMasks( fmt->i_codec, &fmt->video );
        }

        p_props->i_stride = p_bih->biWidth * (p_bih->biBitCount >> 3);
        /* Unintuitively RGB DIB are always coded from bottom to top,
         * except when height is negative */
        if ( p_bih->biHeight <= INT32_MAX )
            p_props->b_flipped = true;
        /* else
         *     set below to positive value */
    }
    else /* Compressed codecs */
    {
        fmt->i_codec = vlc_fourcc_GetCodec( VIDEO_ES, p_bih->biCompression );

        /* Copy extradata if any */
        if( i_bihextra > 0 )
        {
            fmt->p_extra = malloc( i_bihextra );
            if( unlikely(fmt->p_extra == NULL) )
                return VLC_ENOMEM;
            fmt->i_extra = i_bihextra;
            memcpy( fmt->p_extra, p_bihextra, i_bihextra );
        }

        /* Shitty VLC muxed files storing chroma in biCompression */
        if (p_bih->biCompression == VLC_FOURCC('R','V','3','2'))
            fmt->i_codec = VLC_CODEC_BGRX;
        else
            SetBitmapRGBMasks( fmt->i_codec, &fmt->video );
    }

    video_format_Setup( &fmt->video, fmt->i_codec,
                        p_bih->biWidth, p_bih->biHeight,
                        p_bih->biWidth, p_bih->biHeight,
                        fmt->video.i_sar_num, fmt->video.i_sar_den );

    /* Uncompressed Bitmap or YUV, YUV being always top to bottom whatever
     * height sign is, and compressed must also not use flip, so positive
     * values only here */
    if ( fmt->video.i_height > INT32_MAX )
    {
        fmt->video.i_visible_height =
        fmt->video.i_height = -1 * p_bih->biHeight;
    }

    return VLC_SUCCESS;
}

static inline VLC_BITMAPINFOHEADER * CreateBitmapInfoHeader( const es_format_t *fmt,
                                                             size_t *pi_total )
{
    uint16_t biBitCount = 0;
    uint32_t biCompression = BI_RGB;
    bool b_has_alpha = false;
    switch( fmt->i_codec )
    {
        case VLC_CODEC_BGRX:
            biBitCount = 32;
            break;
        case VLC_CODEC_XBGR:
        case VLC_CODEC_XRGB:
        case VLC_CODEC_RGBX:
            biBitCount = 32;
            biCompression = BI_BITFIELDS;
            break;
        case VLC_CODEC_RGB32:
            biBitCount = 32;
            if ( !MatchBitmapRGBMasks( fmt ) )
                biCompression = BI_BITFIELDS;
            break;
        case VLC_CODEC_BGRA:
        case VLC_CODEC_RGBA:
        case VLC_CODEC_ARGB:
        case VLC_CODEC_ABGR:
            biBitCount = 32;
            biCompression = BI_BITFIELDS;
            b_has_alpha = true;
            break;
        case VLC_CODEC_RGB24:
            biBitCount = 24;
            break;
        case VLC_CODEC_RGB16:
        case VLC_CODEC_RGB15:
            biBitCount = 16;
            biCompression = BI_BITFIELDS;
            break;
        case VLC_CODEC_RGBP:
        case VLC_CODEC_GREY:
            biBitCount = 8;
            break;
        case VLC_CODEC_MP4V:
            biCompression = VLC_FOURCC( 'X', 'V', 'I', 'D' );
            break;
        default:
            biCompression = fmt->i_original_fourcc
                ? fmt->i_original_fourcc : fmt->i_codec;
            break;
    }

    size_t i_bih_extra = 0;
    size_t i_bmiColors = 0;
    if( biCompression == BI_BITFIELDS )
        i_bmiColors = (b_has_alpha) ? 16 : 12;
    else if ( fmt->i_codec == VLC_CODEC_RGBP )
        i_bmiColors = fmt->video.p_palette ? (fmt->video.p_palette->i_entries * 4) : 0;
    else
        i_bih_extra = fmt->i_extra;

    VLC_BITMAPINFOHEADER *p_bih = malloc( sizeof(VLC_BITMAPINFOHEADER) +
                                          i_bih_extra +  i_bmiColors );
    if( p_bih == NULL )
        return NULL;

    uint8_t *p_bih_extra = (uint8_t *) &p_bih[1];
    uint8_t *p_bmiColors = p_bih_extra + i_bih_extra;
    p_bih->biClrUsed = 0;
    if( biCompression == BI_BITFIELDS )
    {
        uint32_t i_rmask,i_gmask,i_bmask;
        switch( fmt->i_codec )
        {
            case VLC_CODEC_ABGR:
            case VLC_CODEC_XBGR:
                i_rmask = hton32(0x000000ff);
                i_gmask = hton32(0x0000ff00);
                i_bmask = hton32(0x00ff0000);
                break;
            case VLC_CODEC_ARGB:
            case VLC_CODEC_XRGB:
                i_rmask = hton32(0x00ff0000);
                i_gmask = hton32(0x0000ff00);
                i_bmask = hton32(0x000000ff);
                break;
            case VLC_CODEC_RGBA:
            case VLC_CODEC_RGBX:
                i_rmask = hton32(0xff000000);
                i_gmask = hton32(0x00ff0000);
                i_bmask = hton32(0x0000ff00);
                break;
            case VLC_CODEC_BGRA:
                i_rmask = hton32(0x0000ff00);
                i_gmask = hton32(0x00ff0000);
                i_bmask = hton32(0xff000000);
                break;
            default:
                i_rmask = fmt->video.i_rmask;
                i_gmask = fmt->video.i_gmask;
                i_bmask = fmt->video.i_bmask;
                break;
        }
        SetDWBE( &p_bmiColors[0], i_rmask );
        SetDWBE( &p_bmiColors[4], i_gmask );
        SetDWBE( &p_bmiColors[8], i_bmask );
        if( b_has_alpha )
        {
            SetDWBE( &p_bmiColors[12], ~(i_rmask | i_gmask | i_bmask) );
        }
    }
    else if( fmt->i_codec == VLC_CODEC_RGBP )
    {
        for( int i = 0; i < fmt->video.p_palette->i_entries; i++ )
            memcpy( &p_bmiColors[i * 4], fmt->video.p_palette->palette[i], 4 );
        p_bih->biClrUsed = fmt->video.p_palette->i_entries;
    }
    else if( fmt->i_extra )
    {
        memcpy( p_bih_extra, fmt->p_extra, fmt->i_extra );
    }

    p_bih->biSize = sizeof(VLC_BITMAPINFOHEADER) + i_bih_extra;
    p_bih->biCompression = biCompression;
    p_bih->biBitCount = biBitCount;
    p_bih->biWidth = fmt->video.i_visible_width;
    p_bih->biHeight = fmt->video.i_visible_height;
    p_bih->biPlanes = 1;
    p_bih->biSizeImage = 0;
    p_bih->biXPelsPerMeter = 0;
    p_bih->biYPelsPerMeter = 0;
    p_bih->biClrImportant = 0;

    *pi_total = sizeof(VLC_BITMAPINFOHEADER) + i_bih_extra +  i_bmiColors;
    return p_bih;
}
