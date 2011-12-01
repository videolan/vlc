/*****************************************************************************
 * libvlc_media_player.h:  libvlc_media_player external API
 *****************************************************************************
 * Copyright (C) 1998-2010 VLC authors and VideoLAN
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
 * This file defines libvlc_media_player external API
 */

#ifndef VLC_LIBVLC_MEDIA_PLAYER_H
#define VLC_LIBVLC_MEDIA_PLAYER_H 1

# ifdef __cplusplus
extern "C" {
# else
#  include <stdbool.h>
# endif

/*****************************************************************************
 * Media Player
 *****************************************************************************/
/** \defgroup libvlc_media_player LibVLC media player
 * \ingroup libvlc
 * A LibVLC media player plays one media (usually in a custom drawable).
 * @{
 */

typedef struct libvlc_media_player_t libvlc_media_player_t;

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
 * Description for audio output. It contains
 * name, description and pointer to next record.
 */
typedef struct libvlc_audio_output_t
{
    char *psz_name;
    char *psz_description;
    struct libvlc_audio_output_t *p_next;

} libvlc_audio_output_t;

/**
 * Rectangle type for video geometry
 */
typedef struct libvlc_rectangle_t
{
    int top, left;
    int bottom, right;
} libvlc_rectangle_t;

/**
 * Marq options definition
 */
typedef enum libvlc_video_marquee_option_t {
    libvlc_marquee_Enable = 0,
    libvlc_marquee_Text,                  /** string argument */
    libvlc_marquee_Color,
    libvlc_marquee_Opacity,
    libvlc_marquee_Position,
    libvlc_marquee_Refresh,
    libvlc_marquee_Size,
    libvlc_marquee_Timeout,
    libvlc_marquee_X,
    libvlc_marquee_Y
} libvlc_video_marquee_option_t;

/**
 * Navigation mode
 */
typedef enum libvlc_navigate_mode_t
{
    libvlc_navigate_activate = 0,
    libvlc_navigate_up,
    libvlc_navigate_down,
    libvlc_navigate_left,
    libvlc_navigate_right
} libvlc_navigate_mode_t;

/**
 * Create an empty Media Player object
 *
 * \param p_libvlc_instance the libvlc instance in which the Media Player
 *        should be created.
 * \return a new media player object, or NULL on error.
 */
LIBVLC_API libvlc_media_player_t * libvlc_media_player_new( libvlc_instance_t *p_libvlc_instance );

/**
 * Create a Media Player object from a Media
 *
 * \param p_md the media. Afterwards the p_md can be safely
 *        destroyed.
 * \return a new media player object, or NULL on error.
 */
LIBVLC_API libvlc_media_player_t * libvlc_media_player_new_from_media( libvlc_media_t *p_md );

/**
 * Release a media_player after use
 * Decrement the reference count of a media player object. If the
 * reference count is 0, then libvlc_media_player_release() will
 * release the media player object. If the media player object
 * has been released, then it should not be used again.
 *
 * \param p_mi the Media Player to free
 */
LIBVLC_API void libvlc_media_player_release( libvlc_media_player_t *p_mi );

/**
 * Retain a reference to a media player object. Use
 * libvlc_media_player_release() to decrement reference count.
 *
 * \param p_mi media player object
 */
LIBVLC_API void libvlc_media_player_retain( libvlc_media_player_t *p_mi );

/**
 * Set the media that will be used by the media_player. If any,
 * previous md will be released.
 *
 * \param p_mi the Media Player
 * \param p_md the Media. Afterwards the p_md can be safely
 *        destroyed.
 */
LIBVLC_API void libvlc_media_player_set_media( libvlc_media_player_t *p_mi,
                                                   libvlc_media_t *p_md );

/**
 * Get the media used by the media_player.
 *
 * \param p_mi the Media Player
 * \return the media associated with p_mi, or NULL if no
 *         media is associated
 */
LIBVLC_API libvlc_media_t * libvlc_media_player_get_media( libvlc_media_player_t *p_mi );

/**
 * Get the Event Manager from which the media player send event.
 *
 * \param p_mi the Media Player
 * \return the event manager associated with p_mi
 */
LIBVLC_API libvlc_event_manager_t * libvlc_media_player_event_manager ( libvlc_media_player_t *p_mi );

/**
 * is_playing
 *
 * \param p_mi the Media Player
 * \return 1 if the media player is playing, 0 otherwise
 *
 * \libvlc_return_bool
 */
LIBVLC_API int libvlc_media_player_is_playing ( libvlc_media_player_t *p_mi );

/**
 * Play
 *
 * \param p_mi the Media Player
 * \return 0 if playback started (and was already started), or -1 on error.
 */
LIBVLC_API int libvlc_media_player_play ( libvlc_media_player_t *p_mi );

/**
 * Pause or resume (no effect if there is no media)
 *
 * \param mp the Media Player
 * \param do_pause play/resume if zero, pause if non-zero
 * \version LibVLC 1.1.1 or later
 */
LIBVLC_API void libvlc_media_player_set_pause ( libvlc_media_player_t *mp,
                                                    int do_pause );

/**
 * Toggle pause (no effect if there is no media)
 *
 * \param p_mi the Media Player
 */
LIBVLC_API void libvlc_media_player_pause ( libvlc_media_player_t *p_mi );

/**
 * Stop (no effect if there is no media)
 *
 * \param p_mi the Media Player
 */
LIBVLC_API void libvlc_media_player_stop ( libvlc_media_player_t *p_mi );

/**
 * Callback prototype to allocate and lock a picture buffer.
 *
 * Whenever a new video frame needs to be decoded, the lock callback is
 * invoked. Depending on the video chroma, one or three pixel planes of
 * adequate dimensions must be returned via the second parameter. Those
 * planes must be aligned on 32-bytes boundaries.
 *
 * \param opaque private pointer as passed to libvlc_video_set_callbacks() [IN]
 * \param planes start address of the pixel planes (LibVLC allocates the array
 *             of void pointers, this callback must initialize the array) [OUT]
 * \return a private pointer for the display and unlock callbacks to identify
 *         the picture buffers
 */
typedef void *(*libvlc_video_lock_cb)(void *opaque, void **planes);

/**
 * Callback prototype to unlock a picture buffer.
 *
 * When the video frame decoding is complete, the unlock callback is invoked.
 * This callback might not be needed at all. It is only an indication that the
 * application can now read the pixel values if it needs to.
 *
 * \warning A picture buffer is unlocked after the picture is decoded,
 * but before the picture is displayed.
 *
 * \param opaque private pointer as passed to libvlc_video_set_callbacks() [IN]
 * \param picture private pointer returned from the @ref libvlc_video_lock_cb
 *                callback [IN]
 * \param planes pixel planes as defined by the @ref libvlc_video_lock_cb
 *               callback (this parameter is only for convenience) [IN]
 */
typedef void (*libvlc_video_unlock_cb)(void *opaque, void *picture,
                                       void *const *planes);

/**
 * Callback prototype to display a picture.
 *
 * When the video frame needs to be shown, as determined by the media playback
 * clock, the display callback is invoked.
 *
 * \param opaque private pointer as passed to libvlc_video_set_callbacks() [IN]
 * \param picture private pointer returned from the @ref libvlc_video_lock_cb
 *                callback [IN]
 */
typedef void (*libvlc_video_display_cb)(void *opaque, void *picture);

/**
 * Callback prototype to configure picture buffers format.
 * This callback gets the format of the video as output by the video decoder
 * and the chain of video filters (if any). It can opt to change any parameter
 * as it needs. In that case, LibVLC will attempt to convert the video format
 * (rescaling and chroma conversion) but these operations can be CPU intensive.
 *
 * \param opaque pointer to the private pointer passed to
 *               libvlc_video_set_callbacks() [IN/OUT]
 * \param chroma pointer to the 4 bytes video format identifier [IN/OUT]
 * \param width pointer to the pixel width [IN/OUT]
 * \param height pointer to the pixel height [IN/OUT]
 * \param pitches table of scanline pitches in bytes for each pixel plane
 *                (the table is allocated by LibVLC) [OUT]
 * \param lines table of scanlines count for each plane [OUT]
 * \return the number of picture buffers allocated, 0 indicates failure
 *
 * \note
 * For each pixels plane, the scanline pitch must be bigger than or equal to
 * the number of bytes per pixel multiplied by the pixel width.
 * Similarly, the number of scanlines must be bigger than of equal to
 * the pixel height.
 * Furthermore, we recommend that pitches and lines be multiple of 32
 * to not break assumption that might be made by various optimizations
 * in the video decoders, video filters and/or video converters.
 */
typedef unsigned (*libvlc_video_format_cb)(void **opaque, char *chroma,
                                           unsigned *width, unsigned *height,
                                           unsigned *pitches,
                                           unsigned *lines);

/**
 * Callback prototype to configure picture buffers format.
 *
 * \param opaque private pointer as passed to libvlc_video_set_callbacks()
 *               (and possibly modified by @ref libvlc_video_format_cb) [IN]
 */
typedef void (*libvlc_video_cleanup_cb)(void *opaque);


/**
 * Set callbacks and private data to render decoded video to a custom area
 * in memory.
 * Use libvlc_video_set_format() or libvlc_video_set_format_callbacks()
 * to configure the decoded format.
 *
 * \param mp the media player
 * \param lock callback to lock video memory (must not be NULL)
 * \param unlock callback to unlock video memory (or NULL if not needed)
 * \param display callback to display video (or NULL if not needed)
 * \param opaque private pointer for the three callbacks (as first parameter)
 * \version LibVLC 1.1.1 or later
 */
LIBVLC_API
void libvlc_video_set_callbacks( libvlc_media_player_t *mp,
                                 libvlc_video_lock_cb lock,
                                 libvlc_video_unlock_cb unlock,
                                 libvlc_video_display_cb display,
                                 void *opaque );

/**
 * Set decoded video chroma and dimensions.
 * This only works in combination with libvlc_video_set_callbacks(),
 * and is mutually exclusive with libvlc_video_set_format_callbacks().
 *
 * \param mp the media player
 * \param chroma a four-characters string identifying the chroma
 *               (e.g. "RV32" or "YUYV")
 * \param width pixel width
 * \param height pixel height
 * \param pitch line pitch (in bytes)
 * \version LibVLC 1.1.1 or later
 * \bug All pixel planes are expected to have the same pitch.
 * To use the YCbCr color space with chrominance subsampling,
 * consider using libvlc_video_set_format_callbacks() instead.
 */
LIBVLC_API
void libvlc_video_set_format( libvlc_media_player_t *mp, const char *chroma,
                              unsigned width, unsigned height,
                              unsigned pitch );

/**
 * Set decoded video chroma and dimensions. This only works in combination with
 * libvlc_video_set_callbacks().
 *
 * \param mp the media player
 * \param setup callback to select the video format (cannot be NULL)
 * \param cleanup callback to release any allocated resources (or NULL)
 * \version LibVLC 1.2.0 or later
 */
LIBVLC_API
void libvlc_video_set_format_callbacks( libvlc_media_player_t *mp,
                                        libvlc_video_format_cb setup,
                                        libvlc_video_cleanup_cb cleanup );

/**
 * Set the NSView handler where the media player should render its video output.
 *
 * Use the vout called "macosx".
 *
 * The drawable is an NSObject that follow the VLCOpenGLVideoViewEmbedding
 * protocol:
 *
 * @begincode
 * \@protocol VLCOpenGLVideoViewEmbedding <NSObject>
 * - (void)addVoutSubview:(NSView *)view;
 * - (void)removeVoutSubview:(NSView *)view;
 * \@end
 * @endcode
 *
 * Or it can be an NSView object.
 *
 * If you want to use it along with Qt4 see the QMacCocoaViewContainer. Then
 * the following code should work:
 * @begincode
 * {
 *     NSView *video = [[NSView alloc] init];
 *     QMacCocoaViewContainer *container = new QMacCocoaViewContainer(video, parent);
 *     libvlc_media_player_set_nsobject(mp, video);
 *     [video release];
 * }
 * @endcode
 *
 * You can find a live example in VLCVideoView in VLCKit.framework.
 *
 * \param p_mi the Media Player
 * \param drawable the drawable that is either an NSView or an object following
 * the VLCOpenGLVideoViewEmbedding protocol.
 */
LIBVLC_API void libvlc_media_player_set_nsobject ( libvlc_media_player_t *p_mi, void * drawable );

/**
 * Get the NSView handler previously set with libvlc_media_player_set_nsobject().
 *
 * \param p_mi the Media Player
 * \return the NSView handler or 0 if none where set
 */
LIBVLC_API void * libvlc_media_player_get_nsobject ( libvlc_media_player_t *p_mi );

/**
 * Set the agl handler where the media player should render its video output.
 *
 * \param p_mi the Media Player
 * \param drawable the agl handler
 */
LIBVLC_API void libvlc_media_player_set_agl ( libvlc_media_player_t *p_mi, uint32_t drawable );

/**
 * Get the agl handler previously set with libvlc_media_player_set_agl().
 *
 * \param p_mi the Media Player
 * \return the agl handler or 0 if none where set
 */
LIBVLC_API uint32_t libvlc_media_player_get_agl ( libvlc_media_player_t *p_mi );

/**
 * Set an X Window System drawable where the media player should render its
 * video output. If LibVLC was built without X11 output support, then this has
 * no effects.
 *
 * The specified identifier must correspond to an existing Input/Output class
 * X11 window. Pixmaps are <b>not</b> supported. The caller shall ensure that
 * the X11 server is the same as the one the VLC instance has been configured
 * with. This function must be called before video playback is started;
 * otherwise it will only take effect after playback stop and restart.
 *
 * \param p_mi the Media Player
 * \param drawable the ID of the X window
 */
LIBVLC_API void libvlc_media_player_set_xwindow ( libvlc_media_player_t *p_mi, uint32_t drawable );

/**
 * Get the X Window System window identifier previously set with
 * libvlc_media_player_set_xwindow(). Note that this will return the identifier
 * even if VLC is not currently using it (for instance if it is playing an
 * audio-only input).
 *
 * \param p_mi the Media Player
 * \return an X window ID, or 0 if none where set.
 */
LIBVLC_API uint32_t libvlc_media_player_get_xwindow ( libvlc_media_player_t *p_mi );

/**
 * Set a Win32/Win64 API window handle (HWND) where the media player should
 * render its video output. If LibVLC was built without Win32/Win64 API output
 * support, then this has no effects.
 *
 * \param p_mi the Media Player
 * \param drawable windows handle of the drawable
 */
LIBVLC_API void libvlc_media_player_set_hwnd ( libvlc_media_player_t *p_mi, void *drawable );

/**
 * Get the Windows API window handle (HWND) previously set with
 * libvlc_media_player_set_hwnd(). The handle will be returned even if LibVLC
 * is not currently outputting any video to it.
 *
 * \param p_mi the Media Player
 * \return a window handle or NULL if there are none.
 */
LIBVLC_API void *libvlc_media_player_get_hwnd ( libvlc_media_player_t *p_mi );

/**
 * Callback prototype for audio playback.
 * \param data data pointer as passed to libvlc_audio_set_callbacks() [IN]
 * \param samples pointer to the first audio sample to play back [IN]
 * \param count number of audio samples to play back
 * \param pts expected play time stamp (see libvlc_delay())
 */
typedef void (*libvlc_audio_play_cb)(void *data, const void *samples,
                                     unsigned count, int64_t pts);

/**
 * Callback prototype for audio pause.
 * \note The pause callback is never called if the audio is already paused.
 * \param data data pointer as passed to libvlc_audio_set_callbacks() [IN]
 * \param pts time stamp of the pause request (should be elapsed already)
 */
typedef void (*libvlc_audio_pause_cb)(void *data, int64_t pts);

/**
 * Callback prototype for audio resumption (i.e. restart from pause).
 * \note The resume callback is never called if the audio is not paused.
 * \param data data pointer as passed to libvlc_audio_set_callbacks() [IN]
 * \param pts time stamp of the resumption request (should be elapsed already)
 */
typedef void (*libvlc_audio_resume_cb)(void *data, int64_t pts);

/**
 * Callback prototype for audio buffer flush
 * (i.e. discard all pending buffers and stop playback as soon as possible).
 * \param data data pointer as passed to libvlc_audio_set_callbacks() [IN]
 */
typedef void (*libvlc_audio_flush_cb)(void *data, int64_t pts);

/**
 * Callback prototype for audio buffer drain
 * (i.e. wait for pending buffers to be played).
 * \param data data pointer as passed to libvlc_audio_set_callbacks() [IN]
 */
typedef void (*libvlc_audio_drain_cb)(void *data);

/**
 * Callback prototype for audio volume change.
 * \param data data pointer as passed to libvlc_audio_set_callbacks() [IN]
 * \param volume software volume (1. = nominal, 0. = mute)
 * \param mute muted flag
 */
typedef void (*libvlc_audio_set_volume_cb)(void *data,
                                           float volume, bool mute);

/**
 * Set callbacks and private data for decoded audio.
 * Use libvlc_audio_set_format() or libvlc_audio_set_format_callbacks()
 * to configure the decoded audio format.
 *
 * \param mp the media player
 * \param play callback to play audio samples (must not be NULL)
 * \param pause callback to pause playback (or NULL to ignore)
 * \param resume callback to resume playback (or NULL to ignore)
 * \param flush callback to flush audio buffers (or NULL to ignore)
 * \param drain callback to drain audio buffers (or NULL to ignore)
 * \param opaque private pointer for the audio callbacks (as first parameter)
 * \version LibVLC 1.2.0 or later
 */
LIBVLC_API
void libvlc_audio_set_callbacks( libvlc_media_player_t *mp,
                                 libvlc_audio_play_cb play,
                                 libvlc_audio_pause_cb pause,
                                 libvlc_audio_resume_cb resume,
                                 libvlc_audio_flush_cb flush,
                                 libvlc_audio_drain_cb drain,
                                 void *opaque );

/**
 * Set callbacks and private data for decoded audio.
 * Use libvlc_audio_set_format() or libvlc_audio_set_format_callbacks()
 * to configure the decoded audio format.
 *
 * \param mp the media player
 * \param set_volume callback to apply audio volume,
 *                   or NULL to apply volume in software
 * \version LibVLC 1.2.0 or later
 */
LIBVLC_API
void libvlc_audio_set_volume_callback( libvlc_media_player_t *mp,
                                       libvlc_audio_set_volume_cb set_volume );

/**
 * Callback prototype to setup the audio playback.
 * This is called when the media player needs to create a new audio output.
 * \param opaque pointer to the data pointer passed to
 *               libvlc_audio_set_callbacks() [IN/OUT]
 * \param format 4 bytes sample format [IN/OUT]
 * \param rate sample rate [IN/OUT]
 * \param channels channels count [IN/OUT]
 * \return 0 on success, anything else to skip audio playback
 */
typedef int (*libvlc_audio_setup_cb)(void **data, char *format, unsigned *rate,
                                     unsigned *channels);

/**
 * Callback prototype for audio playback cleanup.
 * This is called when the media player no longer needs an audio output.
 * \param opaque data pointer as passed to libvlc_audio_set_callbacks() [IN]
 */
typedef void (*libvlc_audio_cleanup_cb)(void *data);

/**
 * Set decoded audio format. This only works in combination with
 * libvlc_audio_set_callbacks().
 *
 * \param mp the media player
 * \param setup callback to select the audio format (cannot be NULL)
 * \param cleanup callback to release any allocated resources (or NULL)
 * \version LibVLC 1.2.0 or later
 */
LIBVLC_API
void libvlc_audio_set_format_callbacks( libvlc_media_player_t *mp,
                                        libvlc_audio_setup_cb setup,
                                        libvlc_audio_cleanup_cb cleanup );

/**
 * Set decoded audio format.
 * This only works in combination with libvlc_audio_set_callbacks(),
 * and is mutually exclusive with libvlc_audio_set_format_callbacks().
 *
 * \param mp the media player
 * \param format a four-characters string identifying the sample format
 *               (e.g. "S16N" or "FL32")
 * \param rate sample rate (expressed in Hz)
 * \param channels channels count
 * \version LibVLC 1.2.0 or later
 */
LIBVLC_API
void libvlc_audio_set_format( libvlc_media_player_t *mp, const char *format,
                              unsigned rate, unsigned channels );

/** \bug This might go away ... to be replaced by a broader system */

/**
 * Get the current movie length (in ms).
 *
 * \param p_mi the Media Player
 * \return the movie length (in ms), or -1 if there is no media.
 */
LIBVLC_API libvlc_time_t libvlc_media_player_get_length( libvlc_media_player_t *p_mi );

/**
 * Get the current movie time (in ms).
 *
 * \param p_mi the Media Player
 * \return the movie time (in ms), or -1 if there is no media.
 */
LIBVLC_API libvlc_time_t libvlc_media_player_get_time( libvlc_media_player_t *p_mi );

/**
 * Set the movie time (in ms). This has no effect if no media is being played.
 * Not all formats and protocols support this.
 *
 * \param p_mi the Media Player
 * \param i_time the movie time (in ms).
 */
LIBVLC_API void libvlc_media_player_set_time( libvlc_media_player_t *p_mi, libvlc_time_t i_time );

/**
 * Get movie position.
 *
 * \param p_mi the Media Player
 * \return movie position, or -1. in case of error
 */
LIBVLC_API float libvlc_media_player_get_position( libvlc_media_player_t *p_mi );

/**
 * Set movie position. This has no effect if playback is not enabled.
 * This might not work depending on the underlying input format and protocol.
 *
 * \param p_mi the Media Player
 * \param f_pos the position
 */
LIBVLC_API void libvlc_media_player_set_position( libvlc_media_player_t *p_mi, float f_pos );

/**
 * Set movie chapter (if applicable).
 *
 * \param p_mi the Media Player
 * \param i_chapter chapter number to play
 */
LIBVLC_API void libvlc_media_player_set_chapter( libvlc_media_player_t *p_mi, int i_chapter );

/**
 * Get movie chapter.
 *
 * \param p_mi the Media Player
 * \return chapter number currently playing, or -1 if there is no media.
 */
LIBVLC_API int libvlc_media_player_get_chapter( libvlc_media_player_t *p_mi );

/**
 * Get movie chapter count
 *
 * \param p_mi the Media Player
 * \return number of chapters in movie, or -1.
 */
LIBVLC_API int libvlc_media_player_get_chapter_count( libvlc_media_player_t *p_mi );

/**
 * Is the player able to play
 *
 * \param p_mi the Media Player
 * \return boolean
 *
 * \libvlc_return_bool
 */
LIBVLC_API int libvlc_media_player_will_play( libvlc_media_player_t *p_mi );

/**
 * Get title chapter count
 *
 * \param p_mi the Media Player
 * \param i_title title
 * \return number of chapters in title, or -1
 */
LIBVLC_API int libvlc_media_player_get_chapter_count_for_title(
                       libvlc_media_player_t *p_mi, int i_title );

/**
 * Set movie title
 *
 * \param p_mi the Media Player
 * \param i_title title number to play
 */
LIBVLC_API void libvlc_media_player_set_title( libvlc_media_player_t *p_mi, int i_title );

/**
 * Get movie title
 *
 * \param p_mi the Media Player
 * \return title number currently playing, or -1
 */
LIBVLC_API int libvlc_media_player_get_title( libvlc_media_player_t *p_mi );

/**
 * Get movie title count
 *
 * \param p_mi the Media Player
 * \return title number count, or -1
 */
LIBVLC_API int libvlc_media_player_get_title_count( libvlc_media_player_t *p_mi );

/**
 * Set previous chapter (if applicable)
 *
 * \param p_mi the Media Player
 */
LIBVLC_API void libvlc_media_player_previous_chapter( libvlc_media_player_t *p_mi );

/**
 * Set next chapter (if applicable)
 *
 * \param p_mi the Media Player
 */
LIBVLC_API void libvlc_media_player_next_chapter( libvlc_media_player_t *p_mi );

/**
 * Get the requested movie play rate.
 * @warning Depending on the underlying media, the requested rate may be
 * different from the real playback rate.
 *
 * \param p_mi the Media Player
 * \return movie play rate
 */
LIBVLC_API float libvlc_media_player_get_rate( libvlc_media_player_t *p_mi );

/**
 * Set movie play rate
 *
 * \param p_mi the Media Player
 * \param rate movie play rate to set
 * \return -1 if an error was detected, 0 otherwise (but even then, it might
 * not actually work depending on the underlying media protocol)
 */
LIBVLC_API int libvlc_media_player_set_rate( libvlc_media_player_t *p_mi, float rate );

/**
 * Get current movie state
 *
 * \param p_mi the Media Player
 * \return the current state of the media player (playing, paused, ...) \see libvlc_state_t
 */
LIBVLC_API libvlc_state_t libvlc_media_player_get_state( libvlc_media_player_t *p_mi );

/**
 * Get movie fps rate
 *
 * \param p_mi the Media Player
 * \return frames per second (fps) for this playing movie, or 0 if unspecified
 */
LIBVLC_API float libvlc_media_player_get_fps( libvlc_media_player_t *p_mi );

/** end bug */

/**
 * How many video outputs does this media player have?
 *
 * \param p_mi the media player
 * \return the number of video outputs
 */
LIBVLC_API unsigned libvlc_media_player_has_vout( libvlc_media_player_t *p_mi );

/**
 * Is this media player seekable?
 *
 * \param p_mi the media player
 * \return true if the media player can seek
 *
 * \libvlc_return_bool
 */
LIBVLC_API int libvlc_media_player_is_seekable( libvlc_media_player_t *p_mi );

/**
 * Can this media player be paused?
 *
 * \param p_mi the media player
 * \return true if the media player can pause
 *
 * \libvlc_return_bool
 */
LIBVLC_API int libvlc_media_player_can_pause( libvlc_media_player_t *p_mi );


/**
 * Display the next frame (if supported)
 *
 * \param p_mi the media player
 */
LIBVLC_API void libvlc_media_player_next_frame( libvlc_media_player_t *p_mi );

/**
 * Navigate through DVD Menu
 *
 * \param p_mi the Media Player
 * \param navigate the Navigation mode
 * \version libVLC 1.2.0 or later
 */
LIBVLC_API void libvlc_media_player_navigate( libvlc_media_player_t* p_mi,
                                              unsigned navigate );

/**
 * Release (free) libvlc_track_description_t
 *
 * \param p_track_description the structure to release
 */
LIBVLC_API void libvlc_track_description_list_release( libvlc_track_description_t *p_track_description );

/**
 * \deprecated Use libvlc_track_description_list_release instead
 */
LIBVLC_DEPRECATED void libvlc_track_description_release( libvlc_track_description_t *p_track_description );

/** \defgroup libvlc_video LibVLC video controls
 * @{
 */

/**
 * Toggle fullscreen status on non-embedded video outputs.
 *
 * @warning The same limitations applies to this function
 * as to libvlc_set_fullscreen().
 *
 * \param p_mi the media player
 */
LIBVLC_API void libvlc_toggle_fullscreen( libvlc_media_player_t *p_mi );

/**
 * Enable or disable fullscreen.
 *
 * @warning With most window managers, only a top-level windows can be in
 * full-screen mode. Hence, this function will not operate properly if
 * libvlc_media_player_set_xwindow() was used to embed the video in a
 * non-top-level window. In that case, the embedding window must be reparented
 * to the root window <b>before</b> fullscreen mode is enabled. You will want
 * to reparent it back to its normal parent when disabling fullscreen.
 *
 * \param p_mi the media player
 * \param b_fullscreen boolean for fullscreen status
 */
LIBVLC_API void libvlc_set_fullscreen( libvlc_media_player_t *p_mi, int b_fullscreen );

/**
 * Get current fullscreen status.
 *
 * \param p_mi the media player
 * \return the fullscreen status (boolean)
 *
 * \libvlc_return_bool
 */
LIBVLC_API int libvlc_get_fullscreen( libvlc_media_player_t *p_mi );

/**
 * Enable or disable key press events handling, according to the LibVLC hotkeys
 * configuration. By default and for historical reasons, keyboard events are
 * handled by the LibVLC video widget.
 *
 * \note On X11, there can be only one subscriber for key press and mouse
 * click events per window. If your application has subscribed to those events
 * for the X window ID of the video widget, then LibVLC will not be able to
 * handle key presses and mouse clicks in any case.
 *
 * \warning This function is only implemented for X11 and Win32 at the moment.
 *
 * \param p_mi the media player
 * \param on true to handle key press events, false to ignore them.
 */
LIBVLC_API
void libvlc_video_set_key_input( libvlc_media_player_t *p_mi, unsigned on );

/**
 * Enable or disable mouse click events handling. By default, those events are
 * handled. This is needed for DVD menus to work, as well as a few video
 * filters such as "puzzle".
 *
 * \see libvlc_video_set_key_input().
 *
 * \warning This function is only implemented for X11 and Win32 at the moment.
 *
 * \param p_mi the media player
 * \param on true to handle mouse click events, false to ignore them.
 */
LIBVLC_API
void libvlc_video_set_mouse_input( libvlc_media_player_t *p_mi, unsigned on );

/**
 * Get the pixel dimensions of a video.
 *
 * \param p_mi media player
 * \param num number of the video (starting from, and most commonly 0)
 * \param px pointer to get the pixel width [OUT]
 * \param py pointer to get the pixel height [OUT]
 * \return 0 on success, -1 if the specified video does not exist
 */
LIBVLC_API
int libvlc_video_get_size( libvlc_media_player_t *p_mi, unsigned num,
                           unsigned *px, unsigned *py );

/**
 * Get current video height.
 * \deprecated Use libvlc_video_get_size() instead.
 *
 * \param p_mi the media player
 * \return the video pixel height or 0 if not applicable
 */
LIBVLC_DEPRECATED
int libvlc_video_get_height( libvlc_media_player_t *p_mi );

/**
 * Get current video width.
 * \deprecated Use libvlc_video_get_size() instead.
 *
 * \param p_mi the media player
 * \return the video pixel width or 0 if not applicable
 */
LIBVLC_DEPRECATED
int libvlc_video_get_width( libvlc_media_player_t *p_mi );

/**
 * Get the mouse pointer coordinates over a video.
 * Coordinates are expressed in terms of the decoded video resolution,
 * <b>not</b> in terms of pixels on the screen/viewport (to get the latter,
 * you can query your windowing system directly).
 *
 * Either of the coordinates may be negative or larger than the corresponding
 * dimension of the video, if the cursor is outside the rendering area.
 *
 * @warning The coordinates may be out-of-date if the pointer is not located
 * on the video rendering area. LibVLC does not track the pointer if it is
 * outside of the video widget.
 *
 * @note LibVLC does not support multiple pointers (it does of course support
 * multiple input devices sharing the same pointer) at the moment.
 *
 * \param p_mi media player
 * \param num number of the video (starting from, and most commonly 0)
 * \param px pointer to get the abscissa [OUT]
 * \param py pointer to get the ordinate [OUT]
 * \return 0 on success, -1 if the specified video does not exist
 */
LIBVLC_API
int libvlc_video_get_cursor( libvlc_media_player_t *p_mi, unsigned num,
                             int *px, int *py );

/**
 * Get the current video scaling factor.
 * See also libvlc_video_set_scale().
 *
 * \param p_mi the media player
 * \return the currently configured zoom factor, or 0. if the video is set
 * to fit to the output window/drawable automatically.
 */
LIBVLC_API float libvlc_video_get_scale( libvlc_media_player_t *p_mi );

/**
 * Set the video scaling factor. That is the ratio of the number of pixels on
 * screen to the number of pixels in the original decoded video in each
 * dimension. Zero is a special value; it will adjust the video to the output
 * window/drawable (in windowed mode) or the entire screen.
 *
 * Note that not all video outputs support scaling.
 *
 * \param p_mi the media player
 * \param f_factor the scaling factor, or zero
 */
LIBVLC_API void libvlc_video_set_scale( libvlc_media_player_t *p_mi, float f_factor );

/**
 * Get current video aspect ratio.
 *
 * \param p_mi the media player
 * \return the video aspect ratio or NULL if unspecified
 * (the result must be released with free() or libvlc_free()).
 */
LIBVLC_API char *libvlc_video_get_aspect_ratio( libvlc_media_player_t *p_mi );

/**
 * Set new video aspect ratio.
 *
 * \param p_mi the media player
 * \param psz_aspect new video aspect-ratio or NULL to reset to default
 * \note Invalid aspect ratios are ignored.
 */
LIBVLC_API void libvlc_video_set_aspect_ratio( libvlc_media_player_t *p_mi, const char *psz_aspect );

/**
 * Get current video subtitle.
 *
 * \param p_mi the media player
 * \return the video subtitle selected, or -1 if none
 */
LIBVLC_API int libvlc_video_get_spu( libvlc_media_player_t *p_mi );

/**
 * Get the number of available video subtitles.
 *
 * \param p_mi the media player
 * \return the number of available video subtitles
 */
LIBVLC_API int libvlc_video_get_spu_count( libvlc_media_player_t *p_mi );

/**
 * Get the description of available video subtitles.
 *
 * \param p_mi the media player
 * \return list containing description of available video subtitles
 */
LIBVLC_API libvlc_track_description_t *
        libvlc_video_get_spu_description( libvlc_media_player_t *p_mi );

/**
 * Set new video subtitle.
 *
 * \param p_mi the media player
 * \param i_spu new video subtitle to select
 * \return 0 on success, -1 if out of range
 */
LIBVLC_API int libvlc_video_set_spu( libvlc_media_player_t *p_mi, unsigned i_spu );

/**
 * Set new video subtitle file.
 *
 * \param p_mi the media player
 * \param psz_subtitle new video subtitle file
 * \return the success status (boolean)
 */
LIBVLC_API int libvlc_video_set_subtitle_file( libvlc_media_player_t *p_mi, const char *psz_subtitle );

/**
 * Get the current subtitle delay. Positive values means subtitles are being
 * displayed later, negative values earlier.
 *
 * \param p_mi media player
 * \return time (in microseconds) the display of subtitles is being delayed
 * \version LibVLC 1.2.0 or later
 */
LIBVLC_API int64_t libvlc_video_get_spu_delay( libvlc_media_player_t *p_mi );

/**
 * Set the subtitle delay. This affects the timing of when the subtitle will
 * be displayed. Positive values result in subtitles being displayed later,
 * while negative values will result in subtitles being displayed earlier.
 *
 * The subtitle delay will be reset to zero each time the media changes.
 *
 * \param p_mi media player
 * \param i_delay time (in microseconds) the display of subtitles should be delayed
 * \return 0 on success, -1 on error
 * \version LibVLC 1.2.0 or later
 */
LIBVLC_API int libvlc_video_set_spu_delay( libvlc_media_player_t *p_mi, int64_t i_delay );

/**
 * Get the description of available titles.
 *
 * \param p_mi the media player
 * \return list containing description of available titles
 */
LIBVLC_API libvlc_track_description_t *
        libvlc_video_get_title_description( libvlc_media_player_t *p_mi );

/**
 * Get the description of available chapters for specific title.
 *
 * \param p_mi the media player
 * \param i_title selected title
 * \return list containing description of available chapter for title i_title
 */
LIBVLC_API libvlc_track_description_t *
        libvlc_video_get_chapter_description( libvlc_media_player_t *p_mi, int i_title );

/**
 * Get current crop filter geometry.
 *
 * \param p_mi the media player
 * \return the crop filter geometry or NULL if unset
 */
LIBVLC_API char *libvlc_video_get_crop_geometry( libvlc_media_player_t *p_mi );

/**
 * Set new crop filter geometry.
 *
 * \param p_mi the media player
 * \param psz_geometry new crop filter geometry (NULL to unset)
 */
LIBVLC_API
void libvlc_video_set_crop_geometry( libvlc_media_player_t *p_mi, const char *psz_geometry );

/**
 * Get current teletext page requested.
 *
 * \param p_mi the media player
 * \return the current teletext page requested.
 */
LIBVLC_API int libvlc_video_get_teletext( libvlc_media_player_t *p_mi );

/**
 * Set new teletext page to retrieve.
 *
 * \param p_mi the media player
 * \param i_page teletex page number requested
 */
LIBVLC_API void libvlc_video_set_teletext( libvlc_media_player_t *p_mi, int i_page );

/**
 * Toggle teletext transparent status on video output.
 *
 * \param p_mi the media player
 */
LIBVLC_API void libvlc_toggle_teletext( libvlc_media_player_t *p_mi );

/**
 * Get number of available video tracks.
 *
 * \param p_mi media player
 * \return the number of available video tracks (int)
 */
LIBVLC_API int libvlc_video_get_track_count( libvlc_media_player_t *p_mi );

/**
 * Get the description of available video tracks.
 *
 * \param p_mi media player
 * \return list with description of available video tracks, or NULL on error
 */
LIBVLC_API libvlc_track_description_t *
        libvlc_video_get_track_description( libvlc_media_player_t *p_mi );

/**
 * Get current video track.
 *
 * \param p_mi media player
 * \return the video track (int) or -1 if none
 */
LIBVLC_API int libvlc_video_get_track( libvlc_media_player_t *p_mi );

/**
 * Set video track.
 *
 * \param p_mi media player
 * \param i_track the track (int)
 * \return 0 on success, -1 if out of range
 */
LIBVLC_API
int libvlc_video_set_track( libvlc_media_player_t *p_mi, int i_track );

/**
 * Take a snapshot of the current video window.
 *
 * If i_width AND i_height is 0, original size is used.
 * If i_width XOR i_height is 0, original aspect-ratio is preserved.
 *
 * \param p_mi media player instance
 * \param num number of video output (typically 0 for the first/only one)
 * \param psz_filepath the path where to save the screenshot to
 * \param i_width the snapshot's width
 * \param i_height the snapshot's height
 * \return 0 on success, -1 if the video was not found
 */
LIBVLC_API
int libvlc_video_take_snapshot( libvlc_media_player_t *p_mi, unsigned num,
                                const char *psz_filepath, unsigned int i_width,
                                unsigned int i_height );

/**
 * Enable or disable deinterlace filter
 *
 * \param p_mi libvlc media player
 * \param psz_mode type of deinterlace filter, NULL to disable
 */
LIBVLC_API void libvlc_video_set_deinterlace( libvlc_media_player_t *p_mi,
                                                  const char *psz_mode );

/**
 * Get an integer marquee option value
 *
 * \param p_mi libvlc media player
 * \param option marq option to get \see libvlc_video_marquee_int_option_t
 */
LIBVLC_API int libvlc_video_get_marquee_int( libvlc_media_player_t *p_mi,
                                                 unsigned option );

/**
 * Get a string marquee option value
 *
 * \param p_mi libvlc media player
 * \param option marq option to get \see libvlc_video_marquee_string_option_t
 */
LIBVLC_API char *libvlc_video_get_marquee_string( libvlc_media_player_t *p_mi,
                                                      unsigned option );

/**
 * Enable, disable or set an integer marquee option
 *
 * Setting libvlc_marquee_Enable has the side effect of enabling (arg !0)
 * or disabling (arg 0) the marq filter.
 *
 * \param p_mi libvlc media player
 * \param option marq option to set \see libvlc_video_marquee_int_option_t
 * \param i_val marq option value
 */
LIBVLC_API void libvlc_video_set_marquee_int( libvlc_media_player_t *p_mi,
                                                  unsigned option, int i_val );

/**
 * Set a marquee string option
 *
 * \param p_mi libvlc media player
 * \param option marq option to set \see libvlc_video_marquee_string_option_t
 * \param psz_text marq option value
 */
LIBVLC_API void libvlc_video_set_marquee_string( libvlc_media_player_t *p_mi,
                                                     unsigned option, const char *psz_text );

/** option values for libvlc_video_{get,set}_logo_{int,string} */
enum libvlc_video_logo_option_t {
    libvlc_logo_enable,
    libvlc_logo_file,           /**< string argument, "file,d,t;file,d,t;..." */
    libvlc_logo_x,
    libvlc_logo_y,
    libvlc_logo_delay,
    libvlc_logo_repeat,
    libvlc_logo_opacity,
    libvlc_logo_position
};

/**
 * Get integer logo option.
 *
 * \param p_mi libvlc media player instance
 * \param option logo option to get, values of libvlc_video_logo_option_t
 */
LIBVLC_API int libvlc_video_get_logo_int( libvlc_media_player_t *p_mi,
                                              unsigned option );

/**
 * Set logo option as integer. Options that take a different type value
 * are ignored.
 * Passing libvlc_logo_enable as option value has the side effect of
 * starting (arg !0) or stopping (arg 0) the logo filter.
 *
 * \param p_mi libvlc media player instance
 * \param option logo option to set, values of libvlc_video_logo_option_t
 * \param value logo option value
 */
LIBVLC_API void libvlc_video_set_logo_int( libvlc_media_player_t *p_mi,
                                               unsigned option, int value );

/**
 * Set logo option as string. Options that take a different type value
 * are ignored.
 *
 * \param p_mi libvlc media player instance
 * \param option logo option to set, values of libvlc_video_logo_option_t
 * \param psz_value logo option value
 */
LIBVLC_API void libvlc_video_set_logo_string( libvlc_media_player_t *p_mi,
                                      unsigned option, const char *psz_value );


/** option values for libvlc_video_{get,set}_adjust_{int,float,bool} */
enum libvlc_video_adjust_option_t {
    libvlc_adjust_Enable = 0,
    libvlc_adjust_Contrast,
    libvlc_adjust_Brightness,
    libvlc_adjust_Hue,
    libvlc_adjust_Saturation,
    libvlc_adjust_Gamma
};

/**
 * Get integer adjust option.
 *
 * \param p_mi libvlc media player instance
 * \param option adjust option to get, values of libvlc_video_adjust_option_t
 * \version LibVLC 1.1.1 and later.
 */
LIBVLC_API int libvlc_video_get_adjust_int( libvlc_media_player_t *p_mi,
                                                unsigned option );

/**
 * Set adjust option as integer. Options that take a different type value
 * are ignored.
 * Passing libvlc_adjust_enable as option value has the side effect of
 * starting (arg !0) or stopping (arg 0) the adjust filter.
 *
 * \param p_mi libvlc media player instance
 * \param option adust option to set, values of libvlc_video_adjust_option_t
 * \param value adjust option value
 * \version LibVLC 1.1.1 and later.
 */
LIBVLC_API void libvlc_video_set_adjust_int( libvlc_media_player_t *p_mi,
                                                 unsigned option, int value );

/**
 * Get float adjust option.
 *
 * \param p_mi libvlc media player instance
 * \param option adjust option to get, values of libvlc_video_adjust_option_t
 * \version LibVLC 1.1.1 and later.
 */
LIBVLC_API float libvlc_video_get_adjust_float( libvlc_media_player_t *p_mi,
                                                    unsigned option );

/**
 * Set adjust option as float. Options that take a different type value
 * are ignored.
 *
 * \param p_mi libvlc media player instance
 * \param option adust option to set, values of libvlc_video_adjust_option_t
 * \param value adjust option value
 * \version LibVLC 1.1.1 and later.
 */
LIBVLC_API void libvlc_video_set_adjust_float( libvlc_media_player_t *p_mi,
                                                   unsigned option, float value );

/** @} video */

/** \defgroup libvlc_audio LibVLC audio controls
 * @{
 */

/**
 * Audio device types
 */
typedef enum libvlc_audio_output_device_types_t {
    libvlc_AudioOutputDevice_Error  = -1,
    libvlc_AudioOutputDevice_Mono   =  1,
    libvlc_AudioOutputDevice_Stereo =  2,
    libvlc_AudioOutputDevice_2F2R   =  4,
    libvlc_AudioOutputDevice_3F2R   =  5,
    libvlc_AudioOutputDevice_5_1    =  6,
    libvlc_AudioOutputDevice_6_1    =  7,
    libvlc_AudioOutputDevice_7_1    =  8,
    libvlc_AudioOutputDevice_SPDIF  = 10
} libvlc_audio_output_device_types_t;

/**
 * Audio channels
 */
typedef enum libvlc_audio_output_channel_t {
    libvlc_AudioChannel_Error   = -1,
    libvlc_AudioChannel_Stereo  =  1,
    libvlc_AudioChannel_RStereo =  2,
    libvlc_AudioChannel_Left    =  3,
    libvlc_AudioChannel_Right   =  4,
    libvlc_AudioChannel_Dolbys  =  5
} libvlc_audio_output_channel_t;


/**
 * Get the list of available audio outputs
 *
 * \param p_instance libvlc instance
 * \return list of available audio outputs. It must be freed it with
*          \see libvlc_audio_output_list_release \see libvlc_audio_output_t .
 *         In case of error, NULL is returned.
 */
LIBVLC_API libvlc_audio_output_t *
        libvlc_audio_output_list_get( libvlc_instance_t *p_instance );

/**
 * Free the list of available audio outputs
 *
 * \param p_list list with audio outputs for release
 */
LIBVLC_API void libvlc_audio_output_list_release( libvlc_audio_output_t *p_list );

/**
 * Set the audio output.
 * Change will be applied after stop and play.
 *
 * \param p_mi media player
 * \param psz_name name of audio output,
 *               use psz_name of \see libvlc_audio_output_t
 * \return 0 if function succeded, -1 on error
 */
LIBVLC_API int libvlc_audio_output_set( libvlc_media_player_t *p_mi,
                                            const char *psz_name );

/**
 * Get count of devices for audio output, these devices are hardware oriented
 * like analor or digital output of sound card
 *
 * \param p_instance libvlc instance
 * \param psz_audio_output - name of audio output, \see libvlc_audio_output_t
 * \return number of devices
 */
LIBVLC_API int libvlc_audio_output_device_count( libvlc_instance_t *p_instance,
                                                     const char *psz_audio_output );

/**
 * Get long name of device, if not available short name given
 *
 * \param p_instance libvlc instance
 * \param psz_audio_output - name of audio output, \see libvlc_audio_output_t
 * \param i_device device index
 * \return long name of device
 */
LIBVLC_API char * libvlc_audio_output_device_longname( libvlc_instance_t *p_instance,
                                                           const char *psz_audio_output,
                                                           int i_device );

/**
 * Get id name of device
 *
 * \param p_instance libvlc instance
 * \param psz_audio_output - name of audio output, \see libvlc_audio_output_t
 * \param i_device device index
 * \return id name of device, use for setting device, need to be free after use
 */
LIBVLC_API char * libvlc_audio_output_device_id( libvlc_instance_t *p_instance,
                                                     const char *psz_audio_output,
                                                     int i_device );

/**
 * Set audio output device. Changes are only effective after stop and play.
 *
 * \param p_mi media player
 * \param psz_audio_output - name of audio output, \see libvlc_audio_output_t
 * \param psz_device_id device
 */
LIBVLC_API void libvlc_audio_output_device_set( libvlc_media_player_t *p_mi,
                                                    const char *psz_audio_output,
                                                    const char *psz_device_id );

/**
 * Get current audio device type. Device type describes something like
 * character of output sound - stereo sound, 2.1, 5.1 etc
 *
 * \param p_mi media player
 * \return the audio devices type \see libvlc_audio_output_device_types_t
 */
LIBVLC_API int libvlc_audio_output_get_device_type( libvlc_media_player_t *p_mi );

/**
 * Set current audio device type.
 *
 * \param p_mi vlc instance
 * \param device_type the audio device type,
          according to \see libvlc_audio_output_device_types_t
 */
LIBVLC_API void libvlc_audio_output_set_device_type( libvlc_media_player_t *p_mi,
                                                         int device_type );


/**
 * Toggle mute status.
 *
 * \param p_mi media player
 */
LIBVLC_API void libvlc_audio_toggle_mute( libvlc_media_player_t *p_mi );

/**
 * Get current mute status.
 *
 * \param p_mi media player
 * \return the mute status (boolean)
 *
 * \libvlc_return_bool
 */
LIBVLC_API int libvlc_audio_get_mute( libvlc_media_player_t *p_mi );

/**
 * Set mute status.
 *
 * \param p_mi media player
 * \param status If status is true then mute, otherwise unmute
 */
LIBVLC_API void libvlc_audio_set_mute( libvlc_media_player_t *p_mi, int status );

/**
 * Get current software audio volume.
 *
 * \param p_mi media player
 * \return the software volume in percents
 * (0 = mute, 100 = nominal / 0dB)
 */
LIBVLC_API int libvlc_audio_get_volume( libvlc_media_player_t *p_mi );

/**
 * Set current software audio volume.
 *
 * \param p_mi media player
 * \param i_volume the volume in percents (0 = mute, 100 = 0dB)
 * \return 0 if the volume was set, -1 if it was out of range
 */
LIBVLC_API int libvlc_audio_set_volume( libvlc_media_player_t *p_mi, int i_volume );

/**
 * Get number of available audio tracks.
 *
 * \param p_mi media player
 * \return the number of available audio tracks (int), or -1 if unavailable
 */
LIBVLC_API int libvlc_audio_get_track_count( libvlc_media_player_t *p_mi );

/**
 * Get the description of available audio tracks.
 *
 * \param p_mi media player
 * \return list with description of available audio tracks, or NULL
 */
LIBVLC_API libvlc_track_description_t *
        libvlc_audio_get_track_description( libvlc_media_player_t *p_mi );

/**
 * Get current audio track.
 *
 * \param p_mi media player
 * \return the audio track (int), or -1 if none.
 */
LIBVLC_API int libvlc_audio_get_track( libvlc_media_player_t *p_mi );

/**
 * Set current audio track.
 *
 * \param p_mi media player
 * \param i_track the track (int)
 * \return 0 on success, -1 on error
 */
LIBVLC_API int libvlc_audio_set_track( libvlc_media_player_t *p_mi, int i_track );

/**
 * Get current audio channel.
 *
 * \param p_mi media player
 * \return the audio channel \see libvlc_audio_output_channel_t
 */
LIBVLC_API int libvlc_audio_get_channel( libvlc_media_player_t *p_mi );

/**
 * Set current audio channel.
 *
 * \param p_mi media player
 * \param channel the audio channel, \see libvlc_audio_output_channel_t
 * \return 0 on success, -1 on error
 */
LIBVLC_API int libvlc_audio_set_channel( libvlc_media_player_t *p_mi, int channel );

/**
 * Get current audio delay.
 *
 * \param p_mi media player
 * \return the audio delay (microseconds)
 * \version LibVLC 1.1.1 or later
 */
LIBVLC_API int64_t libvlc_audio_get_delay( libvlc_media_player_t *p_mi );

/**
 * Set current audio delay. The audio delay will be reset to zero each time the media changes.
 *
 * \param p_mi media player
 * \param i_delay the audio delay (microseconds)
 * \return 0 on success, -1 on error
 * \version LibVLC 1.1.1 or later
 */
LIBVLC_API int libvlc_audio_set_delay( libvlc_media_player_t *p_mi, int64_t i_delay );

/** @} audio */

/** @} media_player */

# ifdef __cplusplus
}
# endif

#endif /* VLC_LIBVLC_MEDIA_PLAYER_H */
