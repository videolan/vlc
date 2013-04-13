/*****************************************************************************
 * vlc_es.h: Elementary stream formats descriptions
 *****************************************************************************
 * Copyright (C) 1999-2012 VLC authors and VideoLAN
 * $Id$
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

#ifndef VLC_ES_H
#define VLC_ES_H 1

#include <vlc_fourcc.h>

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
 * audio replay gain description
 */
#define AUDIO_REPLAY_GAIN_MAX (2)
#define AUDIO_REPLAY_GAIN_TRACK (0)
#define AUDIO_REPLAY_GAIN_ALBUM (1)
typedef struct
{
    /* true if we have the peak value */
    bool pb_peak[AUDIO_REPLAY_GAIN_MAX];
    /* peak value where 1.0 means full sample value */
    float      pf_peak[AUDIO_REPLAY_GAIN_MAX];

    /* true if we have the gain value */
    bool pb_gain[AUDIO_REPLAY_GAIN_MAX];
    /* gain value in dB */
    float      pf_gain[AUDIO_REPLAY_GAIN_MAX];
} audio_replay_gain_t;

/**
 * audio format description
 */
struct audio_format_t
{
    vlc_fourcc_t i_format;                          /**< audio format fourcc */
    unsigned int i_rate;                              /**< audio sample-rate */

    /* Describes the channels configuration of the samples (ie. number of
     * channels which are available in the buffer, and positions). */
    uint16_t     i_physical_channels;

    /* Describes from which original channels, before downmixing, the
     * buffer is derived. */
    uint32_t     i_original_channels;

    /* Optional - for A/52, SPDIF and DTS types : */
    /* Bytes used by one compressed frame, depends on bitrate. */
    unsigned int i_bytes_per_frame;

    /* Number of sampleframes contained in one compressed frame. */
    unsigned int i_frame_length;
    /* Please note that it may be completely arbitrary - buffers are not
     * obliged to contain a integral number of so-called "frames". It's
     * just here for the division :
     * buffer_size = i_nb_samples * i_bytes_per_frame / i_frame_length
     */

    /* FIXME ? (used by the codecs) */
    unsigned     i_bitspersample;
    unsigned     i_blockalign;
    uint8_t      i_channels; /* must be <=32 */
};

/* Values available for audio channels */
#define AOUT_CHAN_CENTER            0x1
#define AOUT_CHAN_LEFT              0x2
#define AOUT_CHAN_RIGHT             0x4
#define AOUT_CHAN_REARCENTER        0x10
#define AOUT_CHAN_REARLEFT          0x20
#define AOUT_CHAN_REARRIGHT         0x40
#define AOUT_CHAN_MIDDLELEFT        0x100
#define AOUT_CHAN_MIDDLERIGHT       0x200
#define AOUT_CHAN_LFE               0x1000

#define AOUT_CHANS_FRONT  (AOUT_CHAN_LEFT       | AOUT_CHAN_RIGHT)
#define AOUT_CHANS_MIDDLE (AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT)
#define AOUT_CHANS_REAR   (AOUT_CHAN_REARLEFT   | AOUT_CHAN_REARRIGHT)
#define AOUT_CHANS_CENTER (AOUT_CHAN_CENTER     | AOUT_CHAN_REARCENTER)

#define AOUT_CHANS_STEREO AOUT_CHANS_2_0
#define AOUT_CHANS_2_0    (AOUT_CHANS_FRONT)
#define AOUT_CHANS_2_1    (AOUT_CHANS_FRONT | AOUT_CHAN_LFE)
#define AOUT_CHANS_3_0    (AOUT_CHANS_FRONT | AOUT_CHAN_CENTER)
#define AOUT_CHANS_3_1    (AOUT_CHANS_3_0   | AOUT_CHAN_LFE)
#define AOUT_CHANS_4_0    (AOUT_CHANS_FRONT | AOUT_CHANS_REAR)
#define AOUT_CHANS_4_1    (AOUT_CHANS_4_0   | AOUT_CHAN_LFE)
#define AOUT_CHANS_5_0    (AOUT_CHANS_4_0   | AOUT_CHAN_CENTER)
#define AOUT_CHANS_5_1    (AOUT_CHANS_5_0   | AOUT_CHAN_LFE)
#define AOUT_CHANS_6_0    (AOUT_CHANS_4_0   | AOUT_CHANS_MIDDLE)
#define AOUT_CHANS_7_0    (AOUT_CHANS_6_0   | AOUT_CHAN_CENTER)
#define AOUT_CHANS_7_1    (AOUT_CHANS_5_1   | AOUT_CHANS_MIDDLE)
#define AOUT_CHANS_8_1    (AOUT_CHANS_7_1   | AOUT_CHAN_REARCENTER)

#define AOUT_CHANS_4_0_MIDDLE (AOUT_CHANS_FRONT | AOUT_CHANS_MIDDLE)
#define AOUT_CHANS_4_CENTER_REAR (AOUT_CHANS_FRONT | AOUT_CHANS_CENTER)
#define AOUT_CHANS_5_0_MIDDLE (AOUT_CHANS_4_0_MIDDLE | AOUT_CHAN_CENTER)
#define AOUT_CHANS_6_1_MIDDLE (AOUT_CHANS_5_0_MIDDLE | AOUT_CHAN_REARCENTER | AOUT_CHAN_LFE)

/* Values available for original channels only */
#define AOUT_CHAN_DOLBYSTEREO       0x10000
#define AOUT_CHAN_DUALMONO          0x20000
#define AOUT_CHAN_REVERSESTEREO     0x40000

#define AOUT_CHAN_PHYSMASK          0xFFFF
#define AOUT_CHAN_MAX               9

/**
 * Picture orientation.
 */
typedef enum video_orientation_t
{
    ORIENT_TOP_LEFT = 0, /**< Top line represents top, left column left. */
    ORIENT_TOP_RIGHT, /**< Flipped horizontally */
    ORIENT_BOTTOM_LEFT, /**< Flipped vertically */
    ORIENT_BOTTOM_RIGHT, /**< Rotated 180 degrees */
    ORIENT_LEFT_TOP, /**< Transposed */
    ORIENT_LEFT_BOTTOM, /**< Rotated 90 degrees clockwise */
    ORIENT_RIGHT_TOP, /**< Rotated 90 degrees anti-clockwise */
    ORIENT_RIGHT_BOTTOM, /**< Anti-transposed */

    ORIENT_NORMAL      = ORIENT_TOP_LEFT,
    ORIENT_HFLIPPED    = ORIENT_TOP_RIGHT,
    ORIENT_VFLIPPED    = ORIENT_BOTTOM_LEFT,
    ORIENT_ROTATED_180 = ORIENT_BOTTOM_RIGHT,
    ORIENT_ROTATED_270 = ORIENT_LEFT_BOTTOM,
    ORIENT_ROTATED_90  = ORIENT_RIGHT_TOP,
} video_orientation_t;
/** Convert EXIF orientation to enum video_orientation_t */
#define ORIENT_FROM_EXIF(exif) ((0x01324675U >> (4 * ((exif) - 1))) & 7)
/** Convert enum video_orientation_t to EXIF */
#define ORIENT_TO_EXIF(orient) ((0x12435867U >> (4 * (orient))) & 15)
/** If the orientation is natural or mirrored */
#define ORIENT_IS_MIRROR(orient) parity(orient)
/** If the orientation swaps dimensions */
#define ORIENT_IS_SWAP(orient) (((orient) & 4) != 0)
/** Applies horizontal flip to an orientation */
#define ORIENT_HFLIP(orient) ((orient) ^ 1)
/** Applies vertical flip to an orientation */
#define ORIENT_VFLIP(orient) ((orient) ^ 2)
/** Applies horizontal flip to an orientation */
#define ORIENT_ROTATE_180(orient) ((orient) ^ 3)

/**
 * video format description
 */
struct video_format_t
{
    vlc_fourcc_t i_chroma;                               /**< picture chroma */

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

    uint32_t i_rmask, i_gmask, i_bmask;          /**< color masks for RGB chroma */
    int i_rrshift, i_lrshift;
    int i_rgshift, i_lgshift;
    int i_rbshift, i_lbshift;
    video_palette_t *p_palette;              /**< video palette from demuxer */
    video_orientation_t orientation;                /**< picture orientation */
};

/**
 * Initialize a video_format_t structure with chroma 'i_chroma'
 * \param p_src pointer to video_format_t structure
 * \param i_chroma chroma value to use
 */
static inline void video_format_Init( video_format_t *p_src, vlc_fourcc_t i_chroma )
{
    memset( p_src, 0, sizeof( video_format_t ) );
    p_src->i_chroma = i_chroma;
    p_src->i_sar_num = p_src->i_sar_den = 1;
    p_src->p_palette = NULL;
}

/**
 * Copy video_format_t including the palette
 * \param p_dst video_format_t to copy to
 * \param p_src video_format_t to copy from
 */
static inline int video_format_Copy( video_format_t *p_dst, const video_format_t *p_src )
{
    memcpy( p_dst, p_src, sizeof( *p_dst ) );
    if( p_src->p_palette )
    {
        p_dst->p_palette = (video_palette_t *) malloc( sizeof( video_palette_t ) );
        if( !p_dst->p_palette )
            return VLC_ENOMEM;
        memcpy( p_dst->p_palette, p_src->p_palette, sizeof( *p_dst->p_palette ) );
    }
    return VLC_SUCCESS;
}

/**
 * Cleanup and free palette of this video_format_t
 * \param p_src video_format_t structure to clean
 */
static inline void video_format_Clean( video_format_t *p_src )
{
    free( p_src->p_palette );
    memset( p_src, 0, sizeof( video_format_t ) );
    p_src->p_palette = NULL;
}

/**
 * It will fill up a video_format_t using the given arguments.
 * Note that the video_format_t must already be initialized.
 */
VLC_API void video_format_Setup( video_format_t *, vlc_fourcc_t i_chroma, int i_width, int i_height, int i_sar_num, int i_sar_den );

/**
 * It will copy the crop properties from a video_format_t to another.
 */
VLC_API void video_format_CopyCrop( video_format_t *, const video_format_t * );

/**
 * It will compute the crop/ar properties when scaling.
 */
VLC_API void video_format_ScaleCropAr( video_format_t *, const video_format_t * );

/**
 * This function will check if the first video format is similar
 * to the second one.
 */
VLC_API bool video_format_IsSimilar( const video_format_t *, const video_format_t * );

/**
 * It prints details about the given video_format_t
 */
VLC_API void video_format_Print( vlc_object_t *, const char *, const video_format_t * );

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
        int i_original_frame_width;
        /* the height of the original movie the spu was extracted from */
        int i_original_frame_height;
    } spu;

    struct
    {
        int i_id;
    } dvb;
    struct
    {
        int i_magazine;
        int i_page;
    } teletext;
};

/**
 * ES language definition
 */
typedef struct extra_languages_t
{
        char *psz_language;
        char *psz_description;
} extra_languages_t;

/**
 * ES format definition
 */
struct es_format_t
{
    int             i_cat;              /**< ES category @see es_format_category_e */
    vlc_fourcc_t    i_codec;            /**< FOURCC value as used in vlc */
    vlc_fourcc_t    i_original_fourcc;  /**< original FOURCC from the container */

    int             i_id;       /**< es identifier, where means
                                    -1: let the core mark the right id
                                    >=0: valid id */
    int             i_group;    /**< group identifier, where means:
                                    -1 : standalone
                                    >= 0 then a "group" (program) is created
                                        for each value */
    int             i_priority; /**< priority, where means:
                                    -2 : mean not selectable by the users
                                    -1 : mean not selected by default even
                                         when no other stream
                                    >=0: priority */

    char            *psz_language;        /**< human readible language name */
    char            *psz_description;     /**< human readible description of language */
    int             i_extra_languages;    /**< length in bytes of extra language data pointer */
    extra_languages_t *p_extra_languages; /**< extra language data needed by some decoders */

    audio_format_t  audio;    /**< description of audio format */
    audio_replay_gain_t audio_replay_gain; /*< audio replay gain information */
    video_format_t video;     /**< description of video format */
    subs_format_t  subs;      /**< description of subtitle format */

    unsigned int   i_bitrate; /**< bitrate of this ES */
    int      i_profile;       /**< codec specific information (like real audio flavor, mpeg audio layer, h264 profile ...) */
    int      i_level;         /**< codec specific information: indicates maximum restrictions on the stream (resolution, bitrate, codec features ...) */

    bool     b_packetized;  /**< whether the data is packetized (ie. not truncated) */
    int     i_extra;        /**< length in bytes of extra data pointer */
    void    *p_extra;       /**< extra data needed by some decoders or muxers */

};

/** ES Categories */
enum es_format_category_e
{
    UNKNOWN_ES = 0x00,
    VIDEO_ES   = 0x01,
    AUDIO_ES   = 0x02,
    SPU_ES     = 0x03,
    NAV_ES     = 0x04,
};

/**
 * This function will fill all RGB shift from RGB masks.
 */
VLC_API void video_format_FixRgb( video_format_t * );

/**
 * This function will initialize a es_format_t structure.
 */
VLC_API void es_format_Init( es_format_t *, int i_cat, vlc_fourcc_t i_codec );

/**
 * This function will initialize a es_format_t structure from a video_format_t.
 */
VLC_API void es_format_InitFromVideo( es_format_t *, const video_format_t * );

/**
 * This functions will copy a es_format_t.
 */
VLC_API int es_format_Copy( es_format_t *p_dst, const es_format_t *p_src );

/**
 * This function will clean up a es_format_t and release all associated
 * resources.
 * You can call it multiple times on the same structure.
 */
VLC_API void es_format_Clean( es_format_t *fmt );

/**
 * This function will check if the first ES format is similar
 * to the second one.
 *
 * All descriptive fields are ignored.
 */
VLC_API bool es_format_IsSimilar( const es_format_t *, const es_format_t * );

#endif
