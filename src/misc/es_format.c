/*****************************************************************************
 * es_format.c : es_format_t helpers.
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * $Id$
 *
 * Author: Laurent Aimar <fenrir@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_es.h>


/*****************************************************************************
 * BinaryLog: computes the base 2 log of a binary value
 *****************************************************************************
 * This functions is used by MaskToShift, to get a bit index from a binary
 * value.
 *****************************************************************************/
static int BinaryLog( uint32_t i )
{
    int i_log = 0;

    if( i == 0 ) return -31337;

    if( i & 0xffff0000 ) i_log += 16;
    if( i & 0xff00ff00 ) i_log += 8;
    if( i & 0xf0f0f0f0 ) i_log += 4;
    if( i & 0xcccccccc ) i_log += 2;
    if( i & 0xaaaaaaaa ) i_log += 1;

    return i_log;
}

/**
 * It transforms a color mask into right and left shifts
 * FIXME copied from video_output.c
 */
static void MaskToShift( int *pi_left, int *pi_right, uint32_t i_mask )
{
    uint32_t i_low, i_high;            /* lower hand higher bits of the mask */

    if( !i_mask )
    {
        *pi_left = *pi_right = 0;
        return;
    }

    /* Get bits */
    i_low = i_high = i_mask;

    i_low &= - (int32_t)i_low;          /* lower bit of the mask */
    i_high += i_low;                    /* higher bit of the mask */

    /* Transform bits into an index. Also deal with i_high overflow, which
     * is faster than changing the BinaryLog code to handle 64 bit integers. */
    i_low =  BinaryLog (i_low);
    i_high = i_high ? BinaryLog (i_high) : 32;

    /* Update pointers and return */
    *pi_left =   i_low;
    *pi_right = (8 - i_high + i_low);
}

/* */
void video_format_FixRgb( video_format_t *p_fmt )
{
    /* FIXME find right default mask */
    if( !p_fmt->i_rmask || !p_fmt->i_gmask || !p_fmt->i_bmask )
    {
        switch( p_fmt->i_chroma )
        {
        case VLC_FOURCC('R','V','1','5'):
            p_fmt->i_rmask = 0x7c00;
            p_fmt->i_gmask = 0x03e0;
            p_fmt->i_bmask = 0x001f;
            break;

        case VLC_FOURCC('R','V','1','6'):
            p_fmt->i_rmask = 0xf800;
            p_fmt->i_gmask = 0x07e0;
            p_fmt->i_bmask = 0x001f;
            break;

        case VLC_FOURCC('R','V','2','4'):
            p_fmt->i_rmask = 0xff0000;
            p_fmt->i_gmask = 0x00ff00;
            p_fmt->i_bmask = 0x0000ff;
            break;
        case VLC_FOURCC('R','V','3','2'):
            p_fmt->i_rmask = 0x00ff0000;
            p_fmt->i_gmask = 0x0000ff00;
            p_fmt->i_bmask = 0x000000ff;
            break;

        default:
            return;
        }
    }

    MaskToShift( &p_fmt->i_lrshift, &p_fmt->i_rrshift,
                 p_fmt->i_rmask );
    MaskToShift( &p_fmt->i_lgshift, &p_fmt->i_rgshift,
                 p_fmt->i_gmask );
    MaskToShift( &p_fmt->i_lbshift, &p_fmt->i_rbshift,
                 p_fmt->i_bmask );
}

void es_format_Init( es_format_t *fmt,
                     int i_cat, vlc_fourcc_t i_codec )
{
    fmt->i_cat                  = i_cat;
    fmt->i_codec                = i_codec;
    fmt->i_id                   = -1;
    fmt->i_group                = 0;
    fmt->i_priority             = 0;
    fmt->psz_language           = NULL;
    fmt->psz_description        = NULL;

    fmt->i_extra_languages      = 0;
    fmt->p_extra_languages      = NULL;

    memset( &fmt->audio, 0, sizeof(audio_format_t) );
    memset( &fmt->audio_replay_gain, 0, sizeof(audio_replay_gain_t) );
    memset( &fmt->video, 0, sizeof(video_format_t) );
    memset( &fmt->subs, 0, sizeof(subs_format_t) );

    fmt->b_packetized           = true;
    fmt->i_bitrate              = 0;
    fmt->i_extra                = 0;
    fmt->p_extra                = NULL;
}

int es_format_Copy( es_format_t *dst, const es_format_t *src )
{
    int i;
    memcpy( dst, src, sizeof( es_format_t ) );
    dst->psz_language = src->psz_language ? strdup( src->psz_language ) : NULL;
    dst->psz_description = src->psz_description ? strdup( src->psz_description ) : NULL;
    if( src->i_extra > 0 && dst->p_extra )
    {
        dst->i_extra = src->i_extra;
        dst->p_extra = malloc( src->i_extra );
        if(dst->p_extra)
            memcpy( dst->p_extra, src->p_extra, src->i_extra );
        else
            dst->i_extra = 0;
    }
    else
    {
        dst->i_extra = 0;
        dst->p_extra = NULL;
    }

    dst->subs.psz_encoding = dst->subs.psz_encoding ? strdup( src->subs.psz_encoding ) : NULL;

    if( src->video.p_palette )
    {
        dst->video.p_palette =
            (video_palette_t*)malloc( sizeof( video_palette_t ) );
        if(dst->video.p_palette)
        {
            memcpy( dst->video.p_palette, src->video.p_palette,
                sizeof( video_palette_t ) );
        }
    }

    if( dst->i_extra_languages && src->p_extra_languages)
    {
        dst->i_extra_languages = src->i_extra_languages;
        dst->p_extra_languages = (extra_languages_t*)
            malloc(dst->i_extra_languages * sizeof(*dst->p_extra_languages ));
        if( dst->p_extra_languages )
        {
            for( i = 0; i < dst->i_extra_languages; i++ ) {
                if( src->p_extra_languages[i].psz_language )
                    dst->p_extra_languages[i].psz_language = strdup( src->p_extra_languages[i].psz_language );
                else
                    dst->p_extra_languages[i].psz_language = NULL;
                if( src->p_extra_languages[i].psz_description )
                    dst->p_extra_languages[i].psz_description = strdup( src->p_extra_languages[i].psz_description );
                else
                    dst->p_extra_languages[i].psz_description = NULL;
            }
        }
        else
            dst->i_extra_languages = 0;
    }
    else
        dst->i_extra_languages = 0;
    return VLC_SUCCESS;
}

void es_format_Clean( es_format_t *fmt )
{
    free( fmt->psz_language );
    free( fmt->psz_description );

    if( fmt->i_extra > 0 ) free( fmt->p_extra );

    free( fmt->video.p_palette );
    free( fmt->subs.psz_encoding );

    if( fmt->i_extra_languages > 0 && fmt->p_extra_languages )
    {
        int i;
        for( i = 0; i < fmt->i_extra_languages; i++ )
        {
            free( fmt->p_extra_languages[i].psz_language );
            free( fmt->p_extra_languages[i].psz_description );
        }
        free( fmt->p_extra_languages );
    }

    /* es_format_Clean can be called multiple times */
    memset( fmt, 0, sizeof(*fmt) );
}

