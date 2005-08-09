/*****************************************************************************
 * vlc_es.h: Elementary stream formats descriptions
 *****************************************************************************
 * Copyright (C) 1999-2001 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef _VLC_ES_H
#define _VLC_ES_H 1

/**
 * \file
 * This file defines the elementary streams format types
 */

/**
 * video palette data
 * \see video_format_t
 * \see subs_format_t
 */
struct video_palette_t
{
    int i_entries;      /**< to keep the compatibility with ffmpeg's palette */
    uint8_t palette[256][4];                   /**< 4-byte RGBA/YUVA palette */
};

/**
 * audio format description
 */
struct audio_format_t
{
    vlc_fourcc_t i_format;                          /**< audio format fourcc */
    unsigned int i_rate;                              /**< audio sample-rate */

    /* Describes the channels configuration of the samples (ie. number of
     * channels which are available in the buffer, and positions). */
    uint32_t     i_physical_channels;

    /* Describes from which original channels, before downmixing, the
     * buffer is derived. */
    uint32_t     i_original_channels;

    /* Optional - for A/52, SPDIF and DTS types : */
    /* Bytes used by one compressed frame, depends on bitrate. */
    unsigned int i_bytes_per_frame;

    /* Number of sampleframes contained in one compressed frame. */
    unsigned int        i_frame_length;
    /* Please note that it may be completely arbitrary - buffers are not
     * obliged to contain a integral number of so-called "frames". It's
     * just here for the division :
     * buffer_size = i_nb_samples * i_bytes_per_frame / i_frame_length
     */

    /* FIXME ? (used by the codecs) */
    int i_channels;
    int i_blockalign;
    int i_bitspersample;
};

#ifdef WORDS_BIGENDIAN
#   define AUDIO_FMT_S16_NE VLC_FOURCC('s','1','6','b')
#   define AUDIO_FMT_U16_NE VLC_FOURCC('u','1','6','b')
#else
#   define AUDIO_FMT_S16_NE VLC_FOURCC('s','1','6','l')
#   define AUDIO_FMT_U16_NE VLC_FOURCC('u','1','6','l')
#endif

/**
 * video format description
 */
struct video_format_t
{
    vlc_fourcc_t i_chroma;                               /**< picture chroma */
    unsigned int i_aspect;                                 /**< aspect ratio */

    unsigned int i_width;                                 /**< picture width */
    unsigned int i_height;                               /**< picture height */
    unsigned int i_x_offset;               /**< start offset of visible area */
    unsigned int i_y_offset;               /**< start offset of visible area */
    unsigned int i_visible_width;                 /**< width of visible area */
    unsigned int i_visible_height;               /**< height of visible area */

    unsigned int i_bits_per_pixel;             /**< number of bits per pixel */

    unsigned int i_sar_num;                   /**< sample/pixel aspect ratio */
    unsigned int i_sar_den;

    unsigned int i_frame_rate;                     /**< frame rate numerator */
    unsigned int i_frame_rate_base;              /**< frame rate denominator */

    int i_rmask, i_gmask, i_bmask;          /**< color masks for RGB chroma */
    video_palette_t *p_palette;              /**< video palette from demuxer */
};

/**
 * subtitles format description
 */
struct subs_format_t
{
    /* the character encoding of the text of the subtitle.
     * all gettext recognized shorts can be used */
    char *psz_encoding;


    int  i_x_origin; /**< x coordinate of the subtitle. 0 = left */
    int  i_y_origin; /**< y coordinate of the subtitle. 0 = top */

    struct
    {
        /*  */
        uint32_t palette[16+1];

        /* the width of the original movie the spu was extracted from */
        int	i_original_frame_width;
        /* the height of the original movie the spu was extracted from */
        int	i_original_frame_height;
    } spu;

    struct
    {
        int i_id;
    } dvb;
};

/**
 * ES definition
 */
struct es_format_t
{
    int             i_cat;
    vlc_fourcc_t    i_codec;

    int             i_id;       /* -1: let the core mark the right id
                                   >=0: valid id */
    int             i_group;    /* -1 : standalone
                                   >= 0 then a "group" (program) is created
                                        for each value */
    int             i_priority; /*  -2 : mean not selectable by the users
                                    -1 : mean not selected by default even
                                        when no other stream
                                    >=0: priority */
    char            *psz_language;
    char            *psz_description;

    audio_format_t audio;
    video_format_t video;
    subs_format_t  subs;

    unsigned int   i_bitrate;

    vlc_bool_t     b_packetized; /* wether the data is packetized
                                    (ie. not truncated) */
    int     i_extra;
    void    *p_extra;

};

/* ES Categories */
#define UNKNOWN_ES      0x00
#define VIDEO_ES        0x01
#define AUDIO_ES        0x02
#define SPU_ES          0x03
#define NAV_ES          0x04

static inline void es_format_Init( es_format_t *fmt,
                                   int i_cat, vlc_fourcc_t i_codec )
{
    fmt->i_cat                  = i_cat;
    fmt->i_codec                = i_codec;
    fmt->i_id                   = -1;
    fmt->i_group                = 0;
    fmt->i_priority             = 0;
    fmt->psz_language           = NULL;
    fmt->psz_description        = NULL;

    memset( &fmt->audio, 0, sizeof(audio_format_t) );
    memset( &fmt->video, 0, sizeof(video_format_t) );
    memset( &fmt->subs, 0, sizeof(subs_format_t) );

    fmt->b_packetized           = VLC_TRUE;
    fmt->i_bitrate              = 0;
    fmt->i_extra                = 0;
    fmt->p_extra                = NULL;
}

static inline void es_format_Copy( es_format_t *dst, es_format_t *src )
{
    memcpy( dst, src, sizeof( es_format_t ) );
    if( src->psz_language )
         dst->psz_language = strdup( src->psz_language );
    if( src->psz_description )
        dst->psz_description = strdup( src->psz_description );
    if( src->i_extra > 0 )
    {
        dst->i_extra = src->i_extra;
        dst->p_extra = malloc( src->i_extra );
        memcpy( dst->p_extra, src->p_extra, src->i_extra );
    }
    else
    {
        dst->i_extra = 0;
        dst->p_extra = NULL;
    }

    if( src->subs.psz_encoding )
        dst->subs.psz_encoding = strdup( src->subs.psz_encoding );

    if( src->video.p_palette )
    {
        dst->video.p_palette =
            (video_palette_t*)malloc( sizeof( video_palette_t ) );
        memcpy( dst->video.p_palette, src->video.p_palette,
                sizeof( video_palette_t ) );
    }
}

static inline void es_format_Clean( es_format_t *fmt )
{
    if( fmt->psz_language ) free( fmt->psz_language );
    fmt->psz_language = NULL;

    if( fmt->psz_description ) free( fmt->psz_description );
    fmt->psz_description = NULL;

    if( fmt->i_extra > 0 ) free( fmt->p_extra );
    fmt->i_extra = 0; fmt->p_extra = NULL;

    if( fmt->video.p_palette ) free( fmt->video.p_palette );
    fmt->video.p_palette = NULL;

    if( fmt->subs.psz_encoding ) free( fmt->subs.psz_encoding );
    fmt->subs.psz_encoding = NULL;
}

#endif
