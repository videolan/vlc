/*****************************************************************************
 * deprecated.h:  libvlc deprecated API
 *****************************************************************************
 * Copyright (C) 1998-2008 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Paul Saman <jpsaman@videolan.org>
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

#ifndef LIBVLC_DEPRECATED_H
#define LIBVLC_DEPRECATED_H 1

# ifdef __cplusplus
extern "C" {
# endif

/**
 * \ingroup libvlc_media_player
 * @{
 */

/**
 * Description for video, audio tracks and subtitles. It contains
 * id, name (description string) and pointer to next record.
 */
typedef struct libvlc_track_description_t
{
    int   i_id;
    char *psz_name;
    struct libvlc_track_description_t *p_next;

} libvlc_track_description_t;

/**
 * Release (free) libvlc_track_description_t
 *
 * \param p_track_description the structure to release
 */
LIBVLC_DEPRECATED LIBVLC_API void libvlc_track_description_list_release( libvlc_track_description_t *p_track_description );


/** @}*/

/**
 * \ingroup libvlc_video
 * @{
 */

/**
 * Get number of available video tracks.
 *
 * \param p_mi media player
 * \return the number of available video tracks (int)
 */
LIBVLC_DEPRECATED LIBVLC_API int libvlc_video_get_track_count( libvlc_media_player_t *p_mi );

/**
 * Get the description of available video tracks.
 *
 * \param p_mi media player
 * \return list with description of available video tracks, or NULL on error.
 * It must be freed with libvlc_track_description_list_release()
 */
LIBVLC_DEPRECATED LIBVLC_API libvlc_track_description_t *
        libvlc_video_get_track_description( libvlc_media_player_t *p_mi );

/**
 * Get current video track.
 *
 * \param p_mi media player
 * \return the video track ID (int) or -1 if no active input
 */
LIBVLC_DEPRECATED LIBVLC_API int libvlc_video_get_track( libvlc_media_player_t *p_mi );

/**
 * Set video track.
 *
 * \param p_mi media player
 * \param i_track the track ID (i_id field from track description)
 * \return 0 on success, -1 if out of range
 */
LIBVLC_DEPRECATED LIBVLC_API
int libvlc_video_set_track( libvlc_media_player_t *p_mi, int i_track );

/**
 * Get current video subtitle.
 *
 * \param p_mi the media player
 * \return the video subtitle selected, or -1 if none
 */
LIBVLC_DEPRECATED LIBVLC_API int libvlc_video_get_spu( libvlc_media_player_t *p_mi );

/**
 * Get the number of available video subtitles.
 *
 * \param p_mi the media player
 * \return the number of available video subtitles
 */
LIBVLC_DEPRECATED LIBVLC_API int libvlc_video_get_spu_count( libvlc_media_player_t *p_mi );

/**
 * Get the description of available video subtitles.
 *
 * \param p_mi the media player
 * \return list containing description of available video subtitles.
 * It must be freed with libvlc_track_description_list_release()
 */
LIBVLC_DEPRECATED LIBVLC_API libvlc_track_description_t *
        libvlc_video_get_spu_description( libvlc_media_player_t *p_mi );

/**
 * Set new video subtitle.
 *
 * \param p_mi the media player
 * \param i_spu video subtitle track to select (i_id from track description)
 * \return 0 on success, -1 if out of range
 */
LIBVLC_DEPRECATED LIBVLC_API int libvlc_video_set_spu( libvlc_media_player_t *p_mi, int i_spu );

/** @}*/

/**
 * \ingroup libvlc_audio
 * @{
 */

/**
 * Get number of available audio tracks.
 *
 * \param p_mi media player
 * \return the number of available audio tracks (int), or -1 if unavailable
 */
LIBVLC_DEPRECATED LIBVLC_API int libvlc_audio_get_track_count( libvlc_media_player_t *p_mi );

/**
 * Get the description of available audio tracks.
 *
 * \param p_mi media player
 * \return list with description of available audio tracks, or NULL.
 * It must be freed with libvlc_track_description_list_release()
 */
LIBVLC_DEPRECATED LIBVLC_API libvlc_track_description_t *
        libvlc_audio_get_track_description( libvlc_media_player_t *p_mi );

/**
 * Get current audio track.
 *
 * \param p_mi media player
 * \return the audio track ID or -1 if no active input.
 */
LIBVLC_DEPRECATED LIBVLC_API int libvlc_audio_get_track( libvlc_media_player_t *p_mi );

/**
 * Set current audio track.
 *
 * \param p_mi media player
 * \param i_track the track ID (i_id field from track description)
 * \return 0 on success, -1 on error
 */
LIBVLC_DEPRECATED LIBVLC_API int libvlc_audio_set_track( libvlc_media_player_t *p_mi, int i_track );

/** @}*/

/**
 * \ingroup libvlc
 * \defgroup libvlc_playlist LibVLC playlist (legacy)
 * @deprecated Use @ref libvlc_media_list instead.
 * @{
 * \file
 * LibVLC deprecated playlist API
 */

/**
 * Start playing (if there is any item in the playlist).
 *
 * Additional playlist item options can be specified for addition to the
 * item before it is played.
 *
 * \param p_instance the playlist instance
 */
LIBVLC_DEPRECATED LIBVLC_API
void libvlc_playlist_play( libvlc_instance_t *p_instance );

/** @}*/

# ifdef __cplusplus
}
# endif

#endif /* _LIBVLC_DEPRECATED_H */
