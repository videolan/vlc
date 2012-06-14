/*****************************************************************************
 * libvlc_media.h:  libvlc external API
 *****************************************************************************
 * Copyright (C) 1998-2009 VLC authors and VideoLAN
 * $Id$
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

/**
 * \file
 * This file defines libvlc_media external API
 */

#ifndef VLC_LIBVLC_MEDIA_H
#define VLC_LIBVLC_MEDIA_H 1

# ifdef __cplusplus
extern "C" {
# endif

/** \defgroup libvlc_media LibVLC media
 * \ingroup libvlc
 * @ref libvlc_media_t is an abstract representation of a playable media.
 * It consists of a media location and various optional meta data.
 * @{
 */

typedef struct libvlc_media_t libvlc_media_t;

/** defgroup libvlc_meta LibVLC meta data
 * \ingroup libvlc_media
 * @{
 */

/** Meta data types */
typedef enum libvlc_meta_t {
    libvlc_meta_Title,
    libvlc_meta_Artist,
    libvlc_meta_Genre,
    libvlc_meta_Copyright,
    libvlc_meta_Album,
    libvlc_meta_TrackNumber,
    libvlc_meta_Description,
    libvlc_meta_Rating,
    libvlc_meta_Date,
    libvlc_meta_Setting,
    libvlc_meta_URL,
    libvlc_meta_Language,
    libvlc_meta_NowPlaying,
    libvlc_meta_Publisher,
    libvlc_meta_EncodedBy,
    libvlc_meta_ArtworkURL,
    libvlc_meta_TrackID
    /* Add new meta types HERE */
} libvlc_meta_t;

/** @}*/

/**
 * Note the order of libvlc_state_t enum must match exactly the order of
 * \see mediacontrol_PlayerStatus, \see input_state_e enums,
 * and VideoLAN.LibVLC.State (at bindings/cil/src/media.cs).
 *
 * Expected states by web plugins are:
 * IDLE/CLOSE=0, OPENING=1, BUFFERING=2, PLAYING=3, PAUSED=4,
 * STOPPING=5, ENDED=6, ERROR=7
 */
typedef enum libvlc_state_t
{
    libvlc_NothingSpecial=0,
    libvlc_Opening,
    libvlc_Buffering,
    libvlc_Playing,
    libvlc_Paused,
    libvlc_Stopped,
    libvlc_Ended,
    libvlc_Error
} libvlc_state_t;

enum
{
    libvlc_media_option_trusted = 0x2,
    libvlc_media_option_unique = 0x100
};

typedef enum libvlc_track_type_t
{
    libvlc_track_unknown   = -1,
    libvlc_track_audio     = 0,
    libvlc_track_video     = 1,
    libvlc_track_text      = 2
} libvlc_track_type_t;

/** defgroup libvlc_media_stats_t LibVLC media statistics
 * \ingroup libvlc_media
 * @{
 */
typedef struct libvlc_media_stats_t
{
    /* Input */
    int         i_read_bytes;
    float       f_input_bitrate;

    /* Demux */
    int         i_demux_read_bytes;
    float       f_demux_bitrate;
    int         i_demux_corrupted;
    int         i_demux_discontinuity;

    /* Decoders */
    int         i_decoded_video;
    int         i_decoded_audio;

    /* Video Output */
    int         i_displayed_pictures;
    int         i_lost_pictures;

    /* Audio output */
    int         i_played_abuffers;
    int         i_lost_abuffers;

    /* Stream output */
    int         i_sent_packets;
    int         i_sent_bytes;
    float       f_send_bitrate;
} libvlc_media_stats_t;
/** @}*/

typedef struct libvlc_media_track_info_t
{
    /* Codec fourcc */
    uint32_t    i_codec;
    int         i_id;
    libvlc_track_type_t i_type;

    /* Codec specific */
    int         i_profile;
    int         i_level;

    union {
        struct {
            /* Audio specific */
            unsigned    i_channels;
            unsigned    i_rate;
        } audio;
        struct {
            /* Video specific */
            unsigned    i_height;
            unsigned    i_width;
        } video;
    } u;

} libvlc_media_track_info_t;


/**
 * Create a media with a certain given media resource location,
 * for instance a valid URL.
 *
 * \note To refer to a local file with this function,
 * the file://... URI syntax <b>must</b> be used (see IETF RFC3986).
 * We recommend using libvlc_media_new_path() instead when dealing with
 * local files.
 *
 * \see libvlc_media_release
 *
 * \param p_instance the instance
 * \param psz_mrl the media location
 * \return the newly created media or NULL on error
 */
LIBVLC_API libvlc_media_t *libvlc_media_new_location(
                                   libvlc_instance_t *p_instance,
                                   const char * psz_mrl );

/**
 * Create a media for a certain file path.
 *
 * \see libvlc_media_release
 *
 * \param p_instance the instance
 * \param path local filesystem path
 * \return the newly created media or NULL on error
 */
LIBVLC_API libvlc_media_t *libvlc_media_new_path(
                                   libvlc_instance_t *p_instance,
                                   const char *path );

/**
 * Create a media for an already open file descriptor.
 * The file descriptor shall be open for reading (or reading and writing).
 *
 * Regular file descriptors, pipe read descriptors and character device
 * descriptors (including TTYs) are supported on all platforms.
 * Block device descriptors are supported where available.
 * Directory descriptors are supported on systems that provide fdopendir().
 * Sockets are supported on all platforms where they are file descriptors,
 * i.e. all except Windows.
 *
 * \note This library will <b>not</b> automatically close the file descriptor
 * under any circumstance. Nevertheless, a file descriptor can usually only be
 * rendered once in a media player. To render it a second time, the file
 * descriptor should probably be rewound to the beginning with lseek().
 *
 * \see libvlc_media_release
 *
 * \version LibVLC 1.1.5 and later.
 *
 * \param p_instance the instance
 * \param fd open file descriptor
 * \return the newly created media or NULL on error
 */
LIBVLC_API libvlc_media_t *libvlc_media_new_fd(
                                   libvlc_instance_t *p_instance,
                                   int fd );


/**
 * Create a media as an empty node with a given name.
 *
 * \see libvlc_media_release
 *
 * \param p_instance the instance
 * \param psz_name the name of the node
 * \return the new empty media or NULL on error
 */
LIBVLC_API libvlc_media_t *libvlc_media_new_as_node(
                                   libvlc_instance_t *p_instance,
                                   const char * psz_name );

/**
 * Add an option to the media.
 *
 * This option will be used to determine how the media_player will
 * read the media. This allows to use VLC's advanced
 * reading/streaming options on a per-media basis.
 *
 * The options are detailed in vlc --long-help, for instance
 * "--sout-all". Note that all options are not usable on medias:
 * specifically, due to architectural issues, video-related options
 * such as text renderer options cannot be set on a single media. They
 * must be set on the whole libvlc instance instead.
 *
 * \param p_md the media descriptor
 * \param ppsz_options the options (as a string)
 */
LIBVLC_API void libvlc_media_add_option(
                                   libvlc_media_t *p_md,
                                   const char * ppsz_options );

/**
 * Add an option to the media with configurable flags.
 *
 * This option will be used to determine how the media_player will
 * read the media. This allows to use VLC's advanced
 * reading/streaming options on a per-media basis.
 *
 * The options are detailed in vlc --long-help, for instance
 * "--sout-all". Note that all options are not usable on medias:
 * specifically, due to architectural issues, video-related options
 * such as text renderer options cannot be set on a single media. They
 * must be set on the whole libvlc instance instead.
 *
 * \param p_md the media descriptor
 * \param ppsz_options the options (as a string)
 * \param i_flags the flags for this option
 */
LIBVLC_API void libvlc_media_add_option_flag(
                                   libvlc_media_t *p_md,
                                   const char * ppsz_options,
                                   unsigned i_flags );


/**
 * Retain a reference to a media descriptor object (libvlc_media_t). Use
 * libvlc_media_release() to decrement the reference count of a
 * media descriptor object.
 *
 * \param p_md the media descriptor
 */
LIBVLC_API void libvlc_media_retain( libvlc_media_t *p_md );

/**
 * Decrement the reference count of a media descriptor object. If the
 * reference count is 0, then libvlc_media_release() will release the
 * media descriptor object. It will send out an libvlc_MediaFreed event
 * to all listeners. If the media descriptor object has been released it
 * should not be used again.
 *
 * \param p_md the media descriptor
 */
LIBVLC_API void libvlc_media_release( libvlc_media_t *p_md );


/**
 * Get the media resource locator (mrl) from a media descriptor object
 *
 * \param p_md a media descriptor object
 * \return string with mrl of media descriptor object
 */
LIBVLC_API char *libvlc_media_get_mrl( libvlc_media_t *p_md );

/**
 * Duplicate a media descriptor object.
 *
 * \param p_md a media descriptor object.
 */
LIBVLC_API libvlc_media_t *libvlc_media_duplicate( libvlc_media_t *p_md );

/**
 * Read the meta of the media.
 *
 * If the media has not yet been parsed this will return NULL.
 *
 * This methods automatically calls libvlc_media_parse_async(), so after calling
 * it you may receive a libvlc_MediaMetaChanged event. If you prefer a synchronous
 * version ensure that you call libvlc_media_parse() before get_meta().
 *
 * \see libvlc_media_parse
 * \see libvlc_media_parse_async
 * \see libvlc_MediaMetaChanged
 *
 * \param p_md the media descriptor
 * \param e_meta the meta to read
 * \return the media's meta
 */
LIBVLC_API char *libvlc_media_get_meta( libvlc_media_t *p_md,
                                             libvlc_meta_t e_meta );

/**
 * Set the meta of the media (this function will not save the meta, call
 * libvlc_media_save_meta in order to save the meta)
 *
 * \param p_md the media descriptor
 * \param e_meta the meta to write
 * \param psz_value the media's meta
 */
LIBVLC_API void libvlc_media_set_meta( libvlc_media_t *p_md,
                                           libvlc_meta_t e_meta,
                                           const char *psz_value );


/**
 * Save the meta previously set
 *
 * \param p_md the media desriptor
 * \return true if the write operation was successful
 */
LIBVLC_API int libvlc_media_save_meta( libvlc_media_t *p_md );


/**
 * Get current state of media descriptor object. Possible media states
 * are defined in libvlc_structures.c ( libvlc_NothingSpecial=0,
 * libvlc_Opening, libvlc_Buffering, libvlc_Playing, libvlc_Paused,
 * libvlc_Stopped, libvlc_Ended,
 * libvlc_Error).
 *
 * \see libvlc_state_t
 * \param p_md a media descriptor object
 * \return state of media descriptor object
 */
LIBVLC_API libvlc_state_t libvlc_media_get_state(
                                   libvlc_media_t *p_md );


/**
 * Get the current statistics about the media
 * \param p_md: media descriptor object
 * \param p_stats: structure that contain the statistics about the media
 *                 (this structure must be allocated by the caller)
 * \return true if the statistics are available, false otherwise
 *
 * \libvlc_return_bool
 */
LIBVLC_API int libvlc_media_get_stats( libvlc_media_t *p_md,
                                           libvlc_media_stats_t *p_stats );

/**
 * Get subitems of media descriptor object. This will increment
 * the reference count of supplied media descriptor object. Use
 * libvlc_media_list_release() to decrement the reference counting.
 *
 * \param p_md media descriptor object
 * \return list of media descriptor subitems or NULL
 */

/* This method uses libvlc_media_list_t, however, media_list usage is optionnal
 * and this is here for convenience */
#define VLC_FORWARD_DECLARE_OBJECT(a) struct a

LIBVLC_API VLC_FORWARD_DECLARE_OBJECT(libvlc_media_list_t *)
libvlc_media_subitems( libvlc_media_t *p_md );

/**
 * Get event manager from media descriptor object.
 * NOTE: this function doesn't increment reference counting.
 *
 * \param p_md a media descriptor object
 * \return event manager object
 */
LIBVLC_API libvlc_event_manager_t *
    libvlc_media_event_manager( libvlc_media_t *p_md );

/**
 * Get duration (in ms) of media descriptor object item.
 *
 * \param p_md media descriptor object
 * \return duration of media item or -1 on error
 */
LIBVLC_API libvlc_time_t
   libvlc_media_get_duration( libvlc_media_t *p_md );

/**
 * Parse a media.
 *
 * This fetches (local) meta data and tracks information.
 * The method is synchronous.
 *
 * \see libvlc_media_parse_async
 * \see libvlc_media_get_meta
 * \see libvlc_media_get_tracks_info
 *
 * \param p_md media descriptor object
 */
LIBVLC_API void
libvlc_media_parse( libvlc_media_t *p_md );

/**
 * Parse a media.
 *
 * This fetches (local) meta data and tracks information.
 * The method is the asynchronous of libvlc_media_parse().
 *
 * To track when this is over you can listen to libvlc_MediaParsedChanged
 * event. However if the media was already parsed you will not receive this
 * event.
 *
 * \see libvlc_media_parse
 * \see libvlc_MediaParsedChanged
 * \see libvlc_media_get_meta
 * \see libvlc_media_get_tracks_info
 *
 * \param p_md media descriptor object
 */
LIBVLC_API void
libvlc_media_parse_async( libvlc_media_t *p_md );

/**
 * Get Parsed status for media descriptor object.
 *
 * \see libvlc_MediaParsedChanged
 *
 * \param p_md media descriptor object
 * \return true if media object has been parsed otherwise it returns false
 *
 * \libvlc_return_bool
 */
LIBVLC_API int
   libvlc_media_is_parsed( libvlc_media_t *p_md );

/**
 * Sets media descriptor's user_data. user_data is specialized data
 * accessed by the host application, VLC.framework uses it as a pointer to
 * an native object that references a libvlc_media_t pointer
 *
 * \param p_md media descriptor object
 * \param p_new_user_data pointer to user data
 */
LIBVLC_API void
    libvlc_media_set_user_data( libvlc_media_t *p_md, void *p_new_user_data );

/**
 * Get media descriptor's user_data. user_data is specialized data
 * accessed by the host application, VLC.framework uses it as a pointer to
 * an native object that references a libvlc_media_t pointer
 *
 * \param p_md media descriptor object
 */
LIBVLC_API void *libvlc_media_get_user_data( libvlc_media_t *p_md );

/**
 * Get media descriptor's elementary streams description
 *
 * Note, you need to call libvlc_media_parse() or play the media at least once
 * before calling this function.
 * Not doing this will result in an empty array.
 *
 * \param p_md media descriptor object
 * \param tracks address to store an allocated array of Elementary Streams
 * descriptions (must be freed by the caller)
 *
 * \return the number of Elementary Streams
 */
LIBVLC_API
int libvlc_media_get_tracks_info( libvlc_media_t *p_md,
                                  libvlc_media_track_info_t **tracks );

/** @}*/

# ifdef __cplusplus
}
# endif

#endif /* VLC_LIBVLC_MEDIA_H */
