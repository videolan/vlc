/*****************************************************************************
 * libvlc_media_track.h:  libvlc external API
 *****************************************************************************
 * Copyright (C) 1998-2020 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Paul Saman <jpsaman@videolan.org>
 *          Pierre d'Herbemont <pdherbemont@videolan.org>
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

#ifndef VLC_LIBVLC_MEDIA_TRACK_H
#define VLC_LIBVLC_MEDIA_TRACK_H 1

# ifdef __cplusplus
extern "C" {
# else
#  include <stdbool.h>
# endif

/** \defgroup libvlc_media_track LibVLC media track
 * \ingroup libvlc
 * @ref libvlc_media_track_t is an abstract representation of a media track.
 * @{
 * \file
 * LibVLC media track
 */

typedef enum libvlc_track_type_t
{
    libvlc_track_unknown   = -1,
    libvlc_track_audio     = 0,
    libvlc_track_video     = 1,
    libvlc_track_text      = 2
} libvlc_track_type_t;

typedef struct libvlc_audio_track_t
{
    unsigned    i_channels;
    unsigned    i_rate;
} libvlc_audio_track_t;

typedef enum libvlc_video_orient_t
{
    libvlc_video_orient_top_left,       /**< Normal. Top line represents top, left column left. */
    libvlc_video_orient_top_right,      /**< Flipped horizontally */
    libvlc_video_orient_bottom_left,    /**< Flipped vertically */
    libvlc_video_orient_bottom_right,   /**< Rotated 180 degrees */
    libvlc_video_orient_left_top,       /**< Transposed */
    libvlc_video_orient_left_bottom,    /**< Rotated 90 degrees clockwise (or 270 anti-clockwise) */
    libvlc_video_orient_right_top,      /**< Rotated 90 degrees anti-clockwise */
    libvlc_video_orient_right_bottom    /**< Anti-transposed */
} libvlc_video_orient_t;

typedef enum libvlc_video_projection_t
{
    libvlc_video_projection_rectangular,
    libvlc_video_projection_equirectangular, /**< 360 spherical */

    libvlc_video_projection_cubemap_layout_standard = 0x100,
} libvlc_video_projection_t;

/**
 * Viewpoint
 *
 * \warning allocate using libvlc_video_new_viewpoint()
 */
typedef struct libvlc_video_viewpoint_t
{
    float f_yaw;           /**< view point yaw in degrees  ]-180;180] */
    float f_pitch;         /**< view point pitch in degrees  ]-90;90] */
    float f_roll;          /**< view point roll in degrees ]-180;180] */
    float f_field_of_view; /**< field of view in degrees ]0;180[ (default 80.)*/
} libvlc_video_viewpoint_t;

typedef enum libvlc_video_multiview_t
{
    libvlc_video_multiview_2d,                  /**< No stereoscopy: 2D picture. */
    libvlc_video_multiview_stereo_sbs,          /**< Side-by-side */
    libvlc_video_multiview_stereo_tb,           /**< Top-bottom */
    libvlc_video_multiview_stereo_row,          /**< Row sequential */
    libvlc_video_multiview_stereo_col,          /**< Column sequential */
    libvlc_video_multiview_stereo_frame,        /**< Frame sequential */
    libvlc_video_multiview_stereo_checkerboard, /**< Checkerboard pattern */
} libvlc_video_multiview_t;

typedef struct libvlc_video_track_t
{
    unsigned    i_height;
    unsigned    i_width;
    unsigned    i_sar_num;
    unsigned    i_sar_den;
    unsigned    i_frame_rate_num;
    unsigned    i_frame_rate_den;

    libvlc_video_orient_t       i_orientation;
    libvlc_video_projection_t   i_projection;
    libvlc_video_viewpoint_t    pose; /**< Initial view point */
    libvlc_video_multiview_t    i_multiview;
} libvlc_video_track_t;

typedef struct libvlc_subtitle_track_t
{
    char *psz_encoding;
} libvlc_subtitle_track_t;

typedef struct libvlc_media_track_t
{
    /* Codec fourcc */
    uint32_t    i_codec;
    uint32_t    i_original_fourcc;
    int         i_id;
    libvlc_track_type_t i_type;

    /* Codec specific */
    int         i_profile;
    int         i_level;

    union {
        libvlc_audio_track_t *audio;
        libvlc_video_track_t *video;
        libvlc_subtitle_track_t *subtitle;
    };

    unsigned int i_bitrate;
    char *psz_language;
    char *psz_description;

    /** String identifier of track, can be used to save the track preference
     * from an other LibVLC run, only valid when the track is fetch from a
     * media_player */
    const char *psz_id;
    /** A string identifier is stable when it is certified to be the same
     * across different playback instances for the same track, only valid when
     * the track is fetch from a media_player */
    bool id_stable;
    /** Name of the track, only valid when the track is fetch from a
     * media_player */
    char *psz_name;
    /** true if the track is selected, only valid when the track is fetch from
     * a media_player */
    bool selected;

} libvlc_media_track_t;

/**
 * Opaque struct containing a list of tracks
 */
typedef struct libvlc_media_tracklist_t libvlc_media_tracklist_t;

/**
 * Get the number of tracks in a tracklist
 *
 * \version LibVLC 4.0.0 and later.
 *
 * \param list valid tracklist
 *
 * \return number of tracks, or 0 if the list is empty
 */
LIBVLC_API size_t
libvlc_media_tracklist_count( const libvlc_media_tracklist_t *list );

/**
 * Get a track at a specific index
 *
 * \warning The behaviour is undefined if the index is not valid.
 *
 * \version LibVLC 4.0.0 and later.
 *
 * \param list valid tracklist
 * \param index valid index in the range [0; count[
 *
 * \return a valid track (can't be NULL if libvlc_media_tracklist_count()
 * returned a valid count)
 */
LIBVLC_API libvlc_media_track_t *
libvlc_media_tracklist_at( libvlc_media_tracklist_t *list, size_t index );

/**
 * Release a tracklist
 *
 * \version LibVLC 4.0.0 and later.
 *
 * \see libvlc_media_get_tracklist
 * \see libvlc_media_player_get_tracklist
 *
 * \param list valid tracklist
 */
LIBVLC_API void
libvlc_media_tracklist_delete( libvlc_media_tracklist_t *list );


/**
 * Hold a single track reference
 *
 * \version LibVLC 4.0.0 and later.
 *
 * This function can be used to hold a track from a tracklist. In that case,
 * the track can outlive its tracklist.
 *
 * \param track valid track
 * \return the same track, need to be released with libvlc_media_track_release()
 */
LIBVLC_API libvlc_media_track_t *
libvlc_media_track_hold( libvlc_media_track_t * );

/**
 * Release a single track
 *
 * \version LibVLC 4.0.0 and later.
 *
 * \warning Tracks from a tracklist are released alongside the list with
 * libvlc_media_tracklist_delete().
 *
 * \note You only need to release tracks previously held with
 * libvlc_media_track_hold() or returned by
 * libvlc_media_player_get_selected_track() and
 * libvlc_media_player_get_track_from_id()
 *
 * \param track valid track
 */
LIBVLC_API void
libvlc_media_track_release( libvlc_media_track_t *track );
/** @}*/

# ifdef __cplusplus
}
# endif

#endif /* VLC_LIBVLC_MEDIA_TRACK_H */
