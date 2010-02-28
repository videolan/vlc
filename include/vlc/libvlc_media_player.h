/*****************************************************************************
 * libvlc_media_player.h:  libvlc_media_player external API
 *****************************************************************************
 * Copyright (C) 1998-2010 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Paul Saman <jpsaman@videolan.org>
 *          Pierre d'Herbemont <pdherbemont@videolan.org>
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

/**
 * \file
 * This file defines libvlc_media_player external API
 */

#ifndef VLC_LIBVLC_MEDIA_PLAYER_H
#define VLC_LIBVLC_MEDIA_PLAYER_H 1

# ifdef __cplusplus
extern "C" {
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
    libvlc_marquee_Text,		/** string argument */
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
 * Create an empty Media Player object
 *
 * \param p_libvlc_instance the libvlc instance in which the Media Player
 *        should be created.
 * \return a new media player object, or NULL on error.
 */
VLC_PUBLIC_API libvlc_media_player_t * libvlc_media_player_new( libvlc_instance_t *p_libvlc_instance );

/**
 * Create a Media Player object from a Media
 *
 * \param p_md the media. Afterwards the p_md can be safely
 *        destroyed.
 * \return a new media player object, or NULL on error.
 */
VLC_PUBLIC_API libvlc_media_player_t * libvlc_media_player_new_from_media( libvlc_media_t *p_md );

/**
 * Release a media_player after use
 * Decrement the reference count of a media player object. If the
 * reference count is 0, then libvlc_media_player_release() will
 * release the media player object. If the media player object
 * has been released, then it should not be used again.
 *
 * \param p_mi the Media Player to free
 */
VLC_PUBLIC_API void libvlc_media_player_release( libvlc_media_player_t *p_mi );

/**
 * Retain a reference to a media player object. Use
 * libvlc_media_player_release() to decrement reference count.
 *
 * \param p_mi media player object
 */
VLC_PUBLIC_API void libvlc_media_player_retain( libvlc_media_player_t *p_mi );

/**
 * Set the media that will be used by the media_player. If any,
 * previous md will be released.
 *
 * \param p_mi the Media Player
 * \param p_md the Media. Afterwards the p_md can be safely
 *        destroyed.
 */
VLC_PUBLIC_API void libvlc_media_player_set_media( libvlc_media_player_t *p_mi,
                                                   libvlc_media_t *p_md );

/**
 * Get the media used by the media_player.
 *
 * \param p_mi the Media Player
 * \return the media associated with p_mi, or NULL if no
 *         media is associated
 */
VLC_PUBLIC_API libvlc_media_t * libvlc_media_player_get_media( libvlc_media_player_t *p_mi );

/**
 * Get the Event Manager from which the media player send event.
 *
 * \param p_mi the Media Player
 * \return the event manager associated with p_mi
 */
VLC_PUBLIC_API libvlc_event_manager_t * libvlc_media_player_event_manager ( libvlc_media_player_t *p_mi );

/**
 * is_playing
 *
 * \param p_mi the Media Player
 * \return 1 if the media player is playing, 0 otherwise
 */
VLC_PUBLIC_API int libvlc_media_player_is_playing ( libvlc_media_player_t *p_mi );

/**
 * Play
 *
 * \param p_mi the Media Player
 * \return 0 if playback started (and was already started), or -1 on error.
 */
VLC_PUBLIC_API int libvlc_media_player_play ( libvlc_media_player_t *p_mi );

/**
 * Toggle pause (no effect if there is no media)
 *
 * \param p_mi the Media Player
 */
VLC_PUBLIC_API void libvlc_media_player_pause ( libvlc_media_player_t *p_mi );

/**
 * Stop (no effect if there is no media)
 *
 * \param p_mi the Media Player
 */
VLC_PUBLIC_API void libvlc_media_player_stop ( libvlc_media_player_t *p_mi );

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
VLC_PUBLIC_API void libvlc_media_player_set_nsobject ( libvlc_media_player_t *p_mi, void * drawable );

/**
 * Get the NSView handler previously set with libvlc_media_player_set_nsobject().
 *
 * \param p_mi the Media Player
 * \return the NSView handler or 0 if none where set
 */
VLC_PUBLIC_API void * libvlc_media_player_get_nsobject ( libvlc_media_player_t *p_mi );

/**
 * Set the agl handler where the media player should render its video output.
 *
 * \param p_mi the Media Player
 * \param drawable the agl handler
 */
VLC_PUBLIC_API void libvlc_media_player_set_agl ( libvlc_media_player_t *p_mi, uint32_t drawable );

/**
 * Get the agl handler previously set with libvlc_media_player_set_agl().
 *
 * \param p_mi the Media Player
 * \return the agl handler or 0 if none where set
 */
VLC_PUBLIC_API uint32_t libvlc_media_player_get_agl ( libvlc_media_player_t *p_mi );

/**
 * Set an X Window System drawable where the media player should render its
 * video output. If LibVLC was built without X11 output support, then this has
 * no effects.
 *
 * The specified identifier must correspond to an existing Input/Output class
 * X11 window. Pixmaps are <b>not</b> supported. The caller shall ensure that
 * the X11 server is the same as the one the VLC instance has been configured
 * with.
 * If XVideo is <b>not</b> used, it is assumed that the drawable has the
 * following properties in common with the default X11 screen: depth, scan line
 * pad, black pixel. This is a bug.
 *
 * \param p_mi the Media Player
 * \param drawable the ID of the X window
 */
VLC_PUBLIC_API void libvlc_media_player_set_xwindow ( libvlc_media_player_t *p_mi, uint32_t drawable );

/**
 * Get the X Window System window identifier previously set with
 * libvlc_media_player_set_xwindow(). Note that this will return the identifier
 * even if VLC is not currently using it (for instance if it is playing an
 * audio-only input).
 *
 * \param p_mi the Media Player
 * \return an X window ID, or 0 if none where set.
 */
VLC_PUBLIC_API uint32_t libvlc_media_player_get_xwindow ( libvlc_media_player_t *p_mi );

/**
 * Set a Win32/Win64 API window handle (HWND) where the media player should
 * render its video output. If LibVLC was built without Win32/Win64 API output
 * support, then this has no effects.
 *
 * \param p_mi the Media Player
 * \param drawable windows handle of the drawable
 */
VLC_PUBLIC_API void libvlc_media_player_set_hwnd ( libvlc_media_player_t *p_mi, void *drawable );

/**
 * Get the Windows API window handle (HWND) previously set with
 * libvlc_media_player_set_hwnd(). The handle will be returned even if LibVLC
 * is not currently outputting any video to it.
 *
 * \param p_mi the Media Player
 * \return a window handle or NULL if there are none.
 */
VLC_PUBLIC_API void *libvlc_media_player_get_hwnd ( libvlc_media_player_t *p_mi );



/** \bug This might go away ... to be replaced by a broader system */

/**
 * Get the current movie length (in ms).
 *
 * \param p_mi the Media Player
 * \return the movie length (in ms), or -1 if there is no media.
 */
VLC_PUBLIC_API libvlc_time_t libvlc_media_player_get_length( libvlc_media_player_t *p_mi );

/**
 * Get the current movie time (in ms).
 *
 * \param p_mi the Media Player
 * \return the movie time (in ms), or -1 if there is no media.
 */
VLC_PUBLIC_API libvlc_time_t libvlc_media_player_get_time( libvlc_media_player_t *p_mi );

/**
 * Set the movie time (in ms). This has no effect if no media is being played.
 * Not all formats and protocols support this.
 *
 * \param p_mi the Media Player
 * \param i_time the movie time (in ms).
 */
VLC_PUBLIC_API void libvlc_media_player_set_time( libvlc_media_player_t *p_mi, libvlc_time_t i_time );

/**
 * Get movie position.
 *
 * \param p_mi the Media Player
 * \return movie position, or -1. in case of error
 */
VLC_PUBLIC_API float libvlc_media_player_get_position( libvlc_media_player_t *p_mi );

/**
 * Set movie position. This has no effect if playback is not enabled.
 * This might not work depending on the underlying input format and protocol.
 *
 * \param p_mi the Media Player
 * \param f_pos the position
 */
VLC_PUBLIC_API void libvlc_media_player_set_position( libvlc_media_player_t *p_mi, float f_pos );

/**
 * Set movie chapter (if applicable).
 *
 * \param p_mi the Media Player
 * \param i_chapter chapter number to play
 */
VLC_PUBLIC_API void libvlc_media_player_set_chapter( libvlc_media_player_t *p_mi, int i_chapter );

/**
 * Get movie chapter.
 *
 * \param p_mi the Media Player
 * \return chapter number currently playing, or -1 if there is no media.
 */
VLC_PUBLIC_API int libvlc_media_player_get_chapter( libvlc_media_player_t *p_mi );

/**
 * Get movie chapter count
 *
 * \param p_mi the Media Player
 * \return number of chapters in movie, or -1.
 */
VLC_PUBLIC_API int libvlc_media_player_get_chapter_count( libvlc_media_player_t *p_mi );

/**
 * Is the player able to play
 *
 * \param p_mi the Media Player
 * \return boolean
 */
VLC_PUBLIC_API int libvlc_media_player_will_play( libvlc_media_player_t *p_mi );

/**
 * Get title chapter count
 *
 * \param p_mi the Media Player
 * \param i_title title
 * \return number of chapters in title, or -1
 */
VLC_PUBLIC_API int libvlc_media_player_get_chapter_count_for_title(
                       libvlc_media_player_t *p_mi, int i_title );

/**
 * Set movie title
 *
 * \param p_mi the Media Player
 * \param i_title title number to play
 */
VLC_PUBLIC_API void libvlc_media_player_set_title( libvlc_media_player_t *p_mi, int i_title );

/**
 * Get movie title
 *
 * \param p_mi the Media Player
 * \return title number currently playing, or -1
 */
VLC_PUBLIC_API int libvlc_media_player_get_title( libvlc_media_player_t *p_mi );

/**
 * Get movie title count
 *
 * \param p_mi the Media Player
 * \return title number count, or -1
 */
VLC_PUBLIC_API int libvlc_media_player_get_title_count( libvlc_media_player_t *p_mi );

/**
 * Set previous chapter (if applicable)
 *
 * \param p_mi the Media Player
 */
VLC_PUBLIC_API void libvlc_media_player_previous_chapter( libvlc_media_player_t *p_mi );

/**
 * Set next chapter (if applicable)
 *
 * \param p_mi the Media Player
 */
VLC_PUBLIC_API void libvlc_media_player_next_chapter( libvlc_media_player_t *p_mi );

/**
 * Get the requested movie play rate.
 * @warning Depending on the underlying media, the requested rate may be
 * different from the real playback rate.
 *
 * \param p_mi the Media Player
 * \return movie play rate
 */
VLC_PUBLIC_API float libvlc_media_player_get_rate( libvlc_media_player_t *p_mi );

/**
 * Set movie play rate
 *
 * \param p_mi the Media Player
 * \param rate movie play rate to set
 * \return -1 if an error was detected, 0 otherwise (but even then, it might
 * not actually work depending on the underlying media protocol)
 */
VLC_PUBLIC_API int libvlc_media_player_set_rate( libvlc_media_player_t *p_mi, float rate );

/**
 * Get current movie state
 *
 * \param p_mi the Media Player
 * \return the current state of the media player (playing, paused, ...) \see libvlc_state_t
 */
VLC_PUBLIC_API libvlc_state_t libvlc_media_player_get_state( libvlc_media_player_t *p_mi );

/**
 * Get movie fps rate
 *
 * \param p_mi the Media Player
 * \return frames per second (fps) for this playing movie, or 0 if unspecified
 */
VLC_PUBLIC_API float libvlc_media_player_get_fps( libvlc_media_player_t *p_mi );

/** end bug */

/**
 * How many video outputs does this media player have?
 *
 * \param p_mi the media player
 * \return the number of video outputs
 */
VLC_PUBLIC_API unsigned libvlc_media_player_has_vout( libvlc_media_player_t *p_mi );

/**
 * Is this media player seekable?
 *
 * \param p_mi the media player
 * \return true if the media player can seek
 */
VLC_PUBLIC_API int libvlc_media_player_is_seekable( libvlc_media_player_t *p_mi );

/**
 * Can this media player be paused?
 *
 * \param p_mi the media player
 * \return true if the media player can pause
 */
VLC_PUBLIC_API int libvlc_media_player_can_pause( libvlc_media_player_t *p_mi );


/**
 * Display the next frame (if supported)
 *
 * \param p_mi the media player
 */
VLC_PUBLIC_API void libvlc_media_player_next_frame( libvlc_media_player_t *p_mi );



/**
 * Release (free) libvlc_track_description_t
 *
 * \param p_track_description the structure to release
 */
VLC_PUBLIC_API void libvlc_track_description_release( libvlc_track_description_t *p_track_description );

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
VLC_PUBLIC_API void libvlc_toggle_fullscreen( libvlc_media_player_t *p_mi );

/**
 * Enable or disable fullscreen.
 *
 * @warning With most window managers, only a top-level windows can be in
 * full-screen mode. Hence, this function will not operate properly if
 * libvlc_media_player_set_xid() was used to embed the video in a non-top-level
 * window. In that case, the embedding window must be reparented to the root
 * window <b>before</b> fullscreen mode is enabled. You will want to reparent
 * it back to its normal parent when disabling fullscreen.
 *
 * \param p_mi the media player
 * \param b_fullscreen boolean for fullscreen status
 */
VLC_PUBLIC_API void libvlc_set_fullscreen( libvlc_media_player_t *p_mi, int b_fullscreen );

/**
 * Get current fullscreen status.
 *
 * \param p_mi the media player
 * \return the fullscreen status (boolean)
 */
VLC_PUBLIC_API int libvlc_get_fullscreen( libvlc_media_player_t *p_mi );

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
 * \warning This function is only implemented for X11 at the moment.
 *
 * \param p_mi the media player
 * \param on true to handle key press events, false to ignore them.
 */
VLC_PUBLIC_API
void libvlc_video_set_key_input( libvlc_media_player_t *p_mi, unsigned on );

/**
 * Enable or disable mouse click events handling. By default, those events are
 * handled. This is needed for DVD menus to work, as well as a few video
 * filters such as "puzzle".
 *
 * \note See also libvlc_video_set_key_input().
 *
 * \warning This function is only implemented for X11 at the moment.
 *
 * \param p_mi the media player
 * \param on true to handle mouse click events, false to ignore them.
 */
VLC_PUBLIC_API
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
VLC_PUBLIC_API
int libvlc_video_get_size( libvlc_media_player_t *p_mi, unsigned num,
                           unsigned *px, unsigned *py );

/**
 * Get current video height.
 * You should use libvlc_video_get_size() instead.
 *
 * \param p_mi the media player
 * \return the video pixel height or 0 if not applicable
 */
VLC_DEPRECATED_API
int libvlc_video_get_height( libvlc_media_player_t *p_mi );

/**
 * Get current video width.
 * You should use libvlc_video_get_size() instead.
 *
 * \param p_mi the media player
 * \return the video pixel width or 0 if not applicable
 */
VLC_DEPRECATED_API
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
VLC_PUBLIC_API
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
VLC_PUBLIC_API float libvlc_video_get_scale( libvlc_media_player_t *p_mi );

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
VLC_PUBLIC_API void libvlc_video_set_scale( libvlc_media_player_t *p_mi, float f_factor );

/**
 * Get current video aspect ratio.
 *
 * \param p_mi the media player
 * \return the video aspect ratio or NULL if unspecified
 * (the result must be released with free()).
 */
VLC_PUBLIC_API char *libvlc_video_get_aspect_ratio( libvlc_media_player_t *p_mi );

/**
 * Set new video aspect ratio.
 *
 * \param p_mi the media player
 * \param psz_aspect new video aspect-ratio or NULL to reset to default
 * \note Invalid aspect ratios are ignored.
 */
VLC_PUBLIC_API void libvlc_video_set_aspect_ratio( libvlc_media_player_t *p_mi, const char *psz_aspect );

/**
 * Get current video subtitle.
 *
 * \param p_mi the media player
 * \return the video subtitle selected, or -1 if none
 */
VLC_PUBLIC_API int libvlc_video_get_spu( libvlc_media_player_t *p_mi );

/**
 * Get the number of available video subtitles.
 *
 * \param p_mi the media player
 * \return the number of available video subtitles
 */
VLC_PUBLIC_API int libvlc_video_get_spu_count( libvlc_media_player_t *p_mi );

/**
 * Get the description of available video subtitles.
 *
 * \param p_mi the media player
 * \return list containing description of available video subtitles
 */
VLC_PUBLIC_API libvlc_track_description_t *
        libvlc_video_get_spu_description( libvlc_media_player_t *p_mi );

/**
 * Set new video subtitle.
 *
 * \param p_mi the media player
 * \param i_spu new video subtitle to select
 * \return 0 on success, -1 if out of range
 */
VLC_PUBLIC_API int libvlc_video_set_spu( libvlc_media_player_t *p_mi, unsigned i_spu );

/**
 * Set new video subtitle file.
 *
 * \param p_mi the media player
 * \param psz_subtitle new video subtitle file
 * \return the success status (boolean)
 */
VLC_PUBLIC_API int libvlc_video_set_subtitle_file( libvlc_media_player_t *p_mi, const char *psz_subtitle );

/**
 * Get the description of available titles.
 *
 * \param p_mi the media player
 * \return list containing description of available titles
 */
VLC_PUBLIC_API libvlc_track_description_t *
        libvlc_video_get_title_description( libvlc_media_player_t *p_mi );

/**
 * Get the description of available chapters for specific title.
 *
 * \param p_mi the media player
 * \param i_title selected title
 * \return list containing description of available chapter for title i_title
 */
VLC_PUBLIC_API libvlc_track_description_t *
        libvlc_video_get_chapter_description( libvlc_media_player_t *p_mi, int i_title );

/**
 * Get current crop filter geometry.
 *
 * \param p_mi the media player
 * \return the crop filter geometry or NULL if unset
 */
VLC_PUBLIC_API char *libvlc_video_get_crop_geometry( libvlc_media_player_t *p_mi );

/**
 * Set new crop filter geometry.
 *
 * \param p_mi the media player
 * \param psz_geometry new crop filter geometry (NULL to unset)
 */
VLC_PUBLIC_API
void libvlc_video_set_crop_geometry( libvlc_media_player_t *p_mi, const char *psz_geometry );

/**
 * Get current teletext page requested.
 *
 * \param p_mi the media player
 * \return the current teletext page requested.
 */
VLC_PUBLIC_API int libvlc_video_get_teletext( libvlc_media_player_t *p_mi );

/**
 * Set new teletext page to retrieve.
 *
 * \param p_mi the media player
 * \param i_page teletex page number requested
 */
VLC_PUBLIC_API void libvlc_video_set_teletext( libvlc_media_player_t *p_mi, int i_page );

/**
 * Toggle teletext transparent status on video output.
 *
 * \param p_mi the media player
 */
VLC_PUBLIC_API void libvlc_toggle_teletext( libvlc_media_player_t *p_mi );

/**
 * Get number of available video tracks.
 *
 * \param p_mi media player
 * \return the number of available video tracks (int)
 */
VLC_PUBLIC_API int libvlc_video_get_track_count( libvlc_media_player_t *p_mi );

/**
 * Get the description of available video tracks.
 *
 * \param p_mi media player
 * \return list with description of available video tracks, or NULL on error
 */
VLC_PUBLIC_API libvlc_track_description_t *
        libvlc_video_get_track_description( libvlc_media_player_t *p_mi );

/**
 * Get current video track.
 *
 * \param p_mi media player
 * \return the video track (int) or -1 if none
 */
VLC_PUBLIC_API int libvlc_video_get_track( libvlc_media_player_t *p_mi );

/**
 * Set video track.
 *
 * \param p_mi media player
 * \param i_track the track (int)
 * \return 0 on success, -1 if out of range
 */
VLC_PUBLIC_API
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
VLC_PUBLIC_API
int libvlc_video_take_snapshot( libvlc_media_player_t *p_mi, unsigned num,
                                const char *psz_filepath, unsigned int i_width,
                                unsigned int i_height );

/**
 * Enable or disable deinterlace filter
 *
 * \param p_mi libvlc media player
 * \param psz_mode type of deinterlace filter, NULL to disable
 */
VLC_PUBLIC_API void libvlc_video_set_deinterlace( libvlc_media_player_t *p_mi,
                                                  const char *psz_mode );

/**
 * Get an integer marquee option value
 *
 * \param p_mi libvlc media player
 * \param option marq option to get \see libvlc_video_marquee_int_option_t
 */
VLC_PUBLIC_API int libvlc_video_get_marquee_int( libvlc_media_player_t *p_mi,
                                                 unsigned option );

/**
 * Get a string marquee option value
 *
 * \param p_mi libvlc media player
 * \param option marq option to get \see libvlc_video_marquee_string_option_t
 */
VLC_PUBLIC_API char *libvlc_video_get_marquee_string( libvlc_media_player_t *p_mi,
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
VLC_PUBLIC_API void libvlc_video_set_marquee_int( libvlc_media_player_t *p_mi,
                                                  unsigned option, int i_val );

/**
 * Set a marquee string option
 *
 * \param p_mi libvlc media player
 * \param option marq option to set \see libvlc_video_marquee_string_option_t
 * \param psz_text marq option value
 */
VLC_PUBLIC_API void libvlc_video_set_marquee_string( libvlc_media_player_t *p_mi,
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
    libvlc_logo_position,
};

/**
 * Get integer logo option.
 *
 * \param p_mi libvlc media player instance
 * \param option logo option to get, values of libvlc_video_logo_option_t
 */
VLC_PUBLIC_API int libvlc_video_get_logo_int( libvlc_media_player_t *p_mi,
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
VLC_PUBLIC_API void libvlc_video_set_logo_int( libvlc_media_player_t *p_mi,
                                               unsigned option, int value );

/**
 * Set logo option as string. Options that take a different type value
 * are ignored.
 *
 * \param p_mi libvlc media player instance
 * \param option logo option to set, values of libvlc_video_logo_option_t
 * \param psz_value logo option value
 */
VLC_PUBLIC_API void libvlc_video_set_logo_string( libvlc_media_player_t *p_mi,
                                      unsigned option, const char *psz_value );


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
VLC_PUBLIC_API libvlc_audio_output_t *
        libvlc_audio_output_list_get( libvlc_instance_t *p_instance );

/**
 * Free the list of available audio outputs
 *
 * \param p_list list with audio outputs for release
 */
VLC_PUBLIC_API void libvlc_audio_output_list_release( libvlc_audio_output_t *p_list );

/**
 * Set the audio output.
 * Change will be applied after stop and play.
 *
 * \param p_mi media player
 * \param psz_name name of audio output,
 *               use psz_name of \see libvlc_audio_output_t
 * \return true if function succeded
 */
VLC_PUBLIC_API int libvlc_audio_output_set( libvlc_media_player_t *p_mi,
                                            const char *psz_name );

/**
 * Get count of devices for audio output, these devices are hardware oriented
 * like analor or digital output of sound card
 *
 * \param p_instance libvlc instance
 * \param psz_audio_output - name of audio output, \see libvlc_audio_output_t
 * \return number of devices
 */
VLC_PUBLIC_API int libvlc_audio_output_device_count( libvlc_instance_t *p_instance,
                                                     const char *psz_audio_output );

/**
 * Get long name of device, if not available short name given
 *
 * \param p_instance libvlc instance
 * \param psz_audio_output - name of audio output, \see libvlc_audio_output_t
 * \param i_device device index
 * \return long name of device
 */
VLC_PUBLIC_API char * libvlc_audio_output_device_longname( libvlc_instance_t *p_instance,
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
VLC_PUBLIC_API char * libvlc_audio_output_device_id( libvlc_instance_t *p_instance,
                                                     const char *psz_audio_output,
                                                     int i_device );

/**
 * Set audio output device. Changes are only effective after stop and play.
 *
 * \param p_mi media player
 * \param psz_audio_output - name of audio output, \see libvlc_audio_output_t
 * \param psz_device_id device
 */
VLC_PUBLIC_API void libvlc_audio_output_device_set( libvlc_media_player_t *p_mi,
                                                    const char *psz_audio_output,
                                                    const char *psz_device_id );

/**
 * Get current audio device type. Device type describes something like
 * character of output sound - stereo sound, 2.1, 5.1 etc
 *
 * \param p_mi media player
 * \return the audio devices type \see libvlc_audio_output_device_types_t
 */
VLC_PUBLIC_API int libvlc_audio_output_get_device_type( libvlc_media_player_t *p_mi );

/**
 * Set current audio device type.
 *
 * \param p_mi vlc instance
 * \param device_type the audio device type,
          according to \see libvlc_audio_output_device_types_t
 */
VLC_PUBLIC_API void libvlc_audio_output_set_device_type( libvlc_media_player_t *p_mi,
                                                         int device_type );


/**
 * Toggle mute status.
 *
 * \param p_mi media player
 */
VLC_PUBLIC_API void libvlc_audio_toggle_mute( libvlc_media_player_t *p_mi );

/**
 * Get current mute status.
 *
 * \param p_mi media player
 * \return the mute status (boolean)
 */
VLC_PUBLIC_API int libvlc_audio_get_mute( libvlc_media_player_t *p_mi );

/**
 * Set mute status.
 *
 * \param p_mi media player
 * \param status If status is true then mute, otherwise unmute
 */
VLC_PUBLIC_API void libvlc_audio_set_mute( libvlc_media_player_t *p_mi, int status );

/**
 * Get current audio level.
 *
 * \param p_mi media player
 * \return the audio level (int)
 */
VLC_PUBLIC_API int libvlc_audio_get_volume( libvlc_media_player_t *p_mi );

/**
 * Set current audio level.
 *
 * \param p_mi media player
 * \param i_volume the volume (int)
 * \return 0 if the volume was set, -1 if it was out of range
 */
VLC_PUBLIC_API int libvlc_audio_set_volume( libvlc_media_player_t *p_mi, int i_volume );

/**
 * Get number of available audio tracks.
 *
 * \param p_mi media player
 * \return the number of available audio tracks (int), or -1 if unavailable
 */
VLC_PUBLIC_API int libvlc_audio_get_track_count( libvlc_media_player_t *p_mi );

/**
 * Get the description of available audio tracks.
 *
 * \param p_mi media player
 * \return list with description of available audio tracks, or NULL
 */
VLC_PUBLIC_API libvlc_track_description_t *
        libvlc_audio_get_track_description( libvlc_media_player_t *p_mi );

/**
 * Get current audio track.
 *
 * \param p_mi media player
 * \return the audio track (int), or -1 if none.
 */
VLC_PUBLIC_API int libvlc_audio_get_track( libvlc_media_player_t *p_mi );

/**
 * Set current audio track.
 *
 * \param p_mi media player
 * \param i_track the track (int)
 * \return 0 on success, -1 on error
 */
VLC_PUBLIC_API int libvlc_audio_set_track( libvlc_media_player_t *p_mi, int i_track );

/**
 * Get current audio channel.
 *
 * \param p_mi media player
 * \return the audio channel \see libvlc_audio_output_channel_t
 */
VLC_PUBLIC_API int libvlc_audio_get_channel( libvlc_media_player_t *p_mi );

/**
 * Set current audio channel.
 *
 * \param p_mi media player
 * \param channel the audio channel, \see libvlc_audio_output_channel_t
 * \return 0 on success, -1 on error
 */
VLC_PUBLIC_API int libvlc_audio_set_channel( libvlc_media_player_t *p_mi, int channel );

/** @} audio */

/** @} media_player */

# ifdef __cplusplus
}
# endif

#endif /* VLC_LIBVLC_MEDIA_PLAYER_H */
