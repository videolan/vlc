/*****************************************************************************
 * libvlc_media.h:  libvlc external API
 *****************************************************************************
 * Copyright (C) 1998-2009 VLC authors and VideoLAN
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

#ifndef VLC_LIBVLC_MEDIA_H
#define VLC_LIBVLC_MEDIA_H 1

#include <vlc/libvlc_media_track.h>

# ifdef __cplusplus
extern "C" {
# else
#  include <stdbool.h>
# endif

/** \defgroup libvlc_media LibVLC media
 * \ingroup libvlc
 * @ref libvlc_media_t is an abstract representation of a playable media.
 * It consists of a media location and various optional meta data.
 * @{
 * \file
 * LibVLC media item/descriptor external API
 */

typedef struct libvlc_media_t libvlc_media_t;

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
    libvlc_meta_TrackID,
    libvlc_meta_TrackTotal,
    libvlc_meta_Director,
    libvlc_meta_Season,
    libvlc_meta_Episode,
    libvlc_meta_ShowName,
    libvlc_meta_Actors,
    libvlc_meta_AlbumArtist,
    libvlc_meta_DiscNumber,
    libvlc_meta_DiscTotal
    /* Add new meta types HERE */
} libvlc_meta_t;

/**
 * Note the order of libvlc_state_t enum must match exactly the order of
 * \see mediacontrol_PlayerStatus, \see input_state_e enums,
 * and VideoLAN.LibVLC.State (at bindings/cil/src/media.cs).
 *
 * Expected states by web plugins are:
 * IDLE/CLOSE=0, OPENING=1, PLAYING=3, PAUSED=4,
 * STOPPING=5, ENDED=6, ERROR=7
 */
typedef enum libvlc_state_t
{
    libvlc_NothingSpecial=0,
    libvlc_Opening,
    libvlc_Buffering, /* XXX: Deprecated value. Check the
                       * libvlc_MediaPlayerBuffering event to know the
                       * buffering state of a libvlc_media_player */
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
} libvlc_media_stats_t;

/**
 * Media type
 *
 * \see libvlc_media_get_type
 */
typedef enum libvlc_media_type_t {
    libvlc_media_type_unknown,
    libvlc_media_type_file,
    libvlc_media_type_directory,
    libvlc_media_type_disc,
    libvlc_media_type_stream,
    libvlc_media_type_playlist,
} libvlc_media_type_t;

/**
 * Parse flags used by libvlc_media_parse_with_options()
 *
 * \see libvlc_media_parse_with_options
 */
typedef enum libvlc_media_parse_flag_t
{
    /**
     * Parse media if it's a local file
     */
    libvlc_media_parse_local    = 0x00,
    /**
     * Parse media even if it's a network file
     */
    libvlc_media_parse_network  = 0x01,
    /**
     * Fetch meta and covert art using local resources
     */
    libvlc_media_fetch_local    = 0x02,
    /**
     * Fetch meta and covert art using network resources
     */
    libvlc_media_fetch_network  = 0x04,
    /**
     * Interact with the user (via libvlc_dialog_cbs) when preparsing this item
     * (and not its sub items). Set this flag in order to receive a callback
     * when the input is asking for credentials.
     */
    libvlc_media_do_interact    = 0x08,
} libvlc_media_parse_flag_t;

/**
 * Parse status used sent by libvlc_media_parse_with_options() or returned by
 * libvlc_media_get_parsed_status()
 *
 * \see libvlc_media_parse_with_options
 * \see libvlc_media_get_parsed_status
 */
typedef enum libvlc_media_parsed_status_t
{
    libvlc_media_parsed_status_skipped = 1,
    libvlc_media_parsed_status_failed,
    libvlc_media_parsed_status_timeout,
    libvlc_media_parsed_status_done,
} libvlc_media_parsed_status_t;

/**
 * Type of a media slave: subtitle or audio.
 */
typedef enum libvlc_media_slave_type_t
{
    libvlc_media_slave_type_subtitle,
    libvlc_media_slave_type_audio,
} libvlc_media_slave_type_t;

/**
 * A slave of a libvlc_media_t
 * \see libvlc_media_slaves_get
 */
typedef struct libvlc_media_slave_t
{
    char *                          psz_uri;
    libvlc_media_slave_type_t       i_type;
    unsigned int                    i_priority;
} libvlc_media_slave_t;

/**
 * Callback prototype to open a custom bitstream input media.
 *
 * The same media item can be opened multiple times. Each time, this callback
 * is invoked. It should allocate and initialize any instance-specific
 * resources, then store them in *datap. The instance resources can be freed
 * in the @ref libvlc_media_close_cb callback.
 *
 * \param opaque private pointer as passed to libvlc_media_new_callbacks()
 * \param datap storage space for a private data pointer [OUT]
 * \param sizep byte length of the bitstream or UINT64_MAX if unknown [OUT]
 *
 * \note For convenience, *datap is initially NULL and *sizep is initially 0.
 *
 * \return 0 on success, non-zero on error. In case of failure, the other
 * callbacks will not be invoked and any value stored in *datap and *sizep is
 * discarded.
 */
typedef int (*libvlc_media_open_cb)(void *opaque, void **datap,
                                    uint64_t *sizep);

/**
 * Callback prototype to read data from a custom bitstream input media.
 *
 * \param opaque private pointer as set by the @ref libvlc_media_open_cb
 *               callback
 * \param buf start address of the buffer to read data into
 * \param len bytes length of the buffer
 *
 * \return strictly positive number of bytes read, 0 on end-of-stream,
 *         or -1 on non-recoverable error
 *
 * \note If no data is immediately available, then the callback should sleep.
 * \warning The application is responsible for avoiding deadlock situations.
 */
typedef ssize_t (*libvlc_media_read_cb)(void *opaque, unsigned char *buf,
                                        size_t len);

/**
 * Callback prototype to seek a custom bitstream input media.
 *
 * \param opaque private pointer as set by the @ref libvlc_media_open_cb
 *               callback
 * \param offset absolute byte offset to seek to
 * \return 0 on success, -1 on error.
 */
typedef int (*libvlc_media_seek_cb)(void *opaque, uint64_t offset);

/**
 * Callback prototype to close a custom bitstream input media.
 *
 * \param opaque private pointer as set by the @ref libvlc_media_open_cb
 *               callback
 */
typedef void (*libvlc_media_close_cb)(void *opaque);

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
 * Create a media with custom callbacks to read the data from.
 *
 * \param instance LibVLC instance
 * \param open_cb callback to open the custom bitstream input media
 * \param read_cb callback to read data (must not be NULL)
 * \param seek_cb callback to seek, or NULL if seeking is not supported
 * \param close_cb callback to close the media, or NULL if unnecessary
 * \param opaque data pointer for the open callback
 *
 * \return the newly created media or NULL on error
 *
 * \note If open_cb is NULL, the opaque pointer will be passed to read_cb,
 * seek_cb and close_cb, and the stream size will be treated as unknown.
 *
 * \note The callbacks may be called asynchronously (from another thread).
 * A single stream instance need not be reentrant. However the open_cb needs to
 * be reentrant if the media is used by multiple player instances.
 *
 * \warning The callbacks may be used until all or any player instances
 * that were supplied the media item are stopped.
 *
 * \see libvlc_media_release
 *
 * \version LibVLC 3.0.0 and later.
 */
LIBVLC_API libvlc_media_t *libvlc_media_new_callbacks(
                                   libvlc_instance_t *instance,
                                   libvlc_media_open_cb open_cb,
                                   libvlc_media_read_cb read_cb,
                                   libvlc_media_seek_cb seek_cb,
                                   libvlc_media_close_cb close_cb,
                                   void *opaque );

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
 * \note The options are listed in 'vlc --longhelp' from the command line,
 * e.g. "--sout-all". Keep in mind that available options and their semantics
 * vary across LibVLC versions and builds.
 * \warning Not all options affects libvlc_media_t objects:
 * Specifically, due to architectural issues most audio and video options,
 * such as text renderer options, have no effects on an individual media.
 * These options must be set through libvlc_new() instead.
 *
 * \param p_md the media descriptor
 * \param psz_options the options (as a string)
 */
LIBVLC_API void libvlc_media_add_option(
                                   libvlc_media_t *p_md,
                                   const char * psz_options );

/**
 * Add an option to the media with configurable flags.
 *
 * This option will be used to determine how the media_player will
 * read the media. This allows to use VLC's advanced
 * reading/streaming options on a per-media basis.
 *
 * The options are detailed in vlc --longhelp, for instance
 * "--sout-all". Note that all options are not usable on medias:
 * specifically, due to architectural issues, video-related options
 * such as text renderer options cannot be set on a single media. They
 * must be set on the whole libvlc instance instead.
 *
 * \param p_md the media descriptor
 * \param psz_options the options (as a string)
 * \param i_flags the flags for this option
 */
LIBVLC_API void libvlc_media_add_option_flag(
                                   libvlc_media_t *p_md,
                                   const char * psz_options,
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
 * Note, you need to call libvlc_media_parse_with_options() or play the media
 * at least once before calling this function.
 * If the media has not yet been parsed this will return NULL.
 *
 * \see libvlc_media_parse_with_options
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
 * Get current state of media descriptor object. Possible media states are
 * libvlc_NothingSpecial=0, libvlc_Opening, libvlc_Playing, libvlc_Paused,
 * libvlc_Stopped, libvlc_Ended, libvlc_Error.
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
 * \retval true statistics are available
 * \retval false otherwise
 */
LIBVLC_API bool libvlc_media_get_stats(libvlc_media_t *p_md,
                                       libvlc_media_stats_t *p_stats);

/* The following method uses libvlc_media_list_t, however, media_list usage is optionnal
 * and this is here for convenience */
#define VLC_FORWARD_DECLARE_OBJECT(a) struct a

/**
 * Get subitems of media descriptor object. This will increment
 * the reference count of supplied media descriptor object. Use
 * libvlc_media_list_release() to decrement the reference counting.
 *
 * \param p_md media descriptor object
 * \return list of media descriptor subitems or NULL
 */
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
 * Note, you need to call libvlc_media_parse_with_options() or play the media
 * at least once before calling this function.
 * Not doing this will result in an undefined result.
 *
 * \see libvlc_media_parse_with_options
 *
 * \param p_md media descriptor object
 * \return duration of media item or -1 on error
 */
LIBVLC_API libvlc_time_t
   libvlc_media_get_duration( libvlc_media_t *p_md );

/**
 * Parse the media asynchronously with options.
 *
 * This fetches (local or network) art, meta data and/or tracks information.
 *
 * To track when this is over you can listen to libvlc_MediaParsedChanged
 * event. However if this functions returns an error, you will not receive any
 * events.
 *
 * It uses a flag to specify parse options (see libvlc_media_parse_flag_t). All
 * these flags can be combined. By default, media is parsed if it's a local
 * file.
 *
 * \note Parsing can be aborted with libvlc_media_parse_stop().
 *
 * \see libvlc_MediaParsedChanged
 * \see libvlc_media_get_meta
 * \see libvlc_media_get_tracklist
 * \see libvlc_media_get_parsed_status
 * \see libvlc_media_parse_flag_t
 *
 * \param p_md media descriptor object
 * \param parse_flag parse options:
 * \param timeout maximum time allowed to preparse the media. If -1, the
 * default "preparse-timeout" option will be used as a timeout. If 0, it will
 * wait indefinitely. If > 0, the timeout will be used (in milliseconds).
 * \return -1 in case of error, 0 otherwise
 * \version LibVLC 3.0.0 or later
 */
LIBVLC_API int
libvlc_media_parse_with_options( libvlc_media_t *p_md,
                                 libvlc_media_parse_flag_t parse_flag,
                                 int timeout );

/**
 * Stop the parsing of the media
 *
 * When the media parsing is stopped, the libvlc_MediaParsedChanged event will
 * be sent with the libvlc_media_parsed_status_timeout status.
 *
 * \see libvlc_media_parse_with_options
 *
 * \param p_md media descriptor object
 * \version LibVLC 3.0.0 or later
 */
LIBVLC_API void
libvlc_media_parse_stop( libvlc_media_t *p_md );

/**
 * Get Parsed status for media descriptor object.
 *
 * \see libvlc_MediaParsedChanged
 * \see libvlc_media_parsed_status_t
 * \see libvlc_media_parse_with_options
 *
 * \param p_md media descriptor object
 * \return a value of the libvlc_media_parsed_status_t enum
 * \version LibVLC 3.0.0 or later
 */
LIBVLC_API libvlc_media_parsed_status_t
   libvlc_media_get_parsed_status( libvlc_media_t *p_md );

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
 * \see libvlc_media_set_user_data
 *
 * \param p_md media descriptor object
 */
LIBVLC_API void *libvlc_media_get_user_data( libvlc_media_t *p_md );

/**
 * Get media descriptor's elementary streams description
 *
 * Note, you need to call libvlc_media_parse_with_options() or play the media
 * at least once before calling this function.
 * Not doing this will result in an empty array.
 *
 * \version LibVLC 2.1.0 and later.
 * \see libvlc_media_parse_with_options
 *
 * \param p_md media descriptor object
 * \param tracks address to store an allocated array of Elementary Streams
 *        descriptions (must be freed with libvlc_media_tracks_release
          by the caller) [OUT]
 *
 * \return the number of Elementary Streams (zero on error)
 */
LIBVLC_API
unsigned libvlc_media_tracks_get( libvlc_media_t *p_md,
                                  libvlc_media_track_t ***tracks );

/**
 * Get the track list for one type
 *
 * \version LibVLC 4.0.0 and later.
 *
 * \note You need to call libvlc_media_parse_with_options() or play the media
 * at least once before calling this function.  Not doing this will result in
 * an empty list.
 *
 * \see libvlc_media_parse_with_options
 * \see libvlc_media_tracklist_count
 * \see libvlc_media_tracklist_at
 *
 * \param p_md media descriptor object
 * \param type type of the track list to request
 *
 * \return a valid libvlc_media_tracklist_t or NULL in case of error, if there
 * is no track for a category, the returned list will have a size of 0, delete
 * with libvlc_media_tracklist_delete()
 */
LIBVLC_API libvlc_media_tracklist_t *
libvlc_media_get_tracklist( libvlc_media_t *p_md, libvlc_track_type_t type );

/**
 * Get codec description from media elementary stream
 *
 * Note, you need to call libvlc_media_parse_with_options() or play the media
 * at least once before calling this function.
 *
 * \version LibVLC 3.0.0 and later.
 *
 * \see libvlc_media_track_t
 * \see libvlc_media_parse_with_options
 *
 * \param i_type i_type from libvlc_media_track_t
 * \param i_codec i_codec or i_original_fourcc from libvlc_media_track_t
 *
 * \return codec description
 */
LIBVLC_API
const char *libvlc_media_get_codec_description( libvlc_track_type_t i_type,
                                                uint32_t i_codec );

/**
 * Release media descriptor's elementary streams description array
 *
 * \version LibVLC 2.1.0 and later.
 *
 * \param p_tracks tracks info array to release
 * \param i_count number of elements in the array
 */
LIBVLC_API
void libvlc_media_tracks_release( libvlc_media_track_t **p_tracks,
                                  unsigned i_count );

/**
 * Get the media type of the media descriptor object
 *
 * \version LibVLC 3.0.0 and later.
 *
 * \see libvlc_media_type_t
 *
 * \param p_md media descriptor object
 *
 * \return media type
 */
LIBVLC_API
libvlc_media_type_t libvlc_media_get_type( libvlc_media_t *p_md );

/**
 * \brief libvlc_media_thumbnail_request_t An opaque thumbnail request object
 */
typedef struct libvlc_media_thumbnail_request_t libvlc_media_thumbnail_request_t;

typedef enum libvlc_thumbnailer_seek_speed_t
{
    libvlc_media_thumbnail_seek_precise,
    libvlc_media_thumbnail_seek_fast,
} libvlc_thumbnailer_seek_speed_t;

/**
 * \brief libvlc_media_get_thumbnail_by_time Start an asynchronous thumbnail generation
 *
 * If the request is successfuly queued, the libvlc_MediaThumbnailGenerated
 * is guaranteed to be emited.
 * The resulting thumbnail size can either be:
 * - Hardcoded by providing both width & height. In which case, the image will
 *   be stretched to match the provided aspect ratio, or cropped if crop is true.
 * - Derived from the media aspect ratio if only width or height is provided and
 *   the other one is set to 0.
 *
 * \param md media descriptor object
 * \param time The time at which the thumbnail should be generated
 * \param speed The seeking speed \sa{libvlc_thumbnailer_seek_speed_t}
 * \param width The thumbnail width
 * \param height the thumbnail height
 * \param crop Should the picture be cropped to preserve source aspect ratio
 * \param picture_type The thumbnail picture type \sa{libvlc_picture_type_t}
 * \param timeout A timeout value in ms, or 0 to disable timeout
 *
 * \return A valid opaque request object, or NULL in case of failure.
 * It may be cancelled by libvlc_media_thumbnail_request_cancel().
 * It must be released by libvlc_media_thumbnail_request_destroy().
 *
 * \version libvlc 4.0 or later
 *
 * \see libvlc_picture_t
 * \see libvlc_picture_type_t
 */
LIBVLC_API libvlc_media_thumbnail_request_t*
libvlc_media_thumbnail_request_by_time( libvlc_media_t *md,
                                        libvlc_time_t time,
                                        libvlc_thumbnailer_seek_speed_t speed,
                                        unsigned int width, unsigned int height,
                                        bool crop, libvlc_picture_type_t picture_type,
                                        libvlc_time_t timeout );

/**
 * \brief libvlc_media_get_thumbnail_by_pos Start an asynchronous thumbnail generation
 *
 * If the request is successfuly queued, the libvlc_MediaThumbnailGenerated
 * is guaranteed to be emited.
 * The resulting thumbnail size can either be:
 * - Hardcoded by providing both width & height. In which case, the image will
 *   be stretched to match the provided aspect ratio, or cropped if crop is true.
 * - Derived from the media aspect ratio if only width or height is provided and
 *   the other one is set to 0.
 *
 * \param md media descriptor object
 * \param pos The position at which the thumbnail should be generated
 * \param speed The seeking speed \sa{libvlc_thumbnailer_seek_speed_t}
 * \param width The thumbnail width
 * \param height the thumbnail height
 * \param crop Should the picture be cropped to preserve source aspect ratio
 * \param picture_type The thumbnail picture type \sa{libvlc_picture_type_t}
 * \param timeout A timeout value in ms, or 0 to disable timeout
 *
 * \return A valid opaque request object, or NULL in case of failure.
 * It may be cancelled by libvlc_media_thumbnail_request_cancel().
 * It must be released by libvlc_media_thumbnail_request_destroy().
 *
 * \version libvlc 4.0 or later
 *
 * \see libvlc_picture_t
 * \see libvlc_picture_type_t
 */
LIBVLC_API libvlc_media_thumbnail_request_t*
libvlc_media_thumbnail_request_by_pos( libvlc_media_t *md,
                                       float pos,
                                       libvlc_thumbnailer_seek_speed_t speed,
                                       unsigned int width, unsigned int height,
                                       bool crop, libvlc_picture_type_t picture_type,
                                       libvlc_time_t timeout );

/**
 * @brief libvlc_media_thumbnail_cancel cancels a thumbnailing request
 * @param p_req An opaque thumbnail request object.
 *
 * Cancelling the request will still cause libvlc_MediaThumbnailGenerated event
 * to be emited, with a NULL libvlc_picture_t
 * If the request is cancelled after its completion, the behavior is undefined.
 */
LIBVLC_API void
libvlc_media_thumbnail_request_cancel( libvlc_media_thumbnail_request_t *p_req );

/**
 * @brief libvlc_media_thumbnail_destroy destroys a thumbnail request
 * @param p_req An opaque thumbnail request object.
 *
 * If the request has not completed or hasn't been cancelled yet, the behavior
 * is undefined
 */
LIBVLC_API void
libvlc_media_thumbnail_request_destroy( libvlc_media_thumbnail_request_t *p_req );

/**
 * Add a slave to the current media.
 *
 * A slave is an external input source that may contains an additional subtitle
 * track (like a .srt) or an additional audio track (like a .ac3).
 *
 * \note This function must be called before the media is parsed (via
 * libvlc_media_parse_with_options()) or before the media is played (via
 * libvlc_media_player_play())
 *
 * \version LibVLC 3.0.0 and later.
 *
 * \param p_md media descriptor object
 * \param i_type subtitle or audio
 * \param i_priority from 0 (low priority) to 4 (high priority)
 * \param psz_uri Uri of the slave (should contain a valid scheme).
 *
 * \return 0 on success, -1 on error.
 */
LIBVLC_API
int libvlc_media_slaves_add( libvlc_media_t *p_md,
                             libvlc_media_slave_type_t i_type,
                             unsigned int i_priority,
                             const char *psz_uri );

/**
 * Clear all slaves previously added by libvlc_media_slaves_add() or
 * internally.
 *
 * \version LibVLC 3.0.0 and later.
 *
 * \param p_md media descriptor object
 */
LIBVLC_API
void libvlc_media_slaves_clear( libvlc_media_t *p_md );

/**
 * Get a media descriptor's slave list
 *
 * The list will contain slaves parsed by VLC or previously added by
 * libvlc_media_slaves_add(). The typical use case of this function is to save
 * a list of slave in a database for a later use.
 *
 * \version LibVLC 3.0.0 and later.
 *
 * \see libvlc_media_slaves_add
 *
 * \param p_md media descriptor object
 * \param ppp_slaves address to store an allocated array of slaves (must be
 * freed with libvlc_media_slaves_release()) [OUT]
 *
 * \return the number of slaves (zero on error)
 */
LIBVLC_API
unsigned int libvlc_media_slaves_get( libvlc_media_t *p_md,
                                      libvlc_media_slave_t ***ppp_slaves );

/**
 * Release a media descriptor's slave list
 *
 * \version LibVLC 3.0.0 and later.
 *
 * \param pp_slaves slave array to release
 * \param i_count number of elements in the array
 */
LIBVLC_API
void libvlc_media_slaves_release( libvlc_media_slave_t **pp_slaves,
                                  unsigned int i_count );

/** @}*/

# ifdef __cplusplus
}
# endif

#endif /* VLC_LIBVLC_MEDIA_H */
