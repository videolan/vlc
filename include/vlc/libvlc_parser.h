/*****************************************************************************
 * libvlc_parser.h:  libvlc parser API
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#ifndef VLC_LIBVLC_PARSER_H
#define VLC_LIBVLC_PARSER_H 1

#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>

# ifdef __cplusplus
extern "C" {
# endif

/** \defgroup libvlc_parser LibVLC parser
 * \ingroup libvlc
 * \ref libvlc_parser_t is an abstract representation of a parser
 * @{
 * \file
 * LibVLC parser API
 */
typedef struct libvlc_parser_t libvlc_parser_t;

typedef struct libvlc_media_t libvlc_media_t;
typedef struct libvlc_picture_list_t libvlc_picture_list_t;
typedef struct libvlc_picture_t libvlc_picture_t;
typedef enum libvlc_picture_type_t libvlc_picture_type_t;

/**
 * A parser request object
 */
typedef struct libvlc_parser_request_t libvlc_parser_request_t;

/**
 * A thumbnailer request object
 */
typedef struct libvlc_thumbnailer_request_t libvlc_thumbnailer_request_t;

/**
 * Opaque handle of a parsing/thumbnailing task.
 *
 * Identifies a task request submitted via libvlc_parser_queue()
 * or libvlc_parser_queue_thumbnailing(). It can be passed to
 * libvlc_parser_cancel_request() to cancel that request.
 *
 * \note Validity starts when a submit function returns a non-NULL handle
 * and ends with libvlc_parser_task_release().
 */
typedef struct libvlc_parser_task libvlc_parser_task;

/**
 * Parse flags used by libvlc_parser_request_t
 */
typedef enum libvlc_media_parse_flag_t
{
    /**
     * Parse media
     */
    libvlc_media_parse = 0x01,
    /**
     * Fetch meta and cover art using local resources
     */
    libvlc_media_fetch_local = 0x02,
    /**
     * Fetch meta and cover art using network resources
     */
    libvlc_media_fetch_network = 0x04,
    /**
     * Interact with the user (via libvlc_dialog_cbs) when preparsing this item
     * (and not its sub items). Set this flag in order to receive a callback
     * when the input is asking for credentials.
     */
    libvlc_media_do_interact = 0x08,
} libvlc_media_parse_flag_t;

/**
 * Outcome of a finished parse request, reported by
 * \ref libvlc_parser_cbs.on_parsed
 *
 */
typedef enum libvlc_parser_status_t
{
    /** The parsing failed */
    libvlc_parser_status_failed,
    /** The parsing timed out */
    libvlc_parser_status_timeout,
    /** The parsing was cancelled */
    libvlc_parser_status_cancelled,
    /** The parsing completed successfully */
    libvlc_parser_status_done,
} libvlc_parser_status_t;

/**
 * Thumbnailer seek type
 */
typedef enum libvlc_thumbnailer_seek_type_t
{
    /** Don't seek (default) */
    libvlc_thumbnailer_seek_none,
    /** Seek by time */
    libvlc_thumbnailer_seek_time,
    /** Seek by position */
    libvlc_thumbnailer_seek_pos,
} libvlc_thumbnailer_seek_type_t;

/**
 * Thumbnailer seek speed
 */
typedef enum libvlc_thumbnailer_seek_speed_t
{
    /** Precise, but potentially slow */
    libvlc_media_thumbnail_seek_precise,
    /** Fast, but potentially imprecise */
    libvlc_media_thumbnail_seek_fast,
} libvlc_thumbnailer_seek_speed_t;

/**
 * Thumbnailer seek value
 *
 * The active member is selected based on the associated
 * \ref libvlc_thumbnailer_seek_type_t.
 */
typedef union libvlc_thumbnailer_seek_value_t
{
    /** Seek time, only use if type == libvlc_thumbnailer_seek_time */
    libvlc_time_t time;
    /** Seek position, only use if type == libvlc_thumbnailer_seek_pos */
    double pos;
} libvlc_thumbnailer_seek_value_t;

/**
 * struct defining callbacks for libvlc_parser_queue
 */
struct libvlc_parser_cbs
{
    /** 
     * Version of struct libvlc_parser_cbs
     */
    uint32_t version;

    /**
     * Callback prototype that notify when a parser request finishes
     *
     * \note Mandatory (can't be NULL),
     * available since version 0
     *
     * \param opaque opaque pointer set by cbs_opaque
     * \param task opaque handle returned by libvlc_parser_queue()
     * \param status terminal parse outcome, \ref libvlc_parser_status_t
     */
    void (*on_parsed)(void *opaque,
                      libvlc_parser_task *task,
                      libvlc_parser_status_t status);

    /**
     * Callback prototype that notify when the parser add new attachments to
     * the media.
     *
     * Called before on_parsed, if there are valid attachments.
     *
     * \note Optional (can be NULL),
     * available since version 0
     *
     * \param opaque opaque pointer set by cbs_opaque
     * \param task opaque handle returned by libvlc_parser_queue()
     * \param list list of pictures, the list is only valid from this
     * callback, each pictures can be held separately with
     * libvlc_picture_retain().
     */
    void (*on_attachments_added)(void *opaque,
                                 libvlc_parser_task *task,
                                 libvlc_picture_list_t *list);
};

/**
 * struct defining a parse request
 */
struct libvlc_parser_request_t
{
    /**
     * Version of libvlc_parser_request_t
     */
    uint32_t version;

    /**
     * Media to parse
     *
     * \note Mandatory (can't be NULL),
     * available since version 0
     */
    libvlc_media_t *media;

    /**
     * Parse flags, a combination of \ref libvlc_media_parse_flag_t
     * (default to libvlc_media_parse if 0)
     *
     * \note Optional (can be 0),
     * available since version 0
     */
    libvlc_media_parse_flag_t parse_flags;
};

/**
 * struct defining callbacks for libvlc_parser_queue_thumbnailing
 */
struct libvlc_thumbnailer_cbs
{
    /**
     * Version of struct libvlc_thumbnailer_cbs
     */
    uint32_t version;

    /**
     * Callback prototype that notify when a thumbnailer request finishes
     *
     * \note Mandatory (can't be NULL),
     * available since version 0
     *
     * \param opaque opaque pointer set in cbs_opaque
     * \param task opaque handle returned by libvlc_parser_queue_thumbnailing()
     * \param picture generated thumbnail, the picture is only valid from this
     * callback, it can be held separately with libvlc_picture_retain().
     * NULL in case of an error, timeout or request was cancelled.
     */
    void (*on_ended)( void *opaque, libvlc_parser_task *task, libvlc_picture_t *picture );
};

/**
 * struct defining a thumbnailer request
 */
struct libvlc_thumbnailer_request_t
{
    /**
     * Version of libvlc_thumbnailer_request_t
     */
    uint32_t version;

    /**
     * Media source of the thumbnail.
     *
     * \note Mandatory (can't be NULL),
     * available since version 0
     */
    libvlc_media_t *media;

    /**
     * Thumbnail width (0 by default)
     * \note The resulting thumbnail size can either be:
     *
     * - Hardcoded by providing both width & height. In which case, the image will
     *   be stretched to match the provided aspect ratio, or cropped if crop is true.
     * 
     * - Derived from the media aspect ratio if only width or height is provided and
     *   the other one is set to 0.
     *
     * - Of the same size as the media if both width and height are set to 0.
     *
     * \note Optional (can be 0),
     * available since version 0
     */
    unsigned int width;

    /**
     * Thumbnail height (0 by default)
     * \note The resulting thumbnail size can either be:
     *
     * - Hardcoded by providing both width & height. In which case, the image will
     *   be stretched to match the provided aspect ratio, or cropped if crop is true.
     *
     * - Derived from the media aspect ratio if only width or height is provided and
     *   the other one is set to 0.
     *
     * - Of the same size as the media if both width and height are set to 0.
     *
     * \note Optional (can be 0),
     * available since version 0
     */
    unsigned int height;

    /**
     * True to enable crop (false by default)
     *
     * \note Only meaningful when both width and height are non-zero
     * (hardcoded thumbnail size); ignored otherwise.
     *
     * \note Optional (can be false),
     * available since version 0
     */
    bool crop;

    /**
     * Picture type (libvlc_picture_Argb by default)
     *
     * \note Optional (can be 0),
     * available since version 0
     */
    libvlc_picture_type_t type;

    /**
     * Seek parameters
     *
     * \note Optional (can be zero-initialized, in which case the members
     * will be set to their default values),
     * available since version 0
     */
    struct
    {
        /**
         * by time or by position (libvlc_thumbnailer_seek_none by default)
         */
        libvlc_thumbnailer_seek_type_t type;

        /** Seek value, the active member is selected by type
         *
         * \note `time` by default, although it won't be used if type is libvlc_thumbnailer_seek_none
         */
        libvlc_thumbnailer_seek_value_t value;

        /** precise or fast mode (libvlc_media_thumbnail_seek_precise by default) */
        libvlc_thumbnailer_seek_speed_t speed;
    } seek;

    /**
     * True to enable hardware decoder (false by default)
     *
     * \note Optional (can be false),
     * available since version 0
     */
    bool hw_dec;
};

/**
 * struct defining parser configuration
 */
struct libvlc_parser_cfg
{
    /**
     * Version of struct libvlc_parser_cfg
     */
    uint32_t version;

    /**
     * The maximum number of threads used by the parser, 0 for default
     * (1 thread)
     *
     * \note Optional (can be 0),
     * available since version 0
     */
    uint32_t max_parser_threads;

    /**
     * The maximum number of threads used by the thumbnailer, 0 for default
     * (1 thread)
     *
     * \note Optional (can be 0),
     * available since version 0
     */
    uint32_t max_thumbnailer_threads;

    /**
     * Timeout of the parser in us, 0 for no limits, or -1 to inherit the value of preparse-timeout
     *
     * \note Optional (can be 0),
     * available since version 0
     */
    libvlc_time_t timeout;
};

/**
 * Create a parser
 *
 * \param inst LibVLC instance to create parser with
 * \param cfg a pointer to a valid configuration struct
 * \return a new parser object, or NULL on error.
 * It must be released by libvlc_parser_destroy().
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API libvlc_parser_t *
libvlc_parser_new(libvlc_instance_t *inst,
                  const struct libvlc_parser_cfg *cfg);

/**
 * Destroy a parser and free resources.
 * All pending and running tasks are cancelled, and reported as cancelled
 * via callback. This API waits for all worker threads to join.
 *
 * \note It is safe to call this API while tasks are still running or pending.
 *
 * \param parser the parser
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API void libvlc_parser_destroy(libvlc_parser_t *parser);

/**
 * Parse a media asynchronously
 *
 * This fetches (local or network) art, meta data and/or tracks information.
 *
 * \note It is possible to cancel the request with
 * libvlc_parser_cancel_request()
 *
 * If the request is successfully queued, the \ref
 * libvlc_parser_cbs.on_parsed callback is guaranteed to be called
 *
 * \note A media can be parsed multiple times, for instance to refresh network
 * metadata; a previous successful, failed, timed out or cancelled parse does
 * not prevent re-queueing.
 *
 * \param parser the parser
 * \param req a pointer to a valid request struct
 * \param cbs a pointer to a valid callbacks struct. The pointed struct
 * must be kept alive (and not modified) by the caller until libvlc_parser_cbs.on_parsed
 * is called for the returned task handle.
 * \param cbs_opaque an opaque pointer to be passed to the callbacks
 * \return NULL in case of error, or a valid handle if the item was scheduled
 * for parsing. If this returns NULL, the \ref libvlc_parser_cbs.on_parsed
 * callback will *not* be called.
 * \version LibVLC 4.0.0 or later
 */
LIBVLC_API libvlc_parser_task *
libvlc_parser_queue(libvlc_parser_t *parser, const libvlc_parser_request_t *req,
                    const struct libvlc_parser_cbs *cbs, void *cbs_opaque);

/**
 * Generate a thumbnail asynchronously
 *
 * \note It is possible to cancel the request with
 * libvlc_parser_cancel_request()
 *
 * If the request is successfully queued, the \ref
 * libvlc_thumbnailer_cbs.on_ended callback is guaranteed to be called
 *
 * \param parser the parser
 * \param req a pointer to a valid request struct
 * \param cbs a pointer to a valid callbacks struct. The pointed struct
 * must be kept alive (and not modified) by the caller until libvlc_thumbnailer_cbs.on_ended
 * is called for the returned task handle.
 * \param cbs_opaque an opaque pointer to be passed to the callbacks
 * \return NULL in case of error, or a valid handle if the item was
 * scheduled for thumbnailing. If this returns an error, the \ref
 * libvlc_thumbnailer_cbs.on_ended callback will *not* be called.
 * \version LibVLC 4.0.0 or later
 */
LIBVLC_API libvlc_parser_task *
libvlc_parser_queue_thumbnailing(libvlc_parser_t *parser,
                                 const libvlc_thumbnailer_request_t *req,
                                 const struct libvlc_thumbnailer_cbs *cbs,
                                 void *cbs_opaque);

/**
 * Cancel a parser request
 *
 * \param parser the parser
 * \param task A parser task returned by libvlc_parser_queue(), libvlc_parser_queue_thumbnailing()
 * or NULL to cancel all requests.
 * \return the number of requests cancelled
 *
 * \note
 * - When a task is cancelled, the `on_parsed` callback will be triggered
 *   with libvlc_parser_status_cancelled status.
 *
 * - If the request is already in a terminated state (finished, cancelled, error, timeout),
 *   the call is a no-op and no callback will be invoked.
 * \version libvlc 4.0.0 or later
 */
LIBVLC_API size_t libvlc_parser_cancel_request(libvlc_parser_t *parser,
                                               libvlc_parser_task *task);

/**
 * Fetch the media associated with the task handle.
 *
 * \param task A parser task returned by libvlc_parser_queue() or libvlc_parser_queue_thumbnailing()
 * \return libvlc_media_t associated with the task
 *
 * \note The returned media is held by the task, it must not be
 * released by the caller.
 */
LIBVLC_API libvlc_media_t *
libvlc_parser_task_get_media(libvlc_parser_task *task);

/**
 * Release a parser task handle.
 *
 * \param task the parser task handle
 *
 * \note
 * - The task handle is retained when returned by a submit function.
 *
 * - Mandatory to call to avoid memory leaks.
 *
 * - It is safe to call this API from within the on_parsed/on_ended callback.
 *
 * - The task handle should not be used after calling this function.
 *
 * - If called on an active request, it doesn't cancel the task,
 *   use libvlc_parser_cancel_request() for that.
 */
LIBVLC_API void libvlc_parser_task_release(libvlc_parser_task *task);

/** @} */

# ifdef __cplusplus
}
# endif

#endif /* VLC_LIBVLC_PARSER_H */
