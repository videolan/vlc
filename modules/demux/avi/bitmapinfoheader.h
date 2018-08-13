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
#define BI_RGB              0x0000
#define BI_RLE8             0x0001
#define BI_RLE4             0x0002
#define BI_BITFIELDS        0x0003
#define BI_JPEG             0x0004
#define BI_PNG              0x0005
#define BI_CMYK             0x000B
#define BI_CMYKRLE8         0x000C
#define BI_CMYKRLE4         0x000D

static inline void SetBitmapRGBMasks( vlc_fourcc_t i_fourcc, es_format_t *fmt )
{
    switch( i_fourcc )
    {
        case VLC_CODEC_RGB15:
            fmt->video.i_rmask = 0x7c00;
            fmt->video.i_gmask = 0x03e0;
            fmt->video.i_bmask = 0x001f;
            break;
        case VLC_CODEC_RGB16:
            fmt->video.i_rmask = 0xf800;
            fmt->video.i_gmask = 0x07e0;
            fmt->video.i_bmask = 0x001f;
            break;
        case VLC_CODEC_RGB24: /* BGR (see biBitCount) */
            fmt->video.i_bmask = 0x00ff0000;
            fmt->video.i_gmask = 0x0000ff00;
            fmt->video.i_rmask = 0x000000ff;
            break;
        case VLC_CODEC_RGB32: /* This is in BGRx format */
        case VLC_CODEC_RGBA:
            fmt->video.i_bmask = 0xff000000;
            fmt->video.i_gmask = 0x00ff0000;
            fmt->video.i_rmask = 0x0000ff00;
            break;
        default:
            return;
    }
    fmt->video.i_chroma = i_fourcc;
    video_format_FixRgb( &fmt->video );
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
    if( i_bih <= INT_MAX - sizeof(VLC_BITMAPINFOHEADER) )
        i_bihextra = i_bih - sizeof(VLC_BITMAPINFOHEADER);
    else
        i_bihextra = 0;

    if( p_bih->biCompression == BI_RGB ||
        p_bih->biCompression == BI_BITFIELDS )
    {
        switch( p_bih->biBitCount )
        {
            case 32:
                fmt->i_codec = VLC_CODEC_RGB32;
                SetBitmapRGBMasks( fmt->i_codec, fmt );
                break;
            case 24:
                fmt->i_codec = VLC_CODEC_RGB24; /* BGR (see biBitCount) */
                SetBitmapRGBMasks( fmt->i_codec, fmt );
                break;
            case 16:
                fmt->i_codec = VLC_CODEC_RGB16; /* RGB (5,6,5 bits) */
                SetBitmapRGBMasks( fmt->i_codec, fmt );
                break;
            case 15: /* RGB (B least 5 bits) */
                fmt->i_codec = VLC_CODEC_RGB15;
                SetBitmapRGBMasks( fmt->i_codec, fmt );
                break;
            case 9: /* <- TODO check that */
                fmt->i_codec = VLC_CODEC_I410;
                break;
            case 8:
                if ( p_bih->biClrUsed )
                    fmt->i_codec = VLC_CODEC_RGBP;
                else
                    fmt->i_codec = VLC_CODEC_GREY;
                break;
            default:
                if( p_bih->biClrUsed < 8 )
                    fmt->i_codec = VLC_CODEC_RGBP;
                break;
        }

        if( p_bih->biCompression == BI_BITFIELDS ) /* Only 16 & 32 */
        {
            if( i_bihextra >= 3 * sizeof(uint32_t) )
            {
                fmt->video.i_rmask = GetDWLE( &p_bihextra[0] );
                fmt->video.i_gmask = GetDWLE( &p_bihextra[4] );
                fmt->video.i_bmask = GetDWLE( &p_bihextra[8] );
                if( i_bihextra >= 4 * sizeof(uint32_t) ) /* Alpha channel ? */
                {
                    uint32_t i_alpha = GetDWLE( &p_bihextra[8] );
                    if( fmt->i_codec == VLC_CODEC_RGB32 && i_alpha == 0xFF )
                        fmt->i_codec = VLC_CODEC_BGRA;
                }
            }
            SetBitmapRGBMasks( fmt->i_codec, fmt ); /* override default masks shifts */
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

        p_props->i_stride = p_bih->biWidth * (p_bih->biBitCount >> 3);
        /* RGB DIB are coded from bottom to top */
        if ( p_bih->biHeight < INT32_MAX ) p_props->b_flipped = true;
    }
    else /* Compressed codecs */
    {
        fmt->i_codec = p_bih->biCompression;

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
        SetBitmapRGBMasks( fmt->i_codec, fmt );
    }

    fmt->video.i_visible_width =
    fmt->video.i_width  = p_bih->biWidth;
    fmt->video.i_visible_height =
    fmt->video.i_height = p_bih->biHeight;
    fmt->video.i_bits_per_pixel = p_bih->biBitCount;

    /* Uncompressed Bitmap or YUV, YUV being always topdown */
    if ( fmt->video.i_height > INT32_MAX )
        fmt->video.i_height =
            (unsigned int)(-(int)p_bih->biHeight);

    return VLC_SUCCESS;
}
