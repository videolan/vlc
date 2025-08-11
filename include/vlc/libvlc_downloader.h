/*****************************************************************************
 * libvlc_downloader.h:  LibVLC Downloader API
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#ifndef VLC_LIBVLC_DOWNLOADER_H
#define VLC_LIBVLC_DOWNLOADER_H 1

#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_list.h>

# ifdef __cplusplus
extern "C" {
# endif

/** \defgroup libvlc_downloader LibVLC downloader
 * \ingroup libvlc
 * @ref libvlc_downloader_t is an abstract representation of a downloader
 * @{
 * \file
 * LibVLC downloader API
 */
typedef struct libvlc_downloader_t libvlc_downloader_t;

/**
 * A downloader request object
 */
typedef struct libvlc_downloader_request_t libvlc_downloader_request_t;

/**
 * Opaque handle of a downloader task.
 *
 * Identifies a task request submitted via libvlc_downloader_queue().
 * It can be passed to libvlc_downloader_cancel() to cancel that request,
 * or to libvlc_downloader_set_pause() to pause/resume that request.
 *
 * \note Validity starts when libvlc_downloader_queue() returns a non-NULL handle
 * and ends with libvlc_downloader_task_release().
 */
typedef struct libvlc_downloader_task libvlc_downloader_task;

/**
 * Downloader status
 */
typedef enum libvlc_downloader_status_t
{
    /** download pending */
    libvlc_downloader_status_pending,
    /** active download in progress (not paused) */
    libvlc_downloader_status_running,
    /** download paused */
    libvlc_downloader_status_paused,
    /** download finished */
    libvlc_downloader_status_finished,
    /** download cancelled */
    libvlc_downloader_status_cancelled,
    /** download error occurred */
    libvlc_downloader_status_error,
} libvlc_downloader_status_t;

/** Sentinel return value to signal an error in the download (to be returned by on_buffer callback) */
#define LIBVLC_DOWNLOADER_CB_ERROR ((ptrdiff_t)-1)

/** Sentinel return value to cancel the download (to be returned by on_buffer callback) */
#define LIBVLC_DOWNLOADER_CB_CANCEL ((ptrdiff_t)-2)

/**
 * Downloader callbacks
 */
struct libvlc_downloader_cbs
{
    /**
     * Version of struct libvlc_downloader_cbs
     */
    uint32_t version;

    /**
     * Called when a buffer of data is read.
     *
     * \note Mandatory (can't be NULL),
     * available since version 0
     *
     * \warning Do not call any libvlc_downloader_* API from within this callback.
     * Only libvlc_downloader_task_* APIs may be called on the provided task handle.
     * And avoid blocking operations in this callback as it is invoked with the internal lock held.
     *
     * \param opaque user data
     * \param task opaque handle returned by libvlc_downloader_queue()
     * \param buf pointer to buffer (owned by downloader, only valid during callback)
     * \param len size of buffer
     * \param position total number of bytes read by the downloader so far
     * \param total total size of the media in bytes (only medias with finite size are allowed to download)
     *
     * \return The number of bytes consumed by the user, or \ref LIBVLC_DOWNLOADER_CB_ERROR to
     * terminate the download with an error, or \ref LIBVLC_DOWNLOADER_CB_CANCEL to cancel the download.
     *
     * If the returned number of bytes consumed is less than \p len (partial read), the downloader
     * auto-pauses as a form of backpressure. The user must call `libvlc_downloader_set_pause(dl, task, false)`
     * from the main thread to resume when ready to consume data again.
     *
     * \note User must copy the data if it needs to be accessed later.
     * The maximum buffer size is limited to around 64 KB.
     */
    ptrdiff_t (*on_buffer)(void *opaque, libvlc_downloader_task *task,
                           const uint8_t *buf, size_t len,
                           uint64_t position, uint64_t total);

    /**
     * Called when the downloader state changes.
     *
     * \note Mandatory (can't be NULL),
     * available since version 0
     *
     * Invoked when download starts, pauses, resumes, cancels, finishes
     * or errors out. \ref libvlc_downloader_status_t
     *
     * \warning Do not call any libvlc_downloader_* API from within this callback.
     * Only libvlc_downloader_task_* APIs may be called on the provided task handle.
     * And avoid blocking operations in this callback as it is invoked with the internal lock held.
     *
     * \param opaque user data
     * \param task opaque handle returned by libvlc_downloader_queue()
     * \param status download status
     */
    void (*on_state_update)(void *opaque, libvlc_downloader_task *task,
                            libvlc_downloader_status_t status);

    /**
     * Called when subitems of the media are available.
     *
     * \note Optional (can be NULL),
     * available since version 0
     *
     * \param opaque user data
     * \param task opaque handle returned by libvlc_downloader_queue()
     * \param subitems media list of subitems (owned by LibVLC)
     */
    void (*on_subitems)(void *opaque, libvlc_downloader_task *task,
                        libvlc_media_list_t *subitems);

    /**
     * Called when the parsed media has slaves.
     *
     * \note Optional (can be NULL),
     * available since version 0
     *
     * \param opaque user data
     * \param task opaque handle returned by libvlc_downloader_queue()
     * \param slaves array of libvlc_media_slave_t* (owned by LibVLC)
     * \param count number of slaves
     */
    void (*on_slaves)(void *opaque, libvlc_downloader_task *task,
                      libvlc_media_slave_t **slaves, size_t count);
};

/**
 * struct defining a downloader request
 */
struct libvlc_downloader_request_t
{
    /**
     * Version of libvlc_downloader_request_t
     */
    uint32_t version;

    /**
     * Media to download
     *
     * \note Mandatory (can't be NULL),
     * available since version 0
     *
     * - Only finite-size media are allowed to download.
     *
     * - If the media is a playlist or directory, the user will be notified of the
     *   subitems via the on_subitems callback and the download will not proceed.
     *
     * - If the media is a livestream or unknown type, the download will error out.
     */
    libvlc_media_t *media;
};

/**
 * struct defining downloader configuration
 */
struct libvlc_downloader_cfg
{
    /**
     * Version of struct libvlc_downloader_cfg
     */
    uint32_t version;

    /**
     * The maximum number of threads used by the parser internally, 0 for default
     * (1 thread)
     *
     * \note Optional (can be 0),
     * available since version 0
     */
    uint32_t max_parser_threads;
};

/**
 * Create a downloader instance.
 *
 * Supports downloading files over a limited set of protocols:
 * http(s), ftp, file, nfs, smb, sftp
 *
 * The downloader must be released by calling libvlc_downloader_destroy()
 * when it is no longer needed.
 *
 * \param inst LibVLC instance
 * \param cfg a pointer to a valid downloader configuration struct
 * \return downloader instance or NULL on error
 * 
 * \version LibVLC 4.0.0 or later
 */
LIBVLC_API libvlc_downloader_t *
libvlc_downloader_new(libvlc_instance_t *inst, const struct libvlc_downloader_cfg *cfg);

/**
 * Download a media asynchronously.
 *
 * - The downloader first parses the media.
 *
 * - If the media has subitems, the user will be notified via the on_subitems callback.
 *
 * - If the media has slaves, the user will be notified via the on_slaves callback.
 *
 * - If the media is not a file type,
 *   the download will not proceed. \see libvlc_media_type_t,
 *   and the user will be notified via the on_state_update callback.
 *
 * - If the media is a file type with finite size, the download starts in a separate thread.
 *
 * \param downloader downloader instance
 * \param req a pointer to a valid request struct
 * \param cbs a pointer to a valid callbacks struct. The pointed struct
 * must be kept alive (and not modified) by the caller until libvlc_downloader_cbs.on_state_update()
 * is called for the returned task handle with a terminal state (finished/cancelled/error).
 * \param cbs_opaque opaque pointer for callbacks
 * \return NULL in case of error, or a valid handle if the request was
 * scheduled for downloading.
 *
 * \note No callbacks will be invoked if the return value is NULL.
 *
 * \version LibVLC 4.0.0 or later
 */
LIBVLC_API libvlc_downloader_task *
libvlc_downloader_queue(libvlc_downloader_t *downloader, const libvlc_downloader_request_t *req,
                        const struct libvlc_downloader_cbs *cbs, void *cbs_opaque);

/**
 * Cancel an ongoing download.
 *
 * \param downloader downloader instance
 * \param task a downloader task returned by libvlc_downloader_queue(), 
 * or NULL to cancel all requests.
 *
 * \return the number of requests cancelled
 *
 * \note
 * - This function is valid only if the request is in one of the following states:
 *   pending, running, or paused.
 *
 * - When a request is cancelled, the `on_state_update` callback will be triggered
 *   with the cancelled state.
 *
 * - If the request is already in a terminated state (finished, cancelled, or error),
 *   the call is a no-op and no callback will be invoked.
 *
 * \version LibVLC 4.0.0 or later
 */
LIBVLC_API size_t libvlc_downloader_cancel(libvlc_downloader_t *downloader, libvlc_downloader_task *task);

/**
 * Toggle pause/resume for the download.
 *
 * \param downloader downloader instance
 * \param task a valid downloader task returned by libvlc_downloader_queue()
 * \param paused true to pause, false to resume
 *
 * \note This API is valid only when the download is in pending/running/paused state.
 * And the on_state_update callback with paused/running state will be called only during these
 * state changes. Else, for finished/cancelled/error states, it's a no-op and
 * no callback will be called.
 *
 * \version LibVLC 4.0.0 or later
 */
LIBVLC_API void libvlc_downloader_set_pause(libvlc_downloader_t *downloader,
                                            libvlc_downloader_task *task,
                                            bool paused);

/**
 * Destroy a downloader and free resources.
 * All pending, running and paused downloads are cancelled.
 * Waits for all download threads to join.
 *
 * \param downloader downloader instance
 *
 * \version LibVLC 4.0.0 or later
 */
LIBVLC_API void libvlc_downloader_destroy(libvlc_downloader_t *downloader);

/**
 * Get the media associated with the downloader request handle.
 *
 * \param task opaque handle returned by libvlc_downloader_queue()
 * \return the media associated with the request handle.
 *
 * \note The returned media is held by the task, it must not be
 * released by the caller.
 *
 * \version LibVLC 4.0.0 or later
 */
LIBVLC_API libvlc_media_t *
libvlc_downloader_task_get_media(libvlc_downloader_task *task);

/**
 * Release a downloader task handle.
 *
 * \param task the downloader task handle
 *
 * \note
 * - The task handle is retained when returned by libvlc_downloader_queue().
 *
 * - Mandatory to call to avoid memory leaks.
 *
 * - It is safe to call this API from within the on_state_update callback, when it
 *   reports a terminal state (finished, cancelled, error) \see libvlc_downloader_status_t.
 *
 * - The task handle should not be used after calling this function.
 *
 * - If called on an active task, it doesn't cancel the task,
 *   use \ref libvlc_downloader_cancel() for that.
 *
 * \version LibVLC 4.0.0 or later
 */
LIBVLC_API void libvlc_downloader_task_release(libvlc_downloader_task *task);

/** @}*/

# ifdef __cplusplus
}
# endif

#endif /* VLC_LIBVLC_DOWNLOADER_H */
