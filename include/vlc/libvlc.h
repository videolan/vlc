/*****************************************************************************
 * libvlc.h:  libvlc external API
 *****************************************************************************
 * Copyright (C) 1998-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Paul Saman <jpsaman _at_ m2x _dot_ nl>
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
 * \defgroup libvlc libvlc
 * This is libvlc, the base library of the VLC program.
 *
 * @{
 */


#ifndef _LIBVLC_H
#define _LIBVLC_H 1

#include <vlc/vlc.h>

# ifdef __cplusplus
extern "C" {
# endif

/*****************************************************************************
 * Exception handling
 *****************************************************************************/
/** \defgroup libvlc_exception libvlc_exception
 * \ingroup libvlc_core
 * LibVLC Exceptions handling
 * @{
 */

/**
 * Initialize an exception structure. This can be called several times to
 * reuse an exception structure.
 *
 * \param p_exception the exception to initialize
 */
VLC_PUBLIC_API void libvlc_exception_init( libvlc_exception_t *p_exception );

/**
 * Has an exception been raised?
 *
 * \param p_exception the exception to query
 * \return 0 if the exception was raised, 1 otherwise
 */
VLC_PUBLIC_API int
libvlc_exception_raised( const libvlc_exception_t *p_exception );

/**
 * Raise an exception using a user-provided message.
 *
 * \param p_exception the exception to raise
 * \param psz_format the exception message format string
 * \param ... the format string arguments
 */
VLC_PUBLIC_API void
libvlc_exception_raise( libvlc_exception_t *p_exception,
                        const char *psz_format, ... );

/**
 * Clear an exception object so it can be reused.
 * The exception object must have be initialized.
 *
 * \param p_exception the exception to clear
 */
VLC_PUBLIC_API void libvlc_exception_clear( libvlc_exception_t * );

/**
 * Get an exception's message.
 *
 * \param p_exception the exception to query
 * \return the exception message or NULL if not applicable (exception not
 *         raised, for example)
 */
VLC_PUBLIC_API const char *
libvlc_exception_get_message( const libvlc_exception_t *p_exception );

/**@} */

/*****************************************************************************
 * Core handling
 *****************************************************************************/

/** \defgroup libvlc_core libvlc_core
 * \ingroup libvlc
 * LibVLC Core
 * @{
 */

/**
 * Create and initialize a libvlc instance.
 *
 * \param argc the number of arguments
 * \param argv command-line-type arguments. argv[0] must be the path of the
 *        calling program.
 * \param p_e an initialized exception pointer
 * \return the libvlc instance
 */
VLC_PUBLIC_API libvlc_instance_t *
libvlc_new( int , const char *const *, libvlc_exception_t *);

/**
 * Return a libvlc instance identifier for legacy APIs. Use of this
 * function is discouraged, you should convert your program to use the
 * new API.
 *
 * \param p_instance the instance
 * \return the instance identifier
 */
VLC_PUBLIC_API int libvlc_get_vlc_id( libvlc_instance_t *p_instance );

/**
 * Decrement the reference count of a libvlc instance, and destroy it
 * if it reaches zero.
 *
 * \param p_instance the instance to destroy
 */
VLC_PUBLIC_API void libvlc_release( libvlc_instance_t * );

/**
 * Increments the reference count of a libvlc instance.
 * The initial reference count is 1 after libvlc_new() returns.
 *
 * \param p_instance the instance to reference
 */
VLC_PUBLIC_API void libvlc_retain( libvlc_instance_t * );

/**
 * Retrieve libvlc version.
 *
 * Example: "0.9.0-git Grishenko"
 *
 * \return a string containing the libvlc version
 */
VLC_PUBLIC_API const char * libvlc_get_version();

/**
 * Retrieve libvlc compiler version.
 *
 * Example: "gcc version 4.2.3 (Ubuntu 4.2.3-2ubuntu6)"
 *
 * \return a string containing the libvlc compiler version
 */
VLC_PUBLIC_API const char * libvlc_get_compiler();

/**
 * Retrieve libvlc changeset.
 *
 * Example: "aa9bce0bc4"
 *
 * \return a string containing the libvlc changeset
 */
VLC_PUBLIC_API const char * libvlc_get_changeset();

/** @}*/

/*****************************************************************************
 * Media descriptor
 *****************************************************************************/
/** \defgroup libvlc_media libvlc_media
 * \ingroup libvlc
 * LibVLC Media Descriptor
 * @{
 */

/**
 * Create a media descriptor with the given MRL.
 *
 * \param p_instance the instance
 * \param psz_mrl the MRL to read
 * \param p_e an initialized exception pointer
 * \return the newly created media descriptor
 */
VLC_PUBLIC_API libvlc_media_t * libvlc_media_new(
                                   libvlc_instance_t *p_instance,
                                   const char * psz_mrl,
                                   libvlc_exception_t *p_e );

/**
 * Create a media descriptor as an empty node with the passed name.
 *
 * \param p_instance the instance
 * \param psz_name the name of the node
 * \param p_e an initialized exception pointer
 * \return the new empty media descriptor
 */
VLC_PUBLIC_API libvlc_media_t * libvlc_media_new_as_node(
                                   libvlc_instance_t *p_instance,
                                   const char * psz_name,
                                   libvlc_exception_t *p_e );

/**
 * Add an option to the media descriptor.
 *
 * This option will be used to determine how the media_player will
 * read the media. This allows to use VLC's advanced
 * reading/streaming options on a per-media basis.
 *
 * The options are detailed in vlc --long-help, for instance "--sout-all"
 *
 * \param p_instance the instance
 * \param psz_mrl the MRL to read
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_media_add_option(
                                   libvlc_media_t * p_md,
                                   const char * ppsz_options,
                                   libvlc_exception_t * p_e );

VLC_PUBLIC_API void libvlc_media_retain(
                                   libvlc_media_t *p_meta_desc );

VLC_PUBLIC_API void libvlc_media_release(
                                   libvlc_media_t *p_meta_desc );

VLC_PUBLIC_API char * libvlc_media_get_mrl( libvlc_media_t * p_md,
                                                       libvlc_exception_t * p_e );

VLC_PUBLIC_API libvlc_media_t * libvlc_media_duplicate( libvlc_media_t * );

/**
 * Read the meta of the media descriptor.
 *
 * \param p_meta_desc the media descriptor to read
 * \param p_meta_desc the meta to read
 * \param p_e an initialized exception pointer
 * \return the media descriptor's meta
 */
VLC_PUBLIC_API char * libvlc_media_get_meta(
                                   libvlc_media_t *p_meta_desc,
                                   libvlc_meta_t e_meta,
                                   libvlc_exception_t *p_e );

VLC_PUBLIC_API libvlc_state_t libvlc_media_get_state(
                                   libvlc_media_t *p_meta_desc,
                                   libvlc_exception_t *p_e );

VLC_PUBLIC_API libvlc_media_list_t *
    libvlc_media_subitems( libvlc_media_t *p_md,
                                      libvlc_exception_t *p_e );

VLC_PUBLIC_API libvlc_event_manager_t *
    libvlc_media_event_manager( libvlc_media_t * p_md,
                                           libvlc_exception_t * p_e );

VLC_PUBLIC_API libvlc_time_t
   libvlc_media_get_duration( libvlc_media_t * p_md,
                                         libvlc_exception_t * p_e );

VLC_PUBLIC_API int
   libvlc_media_is_preparsed( libvlc_media_t * p_md,
                                         libvlc_exception_t * p_e );

VLC_PUBLIC_API void
    libvlc_media_set_user_data( libvlc_media_t * p_md,
                                           void * p_new_user_data,
                                           libvlc_exception_t * p_e);
VLC_PUBLIC_API void *
    libvlc_media_get_user_data( libvlc_media_t * p_md,
                                           libvlc_exception_t * p_e);

/** @}*/

/*****************************************************************************
 * Media Instance
 *****************************************************************************/
/** \defgroup libvlc_media_player libvlc_media_player
 * \ingroup libvlc
 * LibVLC Media Instance, object that let you play a media descriptor
 * in a libvlc_drawable_t
 * @{
 */

/** 
 * Create an empty Media Instance object
 *
 * \param p_libvlc_instance the libvlc instance in which the Media Instance
 *        should be created.
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API libvlc_media_player_t * libvlc_media_player_new( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Create a Media Instance object from a Media Descriptor
 *
 * \param p_md the media descriptor. Afterwards the p_md can be safely
 *        destroyed.
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API libvlc_media_player_t * libvlc_media_player_new_from_media( libvlc_media_t *, libvlc_exception_t * );

/**
 * Release a media_player after use
 *
 * \param p_mi the Media Instance to free
 */
VLC_PUBLIC_API void libvlc_media_player_release( libvlc_media_player_t * );
VLC_PUBLIC_API void libvlc_media_player_retain( libvlc_media_player_t * );

/** Set the media descriptor that will be used by the media_player. If any,
 * previous md will be released.
 *
 * \param p_mi the Media Instance
 * \param p_md the Media Descriptor. Afterwards the p_md can be safely
 *        destroyed.
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_media_player_set_media( libvlc_media_player_t *, libvlc_media_t *, libvlc_exception_t * );

/**
 * Get the media descriptor used by the media_player. 
 *
 * \param p_mi the Media Instance
 * \param p_e an initialized exception pointer
 * \return the media descriptor associated with p_mi, or NULL if no
 *         media descriptor is associated
 */
VLC_PUBLIC_API libvlc_media_t * libvlc_media_player_get_media( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Get the Event Manager from which the media instance send event.
 *
 * \param p_mi the Media Instance
 * \param p_e an initialized exception pointer
 * \return the event manager associated with p_mi
 */
VLC_PUBLIC_API libvlc_event_manager_t * libvlc_media_player_event_manager ( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Play 
 *
 * \param p_mi the Media Instance
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_media_player_play ( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Pause 
 *
 * \param p_mi the Media Instance
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_media_player_pause ( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Stop 
 *
 * \param p_mi the Media Instance
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_media_player_stop ( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Set the drawable where the media instance should render its video output
 *
 * \param p_mi the Media Instance
 * \param drawable the libvlc_drawable_t where the media instance
 *        should render its video
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_media_player_set_drawable ( libvlc_media_player_t *, libvlc_drawable_t, libvlc_exception_t * );

/**
 * Get the drawable where the media instance should render its video output
 *
 * \param p_mi the Media Instance
 * \param p_e an initialized exception pointer
 * \return the libvlc_drawable_t where the media instance
 *         should render its video
 */
VLC_PUBLIC_API libvlc_drawable_t
                    libvlc_media_player_get_drawable ( libvlc_media_player_t *, libvlc_exception_t * );

/** \bug This might go away ... to be replaced by a broader system */
VLC_PUBLIC_API libvlc_time_t  libvlc_media_player_get_length     ( libvlc_media_player_t *, libvlc_exception_t *);
VLC_PUBLIC_API libvlc_time_t  libvlc_media_player_get_time       ( libvlc_media_player_t *, libvlc_exception_t *);
VLC_PUBLIC_API void           libvlc_media_player_set_time       ( libvlc_media_player_t *, libvlc_time_t, libvlc_exception_t *);
VLC_PUBLIC_API float          libvlc_media_player_get_position   ( libvlc_media_player_t *, libvlc_exception_t *);
VLC_PUBLIC_API void           libvlc_media_player_set_position   ( libvlc_media_player_t *, float, libvlc_exception_t *);
VLC_PUBLIC_API void           libvlc_media_player_set_chapter    ( libvlc_media_player_t *, int, libvlc_exception_t *);
VLC_PUBLIC_API int            libvlc_media_player_get_chapter    (libvlc_media_player_t *, libvlc_exception_t *);
VLC_PUBLIC_API int            libvlc_media_player_get_chapter_count( libvlc_media_player_t *, libvlc_exception_t *);
VLC_PUBLIC_API int            libvlc_media_player_will_play      ( libvlc_media_player_t *, libvlc_exception_t *);
VLC_PUBLIC_API float          libvlc_media_player_get_rate       ( libvlc_media_player_t *, libvlc_exception_t *);
VLC_PUBLIC_API void           libvlc_media_player_set_rate       ( libvlc_media_player_t *, float, libvlc_exception_t *);
VLC_PUBLIC_API libvlc_state_t libvlc_media_player_get_state   ( libvlc_media_player_t *, libvlc_exception_t *);
VLC_PUBLIC_API float          libvlc_media_player_get_fps( libvlc_media_player_t *, libvlc_exception_t *);

/**
 * Does this media instance have a video output?
 *
 * \param p_md the media instance
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API int  libvlc_media_player_has_vout( libvlc_media_player_t *, libvlc_exception_t *);

/**
 * Is this media instance seekable?
 *
 * \param p_input the input
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API int libvlc_media_player_is_seekable( libvlc_media_player_t *p_mi, libvlc_exception_t *p_e );

/**
 * Can this media instance be paused?
 *
 * \param p_input the input
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API int libvlc_media_player_can_pause( libvlc_media_player_t *p_mi, libvlc_exception_t *p_e );

/** \defgroup libvlc_video libvlc_video
 * \ingroup libvlc_media_player
 * LibVLC Video handling
 * @{
 */

/**
 * Toggle fullscreen status on video output.
 *
 * \param p_input the input
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_toggle_fullscreen( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Enable or disable fullscreen on a video output.
 *
 * \param p_input the input
 * \param b_fullscreen boolean for fullscreen status
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_set_fullscreen( libvlc_media_player_t *, int, libvlc_exception_t * );

/**
 * Get current fullscreen status.
 *
 * \param p_input the input
 * \param p_e an initialized exception pointer
 * \return the fullscreen status (boolean)
 */
VLC_PUBLIC_API int libvlc_get_fullscreen( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Get current video height.
 *
 * \param p_input the input
 * \param p_e an initialized exception pointer
 * \return the video height
 */
VLC_PUBLIC_API int libvlc_video_get_height( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Get current video width.
 *
 * \param p_input the input
 * \param p_e an initialized exception pointer
 * \return the video width
 */
VLC_PUBLIC_API int libvlc_video_get_width( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Get current video aspect ratio.
 *
 * \param p_input the input
 * \param p_e an initialized exception pointer
 * \return the video aspect ratio
 */
VLC_PUBLIC_API char *libvlc_video_get_aspect_ratio( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Set new video aspect ratio.
 *
 * \param p_input the input
 * \param psz_aspect new video aspect-ratio
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_video_set_aspect_ratio( libvlc_media_player_t *, char *, libvlc_exception_t * );

/**
 * Get current video subtitle.
 *
 * \param p_input the input
 * \param p_e an initialized exception pointer
 * \return the video subtitle selected
 */
VLC_PUBLIC_API int libvlc_video_get_spu( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Set new video subtitle.
 *
 * \param p_input the input
 * \param i_spu new video subtitle to select
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_video_set_spu( libvlc_media_player_t *, int , libvlc_exception_t * );

/**
 * Get current crop filter geometry.
 *
 * \param p_input the input
 * \param p_e an initialized exception pointer
 * \return the crop filter geometry
 */
VLC_PUBLIC_API char *libvlc_video_get_crop_geometry( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Set new crop filter geometry.
 *
 * \param p_input the input
 * \param psz_geometry new crop filter geometry
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_video_set_crop_geometry( libvlc_media_player_t *, char *, libvlc_exception_t * );

/**
 * Toggle teletext transparent status on video output.
 *
 * \param p_input the input
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_toggle_teletext( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Get current teletext page requested.
 *
 * \param p_input the input
 * \param p_e an initialized exception pointer
 * \return the current teletext page requested.
 */
VLC_PUBLIC_API int libvlc_video_get_teletext( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Set new teletext page to retrieve.
 *
 * \param p_input the input
 * \param i_page teletex page number requested
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_video_set_teletext( libvlc_media_player_t *, int, libvlc_exception_t * );

/**
 * Take a snapshot of the current video window.
 *
 * If i_width AND i_height is 0, original size is used.
 * If i_width XOR i_height is 0, original aspect-ratio is preserved.
 *
 * \param p_input the input
 * \param psz_filepath the path where to save the screenshot to
 * \param i_width the snapshot's width
 * \param i_height the snapshot's height
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_video_take_snapshot( libvlc_media_player_t *, char *,unsigned int, unsigned int, libvlc_exception_t * );

VLC_PUBLIC_API int libvlc_video_destroy( libvlc_media_player_t *, libvlc_exception_t *);

/**
 * Resize the current video output window.
 *
 * \param p_instance libvlc instance
 * \param width new width for video output window
 * \param height new height for video output window
 * \param p_e an initialized exception pointer
 * \return the success status (boolean)
 */
VLC_PUBLIC_API void libvlc_video_resize( libvlc_media_player_t *, int, int, libvlc_exception_t *);

/**
 * Change the parent for the current the video output.
 *
 * \param p_instance libvlc instance
 * \param drawable the new parent window (Drawable on X11, CGrafPort on MacOSX, HWND on Win32)
 * \param p_e an initialized exception pointer
 * \return the success status (boolean)
 */
VLC_PUBLIC_API int libvlc_video_reparent( libvlc_media_player_t *, libvlc_drawable_t, libvlc_exception_t * );

/**
 * Tell windowless video output to redraw rectangular area (MacOS X only).
 *
 * \param p_instance libvlc instance
 * \param area coordinates within video drawable
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_video_redraw_rectangle( libvlc_media_player_t *, const libvlc_rectangle_t *, libvlc_exception_t * );

/**
 * Set the default video output's parent.
 *
 * This setting will be used as default for all video outputs.
 *
 * \param p_instance libvlc instance
 * \param drawable the new parent window (Drawable on X11, CGrafPort on MacOSX, HWND on Win32)
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_video_set_parent( libvlc_instance_t *, libvlc_drawable_t, libvlc_exception_t * );

/**
 * Set the default video output parent.
 *
 * This setting will be used as default for all video outputs.
 *
 * \param p_instance libvlc instance
 * \param drawable the new parent window (Drawable on X11, CGrafPort on MacOSX, HWND on Win32)
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API libvlc_drawable_t libvlc_video_get_parent( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Set the default video output size.
 *
 * This setting will be used as default for all video outputs.
 *
 * \param p_instance libvlc instance
 * \param width new width for video drawable
 * \param height new height for video drawable
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_video_set_size( libvlc_instance_t *, int, int, libvlc_exception_t * );

/**
 * Set the default video output viewport for a windowless video output
 * (MacOS X only).
 *
 * This setting will be used as default for all video outputs.
 *
 * \param p_instance libvlc instance
 * \param view coordinates within video drawable
 * \param clip coordinates within video drawable
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_video_set_viewport( libvlc_instance_t *, const libvlc_rectangle_t *, const libvlc_rectangle_t *, libvlc_exception_t * );

/** @} video */

/** \defgroup libvlc_audio libvlc_audio
 * \ingroup libvlc_media_player
 * LibVLC Audio handling
 * @{
 */

/**
 * Toggle mute status.
 *
 * \param p_instance libvlc instance
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_audio_toggle_mute( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Get current mute status.
 *
 * \param p_instance libvlc instance
 * \param p_e an initialized exception pointer
 * \return the mute status (boolean)
 */
VLC_PUBLIC_API int libvlc_audio_get_mute( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Set mute status.
 *
 * \param p_instance libvlc instance
 * \param status If status is true then mute, otherwise unmute
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_audio_set_mute( libvlc_instance_t *, int , libvlc_exception_t * );

/**
 * Get current audio level.
 *
 * \param p_instance libvlc instance
 * \param p_e an initialized exception pointer
 * \return the audio level (int)
 */
VLC_PUBLIC_API int libvlc_audio_get_volume( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Set current audio level.
 *
 * \param p_instance libvlc instance
 * \param i_volume the volume (int)
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_audio_set_volume( libvlc_instance_t *, int, libvlc_exception_t *);

/**
 * Get number of available audio tracks.
 *
 * \param p_mi media instance
 * \param p_e an initialized exception
 * \return the number of available audio tracks (int)
 */
VLC_PUBLIC_API int libvlc_audio_get_track_count( libvlc_media_player_t *,  libvlc_exception_t * );

/**
 * Get current audio track.
 *
 * \param p_input input instance
 * \param p_e an initialized exception pointer
 * \return the audio track (int)
 */
VLC_PUBLIC_API int libvlc_audio_get_track( libvlc_media_player_t *, libvlc_exception_t * );

/**
 * Set current audio track.
 *
 * \param p_input input instance
 * \param i_track the track (int)
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_audio_set_track( libvlc_media_player_t *, int, libvlc_exception_t * );

/**
 * Get current audio channel.
 *
 * \param p_instance input instance
 * \param p_e an initialized exception pointer
 * \return the audio channel (int)
 */
VLC_PUBLIC_API int libvlc_audio_get_channel( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Set current audio channel.
 *
 * \param p_instance input instance
 * \param i_channel the audio channel (int)
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_audio_set_channel( libvlc_instance_t *, int, libvlc_exception_t * );

/** @} audio */

/** @} media_player */

/*****************************************************************************
 * Event handling
 *****************************************************************************/

/** \defgroup libvlc_event libvlc_event
 * \ingroup libvlc_core
 * LibVLC Events
 * @{
 */

/**
 * Register for an event notification.
 *
 * \param p_event_manager the event manager to which you want to attach to.
 *        Generally it is obtained by vlc_my_object_event_manager() where
 *        my_object is the object you want to listen to.
 * \param i_event_type the desired event to which we want to listen
 * \param f_callback the function to call when i_event_type occurs
 * \param user_data user provided data to carry with the event
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_event_attach( libvlc_event_manager_t *p_event_manager,
                                         libvlc_event_type_t i_event_type,
                                         libvlc_callback_t f_callback,
                                         void *user_data,
                                         libvlc_exception_t *p_e );

/**
 * Unregister an event notification.
 *
 * \param p_event_manager the event manager
 * \param i_event_type the desired event to which we want to unregister
 * \param f_callback the function to call when i_event_type occurs
 * \param p_user_data user provided data to carry with the event
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_event_detach( libvlc_event_manager_t *p_event_manager,
                                         libvlc_event_type_t i_event_type,
                                         libvlc_callback_t f_callback,
                                         void *p_user_data,
                                         libvlc_exception_t *p_e );

/**
 * Get an event's type name.
 *
 * \param i_event_type the desired event
 */
VLC_PUBLIC_API const char * libvlc_event_type_name( libvlc_event_type_t event_type );

/** @} */

/*****************************************************************************
 * Media Library
 *****************************************************************************/
/** \defgroup libvlc_media_library libvlc_media_library
 * \ingroup libvlc
 * LibVLC Media Library
 * @{
 */
VLC_PUBLIC_API libvlc_media_library_t *
    libvlc_media_library_new( libvlc_instance_t * p_inst,
                              libvlc_exception_t * p_e );
VLC_PUBLIC_API void
    libvlc_media_library_release( libvlc_media_library_t * p_mlib );
VLC_PUBLIC_API void
    libvlc_media_library_retain( libvlc_media_library_t * p_mlib );


VLC_PUBLIC_API void
    libvlc_media_library_load( libvlc_media_library_t * p_mlib,
                               libvlc_exception_t * p_e );

VLC_PUBLIC_API void
    libvlc_media_library_save( libvlc_media_library_t * p_mlib,
                               libvlc_exception_t * p_e );

VLC_PUBLIC_API libvlc_media_list_t *
    libvlc_media_library_media_list( libvlc_media_library_t * p_mlib,
                                     libvlc_exception_t * p_e );


/** @} */


/*****************************************************************************
 * Services/Media Discovery
 *****************************************************************************/
/** \defgroup libvlc_media_discoverer libvlc_media_discoverer
 * \ingroup libvlc
 * LibVLC Media Discoverer
 * @{
 */

VLC_PUBLIC_API libvlc_media_discoverer_t *
libvlc_media_discoverer_new_from_name( libvlc_instance_t * p_inst,
                                       const char * psz_name,
                                       libvlc_exception_t * p_e );
VLC_PUBLIC_API void   libvlc_media_discoverer_release( libvlc_media_discoverer_t * p_mdis );
VLC_PUBLIC_API char * libvlc_media_discoverer_localized_name( libvlc_media_discoverer_t * p_mdis );

VLC_PUBLIC_API libvlc_media_list_t * libvlc_media_discoverer_media_list( libvlc_media_discoverer_t * p_mdis );

VLC_PUBLIC_API libvlc_event_manager_t *
        libvlc_media_discoverer_event_manager( libvlc_media_discoverer_t * p_mdis );

VLC_PUBLIC_API int
        libvlc_media_discoverer_is_running( libvlc_media_discoverer_t * p_mdis );

/**@} */


/*****************************************************************************
 * Message log handling
 *****************************************************************************/

/** \defgroup libvlc_log libvlc_log
 * \ingroup libvlc_core
 * LibVLC Message Logging
 * @{
 */

/**
 * Return the VLC messaging verbosity level.
 *
 * \param p_instance libvlc instance
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API unsigned libvlc_get_log_verbosity( const libvlc_instance_t *p_instance,
                                                  libvlc_exception_t *p_e );

/**
 * Set the VLC messaging verbosity level.
 *
 * \param p_instance libvlc log instance
 * \param level log level
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_set_log_verbosity( libvlc_instance_t *p_instance, unsigned level,
                                              libvlc_exception_t *p_e );

/**
 * Open a VLC message log instance.
 *
 * \param p_instance libvlc instance
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API libvlc_log_t *libvlc_log_open( libvlc_instance_t *, libvlc_exception_t *);

/**
 * Close a VLC message log instance.
 *
 * \param p_log libvlc log instance
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_log_close( libvlc_log_t *, libvlc_exception_t *);

/**
 * Returns the number of messages in a log instance.
 *
 * \param p_log libvlc log instance
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API unsigned libvlc_log_count( const libvlc_log_t *, libvlc_exception_t *);

/**
 * Clear a log instance.
 *
 * All messages in the log are removed. The log should be cleared on a
 * regular basis to avoid clogging.
 *
 * \param p_log libvlc log instance
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_log_clear( libvlc_log_t *, libvlc_exception_t *);

/**
 * Allocate and returns a new iterator to messages in log.
 *
 * \param p_log libvlc log instance
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API libvlc_log_iterator_t *libvlc_log_get_iterator( const libvlc_log_t *, libvlc_exception_t *);

/**
 * Release a previoulsy allocated iterator.
 *
 * \param p_iter libvlc log iterator
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API void libvlc_log_iterator_free( libvlc_log_iterator_t *p_iter, libvlc_exception_t *p_e );

/**
 * Return whether log iterator has more messages.
 *
 * \param p_iter libvlc log iterator
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API int libvlc_log_iterator_has_next( const libvlc_log_iterator_t *p_iter, libvlc_exception_t *p_e );

/**
 * Return the next log message.
 *
 * The message contents must not be freed
 *
 * \param p_iter libvlc log iterator
 * \param p_buffer log buffer
 * \param p_e an initialized exception pointer
 */
VLC_PUBLIC_API libvlc_log_message_t *libvlc_log_iterator_next( libvlc_log_iterator_t *p_iter,
                                                               libvlc_log_message_t *p_buffer,
                                                               libvlc_exception_t *p_e );

/** @} */

# ifdef __cplusplus
}
# endif

#endif /* <vlc/libvlc.h> */
