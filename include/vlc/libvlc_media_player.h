/*****************************************************************************
 * libvlc_media_player.h:  libvlc_media_player external API
 *****************************************************************************
 * Copyright (C) 1998-2009 the VideoLAN team
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
/** \defgroup libvlc_media_player libvlc_media_player
 * \ingroup libvlc
 * LibVLC Media Player, object that let you play a media
 * in a custom drawable
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
VLC_PUBLIC_API libvlc_media_player_t * libvlc_media_player_new( libvlc_instance_t * );

/**
 * Create a Media Player object from a Media
 *
 * \param p_md the media. Afterwards the p_md can be safely
 *        destroyed.
 * \return a new media player object, or NULL on error.
 */
VLC_PUBLIC_API libvlc_media_player_t * libvlc_media_player_new_from_media( libvlc_media_t * );

/**
 * Release a media_player after use
 * Decrement the reference count of a media player object. If the
 * reference count is 0, then libvlc_media_player_release() will
 * release the media player object. If the media player object
 * has been released, then it should not be used again.
 *
 * \param p_mi the Media Player to free
 */
VLC_PUBLIC_API void libvlc_media_player_release( libvlc_media_player_t * );

/**
 * Retain a reference to a media player object. Use
 * libvlc_media_player_release() to decrement reference count.
 *
 * \param p_mi media player object
 */
VLC_PUBLIC_API void libvlc_media_player_retain( libvlc_media_player_t * );

/**
 * Set the media that will be used by the media_player. If any,
 * previous md will be released.
 *
 * \param p_mi the Media Player
 * \param p_md the Media. Afterwards the p_md can be safely
 *        destroyed.
 */
VLC_PUBLIC_API void libvlc_media_player_set_media( libvlc_media_player_t *, libvlc_media_t * );

/**
 * Get the media used by the media_player.
 *
 * \param p_mi the Media Player
 * \return the media associated with p_mi, or NULL if no
 *         media is associated
 */
VLC_PUBLIC_API libvlc_media_t * libvlc_media_player_get_media( libvlc_media_player_t * );

/**
 * Get the Event Manager from which the media player send event.
 *
 * \param p_mi the Media Player
 * \return the event manager associated with p_mi
 */
VLC_PUBLIC_API libvlc_event_manager_t * libvlc_media_player_event_manager ( libvlc_media_player_t * );

/**
 * is_playing
 *
 * \param p_mi the Media Player
 * \return 1 if the media player is playing, 0 otherwise
 */
VLC_PUBLIC_API int libvlc_media_player_is_playing ( libvlc_media_player_t * );

/**
 * Play
 *
 * \param p_mi the Media Player
 * \return 0 if playback started (and was already started), or -1 on error.
 */
VLC_PUBLIC_API int libvlc_media_player_play ( libvlc_media_player_t * );

/**
 * Toggle pause (no effect if there is no media)
 *
 * \param p_mi the Media Player
 */
VLC_PUBLIC_API void libvlc_media_player_pause ( libvlc_media_player_t * );

/**
 * Stop (no effect if there is no media)
 *
 * \param p_mi the Media Player
 */
VLC_PUBLIC_API void libvlc_media_player_stop ( libvlc_media_player_t * );

/**
 * Set the NSView handler where the media player should render its video output.
 *
 * The object minimal_macosx expects is of kind NSObject and should
 * respect the protocol:
 *
 * @protocol VLCOpenGLVideoViewEmbedding <NSObject>
 * - (void)addVoutSubview:(NSView *)view;
 * - (void)removeVoutSubview:(NSView *)view;
 * @end
 *
 * You can find a live example in VLCVideoView in VLCKit.framework.
 *
 * \param p_mi the Media Player
 * \param drawable the NSView handler
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
 * \param p_e an initialized exception pointer
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
VLC_PUBLIC_API libvlc_time_t libvlc_media_player_get_length( libvlc_media_player_t * );

/**
 * Get the current movie time (in ms).
 *
 * \param p_mi the Media Player
 * \return the movie time (in ms), or -1 if there is no media.
 */
VLC_PUBLIC_API libvlc_time_t libvlc_media_player_get_time( libvlc_media_player_t * );

/**
 * Set the movie time (in ms). This has no effect if no media is being played.
 * Not all formats and protocols support this.
 *
 * \param p_mi the Media Player
 * \param the movie time (in ms).
 */
VLC_PUBLIC_API void libvlc_media_player_set_time( libvlc_media_player_t *, libvlc_time_t );

/**
 * Get movie position.
 *
 * \param p_mi the Media Player
 * \return movie position, or -1. in case of error
 */
VLC_PUBLIC_API float libvlc_media_player_get_position( libvlc_media_player_t * );

/**
 * Set movie position. This has no effect if playback is not enabled.
 * This might not work depending on the underlying input format and protocol.
 *
 * \param p_mi the Media Player
 * \param f_pos the position
 */
VLC_PUBLIC_API void libvlc_media_player_set_position( libvlc_media_player_t *, float );

/**
 * Set movie chapter (if applicable).
 *
 * \param p_mi the Media Player
 * \param i_chapter chapter number to play
 */
VLC_PUBLIC_API void libvlc_media_player_set_chapter( libvlc_media_player_t *, int );

/**
 * Get movie chapter.
 *
 * \param p_mi the Media Player
 * \return chapter number currently playing, or -1 if there is no media.
 */
VLC_PUBLIC_API int libvlc_media_player_get_chapter( libvlc_media_player_t * );

/**
 * Get movie chapter count
 *
 * \param p_mi the Media Player
 * \return number of chapters in movie, or -1.
 */
VLC_PUBLIC_API int libvlc_media_player_get_chapter_count( libvlc_media_player_t * );

/**
 * Is the player able to play
 *
 * \param p_mi the Media Player
 * \return boolean
 */
VLC_PUBLIC_API int libvlc_media_player_will_play( libvlc_media_player_t * );

/**
 * Get title chapter count
 *
 * \param p_mi the Media Player
 * \param i_title title
 * \return number of chapters in title, or -1
 */
VLC_PUBLIC_API int libvlc_media_player_get_chapter_count_for_title(
                       libvlc_media_player_t *, int );

/**
 * Set movie title
 *
 * \param p_mi the Media Player
 * \param i_title title number to play
 */
VLC_PUBLIC_API void libvlc_media_player_set_title( libvlc_media_player_t *, int );

/**
 * Get movie title
 *
 * \param p_mi the Media Player
 * \return title number currently playing, or -1
 */
VLC_PUBLIC_API int libvlc_media_player_get_title( libvlc_media_player_t * );

/**
 * Get movie title count
 *
 * \param p_mi the Media Player
 * \return title number count, or -1
 */
VLC_PUBLIC_API int libvlc_media_player_get_title_count( libvlc_media_player_t * );

/**
 * Set previous chapter (if applicable)
 *
 * \param p_mi the Media Player
 */
VLC_PUBLIC_API void libvlc_media_player_previous_chapter( libvlc_media_player_t * );

/**
 * Set next chapter (if applicable)
 *
 * \param p_mi the Media Player
 */
VLC_PUBLIC_API void libvlc_media_player_next_chapter( libvlc_media_player_t * );

/**
 * Get movie play rate
 *
 * \param p_mi the Media Player
 * \return movie play rate, or zero in case of error
 */
VLC_PUBLIC_API float libvlc_media_player_get_rate( libvlc_media_player_t * );

/**
 * Set movie play rate
 *
 * \param p_mi the Media Player
 * \param movie play rate to set
 * \return -1 if an error was detected, 0 otherwise (but even then, it might
 * not actually work depending on the underlying media protocol)
 */
VLC_PUBLIC_API int libvlc_media_player_set_rate( libvlc_media_player_t *, float );

/**
 * Get current movie state
 *
 * \param p_mi the Media Player
 */
VLC_PUBLIC_API libvlc_state_t libvlc_media_player_get_state( libvlc_media_player_t *);

/**
 * Get movie fps rate
 *
 * \param p_mi the Media Player
 * \return frames per second (fps) for this playing movie, or 0 if unspecified
 */
VLC_PUBLIC_API float libvlc_media_player_get_fps( libvlc_media_player_t * );

/** end bug */

/**
 * Does this media player have a video output?
 *
 * \param p_md the media player
 */
VLC_PUBLIC_API int libvlc_media_player_has_vout( libvlc_media_player_t * );

/**
 * Is this media player seekable?
 *
 * \param p_input the input
 */
VLC_PUBLIC_API int libvlc_media_player_is_seekable( libvlc_media_player_t *p_mi );

/**
 * Can this media player be paused?
 *
 * \param p_input the input
 */
VLC_PUBLIC_API int libvlc_media_player_can_pause( libvlc_media_player_t *p_mi );


/**
 * Display the next frame (if supported)
 *
 * \param p_input the libvlc_media_player_t instance
 */
VLC_PUBLIC_API void libvlc_media_player_next_frame( libvlc_media_player_t *p_input );



/**
 * Release (free) libvlc_track_description_t
 *
 * \param p_track_description the structure to release
 */
VLC_PUBLIC_API void libvlc_track_description_release( libvlc_track_description_t *p_track_description );

/** \defgroup libvlc_video libvlc_video
 * \ingroup libvlc_media_player
 * LibVLC Video handling
 * @{
 */

/**
 * Toggle fullscreen status on a non-embedded video output.
 *
 * @warning The same limitations applies to this function
 * as to libvlc_set_fullscreen().
 *
 * \param p_mediaplayer the media player
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_toggle_fullscreen( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Enable or disable fullscreen on a non-embedded video output.
 *
 * @warning With most window managers, only a top-level windows can switch to
 * full-screen mode. Hence, this function will not operate properly if
 * libvlc_media_player_set_xid() or libvlc_media_player_set_hwnd() was
 * used to embed the video in a non-LibVLC widget. If you want to to render an
 * embedded LibVLC video full-screen, the parent embedding widget must expanded
 * to full screen (LibVLC cannot take care of that).
 * LibVLC will then automatically resize the video as appropriate.
 *
 * \param p_mediaplayer the media player
 * \param b_fullscreen boolean for fullscreen status
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_set_fullscreen( libvlc_media_player_t *, int, libvlc_exception_t * );

/**
 * Get current fullscreen status.
 *
 * \param p_mediaplayer the media player
 * \param p_e an initialized exception pointer
 * \return the fullscreen status (boolean)
 */
VLC_PUBLIC_API int libvlc_get_fullscreen( libvlc_media_player_t *, libvlc_exception_t * );

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
 * \param mp the media player
 * \param on true to handle key press events, false to ignore them.
 */
VLC_PUBLIC_API
void libvlc_video_set_key_input( libvlc_media_player_t *mp, unsigned on );

/**
 * Enable or disable mouse click events handling. By default, those events are
 * handled. This is needed for DVD menus to work, as well as a few video
 * filters such as "puzzle".
 *
 * \note See also \func libvlc_video_set_key_input().
 *
 * \warning This function is only implemented for X11 at the moment.
 *
 * \param mp the media player
 * \param on true to handle mouse click events, false to ignore them.
 */
VLC_PUBLIC_API
void libvlc_video_set_mouse_input( libvlc_media_player_t *mp, unsigned on );

/**
 * Get current video height.
 *
 * \param p_mediaplayer the media player
 * \param p_e an initialized exception pointer
 * \return the video height
 */
VLC_PUBLIC_API int libvlc_video_get_height( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Get current video width.
 *
 * \param p_mediaplayer the media player
 * \param p_e an initialized exception pointer
 * \return the video width
 */
VLC_PUBLIC_API int libvlc_video_get_width( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Get the current video scaling factor.
 * See also libvlc_video_set_scale().
 *
 * \param p_mediaplayer the media player
 * \param p_e an initialized exception pointer
 * \return the currently configured zoom factor, or 0. if the video is set
 * to fit to the output window/drawable automatically.
 */
VLC_PUBLIC_API float libvlc_video_get_scale( libvlc_media_player_t *,
                                             libvlc_exception_t *p_e );

/**
 * Set the video scaling factor. That is the ratio of the number of pixels on
 * screen to the number of pixels in the original decoded video in each
 * dimension. Zero is a special value; it will adjust the video to the output
 * window/drawable (in windowed mode) or the entire screen.
 *
 * Note that not all video outputs support scaling.
 *
 * \param p_mediaplayer the media player
 * \param i_factor the scaling factor, or zero
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_video_set_scale( libvlc_media_player_t *, float,
                                            libvlc_exception_t *p_e );

/**
 * Get current video aspect ratio.
 *
 * \param p_mediaplayer the media player
 * \param p_e an initialized exception pointer
 * \return the video aspect ratio
 */
VLC_PUBLIC_API char *libvlc_video_get_aspect_ratio( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Set new video aspect ratio.
 *
 * \param p_mediaplayer the media player
 * \param psz_aspect new video aspect-ratio
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_video_set_aspect_ratio( libvlc_media_player_t *, const char *, libvlc_exception_t * );

/**
 * Get current video subtitle.
 *
 * \param p_mediaplayer the media player
 * \param p_e an initialized exception pointer
 * \return the video subtitle selected
 */
VLC_PUBLIC_API int libvlc_video_get_spu( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Get the number of available video subtitles.
 *
 * \param p_mediaplayer the media player
 * \param p_e an initialized exception pointer
 * \return the number of available video subtitles
 */
VLC_PUBLIC_API int libvlc_video_get_spu_count( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Get the description of available video subtitles.
 *
 * \param p_mediaplayer the media player
 * \param p_e an initialized exception pointer
 * \return list containing description of available video subtitles
 */
VLC_PUBLIC_API libvlc_track_description_t *
        libvlc_video_get_spu_description( libvlc_media_player_t * );

/**
 * Set new video subtitle.
 *
 * \param p_mediaplayer the media player
 * \param i_spu new video subtitle to select
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_video_set_spu( libvlc_media_player_t *, int , libvlc_exception_t * );

/**
 * Set new video subtitle file.
 *
 * \param p_mediaplayer the media player
 * \param psz_subtitle new video subtitle file
 * \param p_e an initialized exception pointer
 * \return the success status (boolean)
 */
VLC_PUBLIC_API int libvlc_video_set_subtitle_file( libvlc_media_player_t *, const char * );

/**
 * Get the description of available titles.
 *
 * \param p_mediaplayer the media player
 * \param p_e an initialized exception pointer
 * \return list containing description of available titles
 */
VLC_PUBLIC_API libvlc_track_description_t *
        libvlc_video_get_title_description( libvlc_media_player_t * );

/**
 * Get the description of available chapters for specific title.
 *
 * \param p_mediaplayer the media player
 * \param i_title selected title
 * \param p_e an initialized exception pointer
 * \return list containing description of available chapter for title i_title
 */
VLC_PUBLIC_API libvlc_track_description_t *
        libvlc_video_get_chapter_description( libvlc_media_player_t *, int );

/**
 * Get current crop filter geometry.
 *
 * \param p_mediaplayer the media player
 * \param p_e an initialized exception pointer
 * \return the crop filter geometry
 */
VLC_PUBLIC_API char *libvlc_video_get_crop_geometry( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Set new crop filter geometry.
 *
 * \param p_mediaplayer the media player
 * \param psz_geometry new crop filter geometry
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_video_set_crop_geometry( libvlc_media_player_t *, const char *, libvlc_exception_t * );

/**
 * Toggle teletext transparent status on video output.
 *
 * \param p_mediaplayer the media player
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_toggle_teletext( libvlc_media_player_t * );

/**
 * Get number of available video tracks.
 *
 * \param p_mi media player
 * \return the number of available video tracks (int)
 */
VLC_PUBLIC_API int libvlc_video_get_track_count( libvlc_media_player_t * );

/**
 * Get the description of available video tracks.
 *
 * \param p_mi media player
 * \param p_e an initialized exception
 * \return list with description of available video tracks, or NULL on error
 */
VLC_PUBLIC_API libvlc_track_description_t *
        libvlc_video_get_track_description( libvlc_media_player_t * );

/**
 * Get current video track.
 *
 * \param p_mi media player
 * \param p_e an initialized exception pointer
 * \return the video track (int)
 */
VLC_PUBLIC_API int libvlc_video_get_track( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Set video track.
 *
 * \param p_mi media player
 * \param i_track the track (int)
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_video_set_track( libvlc_media_player_t *, int, libvlc_exception_t * );

/**
 * Take a snapshot of the current video window.
 *
 * If i_width AND i_height is 0, original size is used.
 * If i_width XOR i_height is 0, original aspect-ratio is preserved.
 *
 * \param p_mi media player instance
 * \param psz_filepath the path where to save the screenshot to
 * \param i_width the snapshot's width
 * \param i_height the snapshot's height
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_video_take_snapshot( libvlc_media_player_t *, const char *,unsigned int, unsigned int, libvlc_exception_t * );

/**
 * Enable or disable deinterlace filter
 *
 * \param p_mi libvlc media player
 * \param b_enable boolean to enable or disable deinterlace filter
 * \param psz_mode type of deinterlace filter to use
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_video_set_deinterlace( libvlc_media_player_t *,
                                                  int , const char *,
                                                  libvlc_exception_t *);

/**
 * Get an integer marquee option value
 *
 * \param p_mi libvlc media player
 * \param option marq option to get \see libvlc_video_marquee_int_option_t
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API int libvlc_video_get_marquee_int( libvlc_media_player_t *,
                                             unsigned, libvlc_exception_t * );

/**
 * Get a string marquee option value
 *
 * \param p_mi libvlc media player
 * \param option marq option to get \see libvlc_video_marquee_string_option_t
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API char *libvlc_video_get_marquee_string( libvlc_media_player_t *,
                                             unsigned, libvlc_exception_t * );

/**
 * Enable, disable or set an integer marquee option
 *
 * Setting libvlc_marquee_Enable has the side effect of enabling (arg !0)
 * or disabling (arg 0) the marq filter.
 *
 * \param p_mi libvlc media player
 * \param option marq option to set \see libvlc_video_marquee_int_option_t
 * \param i_val marq option value
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_video_set_marquee_int( libvlc_media_player_t *,
                                        unsigned, int, libvlc_exception_t * );

/**
 * Set a marquee string option
 *
 * \param p_mi libvlc media player
 * \param option marq option to set \see libvlc_video_marquee_string_option_t
 * \param psz_text marq option value
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_video_set_marquee_string( libvlc_media_player_t *,
                               unsigned, const char *, libvlc_exception_t * );

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
 * \param p_e an pointer to an initialized exception object
 */
VLC_PUBLIC_API int libvlc_video_get_logo_int( libvlc_media_player_t *p_mi,
                                 unsigned option, libvlc_exception_t *p_e );

/**
 * Set logo option as integer. Options that take a different type value
 * cause an invalid argument exception.
 * Passing libvlc_logo_enable as option value has the side effect of
 * starting (arg !0) or stopping (arg 0) the logo filter.
 *
 * \param p_mi libvlc media player instance
 * \param option logo option to set, values of libvlc_video_logo_option_t
 * \param value logo option value
 * \param p_e an pointer to an initialized exception object
 */
VLC_PUBLIC_API void libvlc_video_set_logo_int( libvlc_media_player_t *p_mi,
                        unsigned option, int value, libvlc_exception_t *p_e );

/**
 * Set logo option as string. Options that take a different type value
 * cause an invalid argument exception.
 *
 * \param p_mi libvlc media player instance
 * \param option logo option to set, values of libvlc_video_logo_option_t
 * \param psz_value logo option value
 * \param p_e an pointer to an initialized exception object
 */
VLC_PUBLIC_API void libvlc_video_set_logo_string( libvlc_media_player_t *p_mi,
            unsigned option, const char *psz_value, libvlc_exception_t *p_e );


/** @} video */

/** \defgroup libvlc_audio libvlc_audio
 * \ingroup libvlc_media_player
 * LibVLC Audio handling
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
        libvlc_audio_output_list_get( libvlc_instance_t * );

/**
 * Free the list of available audio outputs
 *
 * \param p_list list with audio outputs for release
 */
VLC_PUBLIC_API void libvlc_audio_output_list_release( libvlc_audio_output_t * );

/**
 * Set the audio output.
 * Change will be applied after stop and play.
 *
 * \param mp media player
 * \param psz_name name of audio output,
 *               use psz_name of \see libvlc_audio_output_t
 * \return true if function succeded
 */
VLC_PUBLIC_API int libvlc_audio_output_set( libvlc_media_player_t *,
                                            const char * );

/**
 * Get count of devices for audio output, these devices are hardware oriented
 * like analor or digital output of sound card
 *
 * \param p_instance libvlc instance
 * \param psz_audio_output - name of audio output, \see libvlc_audio_output_t
 * \return number of devices
 */
VLC_PUBLIC_API int libvlc_audio_output_device_count( libvlc_instance_t *,
                                                     const char * );

/**
 * Get long name of device, if not available short name given
 *
 * \param p_instance libvlc instance
 * \param psz_audio_output - name of audio output, \see libvlc_audio_output_t
 * \param i_device device index
 * \return long name of device
 */
VLC_PUBLIC_API char * libvlc_audio_output_device_longname( libvlc_instance_t *,
                                                           const char *,
                                                           int );

/**
 * Get id name of device
 *
 * \param p_instance libvlc instance
 * \param psz_audio_output - name of audio output, \see libvlc_audio_output_t
 * \param i_device device index
 * \return id name of device, use for setting device, need to be free after use
 */
VLC_PUBLIC_API char * libvlc_audio_output_device_id( libvlc_instance_t *,
                                                     const char *,
                                                     int );

/**
 * Set audio output device. Changes are only effective after stop and play.
 *
 * \param mp media player
 * \param psz_audio_output - name of audio output, \see libvlc_audio_output_t
 * \param psz_device_id device
 */
VLC_PUBLIC_API void libvlc_audio_output_device_set( libvlc_media_player_t *,
                                                    const char *,
                                                    const char * );

/**
 * Get current audio device type. Device type describes something like
 * character of output sound - stereo sound, 2.1, 5.1 etc
 *
 * \param mp media player
 * \return the audio devices type \see libvlc_audio_output_device_types_t
 */
VLC_PUBLIC_API int libvlc_audio_output_get_device_type(
        libvlc_media_player_t * );

/**
 * Set current audio device type.
 *
 * \param mp vlc instance
 * \param device_type the audio device type,
          according to \see libvlc_audio_output_device_types_t
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_audio_output_set_device_type( libvlc_media_player_t *,
                                                         int );


/**
 * Toggle mute status.
 *
 * \param mp media player
 */
VLC_PUBLIC_API void libvlc_audio_toggle_mute( libvlc_media_player_t * );

/**
 * Get current mute status.
 *
 * \param mp media player
 * \return the mute status (boolean)
 */
VLC_PUBLIC_API int libvlc_audio_get_mute( libvlc_media_player_t * );

/**
 * Set mute status.
 *
 * \param mp media player
 * \param status If status is true then mute, otherwise unmute
 */
VLC_PUBLIC_API void libvlc_audio_set_mute( libvlc_media_player_t *, int );

/**
 * Get current audio level.
 *
 * \param mp media player
 * \param p_e an initialized exception pointer
 * \return the audio level (int)
 */
VLC_PUBLIC_API int libvlc_audio_get_volume( libvlc_media_player_t * );

/**
 * Set current audio level.
 *
 * \param mp media player
 * \param i_volume the volume (int)
 * \return 0 if the volume was set, -1 if it was out of range
 */
VLC_PUBLIC_API int libvlc_audio_set_volume( libvlc_media_player_t *, int );

/**
 * Get number of available audio tracks.
 *
 * \param p_mi media player
 * \return the number of available audio tracks (int), or -1 if unavailable
 */
VLC_PUBLIC_API int libvlc_audio_get_track_count( libvlc_media_player_t * );

/**
 * Get the description of available audio tracks.
 *
 * \param p_mi media player
 * \return list with description of available audio tracks, or NULL
 */
VLC_PUBLIC_API libvlc_track_description_t *
        libvlc_audio_get_track_description( libvlc_media_player_t * );

/**
 * Get current audio track.
 *
 * \param p_mi media player
 * \return the audio track (int), or -1 if none.
 */
VLC_PUBLIC_API int libvlc_audio_get_track( libvlc_media_player_t * );

/**
 * Set current audio track.
 *
 * \param p_mi media player
 * \param i_track the track (int)
 * \return 0 on success, -1 on error
 */
VLC_PUBLIC_API int libvlc_audio_set_track( libvlc_media_player_t *, int );

/**
 * Get current audio channel.
 *
 * \param mp media player
 * \return the audio channel \see libvlc_audio_output_channel_t
 */
VLC_PUBLIC_API int libvlc_audio_get_channel( libvlc_media_player_t * );

/**
 * Set current audio channel.
 *
 * \param p_mi media player
 * \param channel the audio channel, \see libvlc_audio_output_channel_t
 * \return 0 on success, -1 on error
 */
VLC_PUBLIC_API int libvlc_audio_set_channel( libvlc_media_player_t *, int );

/** @} audio */

/** @} media_player */

# ifdef __cplusplus
}
# endif

#endif /* VLC_LIBVLC_MEDIA_PLAYER_H */
