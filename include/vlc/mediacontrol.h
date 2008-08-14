/*****************************************************************************
 * mediacontrol.h: global header for mediacontrol
 *****************************************************************************
 * Copyright (C) 2005-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Olivier Aubert <olivier.aubert@liris.univ-lyon1.fr>
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
 * This file defines libvlc mediacontrol_* external API
 */

/**
 * \defgroup mediacontrol MediaControl
 * This is the MediaControl API, * intended to provide a generic API to movie players.
 *
 * @{
 */


#ifndef VLC_CONTROL_H
#define VLC_CONTROL_H 1

# ifdef __cplusplus
extern "C" {
# endif

#if defined( WIN32 )
#include <windows.h>
typedef HWND WINDOWHANDLE;
#else
typedef int WINDOWHANDLE;
#endif

#include <vlc/libvlc.h>
#include <vlc/mediacontrol_structures.h>

/**
 * mediacontrol_Instance is an opaque structure, defined in
 * mediacontrol_internal.h. API users do not have to mess with it.
 */
typedef struct mediacontrol_Instance mediacontrol_Instance;

/**************************************************************************
 *  Helper functions
 ***************************************************************************/

/**
 * Free a RGBPicture structure.
 * \param pic: the RGBPicture structure
 */
VLC_PUBLIC_API void mediacontrol_RGBPicture__free( mediacontrol_RGBPicture *pic );

VLC_PUBLIC_API void mediacontrol_PlaylistSeq__free( mediacontrol_PlaylistSeq *ps );

/**
 * Free a StreamInformation structure.
 * \param pic: the StreamInformation structure
 */
VLC_PUBLIC_API void
mediacontrol_StreamInformation__free( mediacontrol_StreamInformation* p_si );

/**
 * Instanciate and initialize an exception structure.
 * \return the exception
 */
VLC_PUBLIC_API mediacontrol_Exception *
  mediacontrol_exception_create( void );

/**
 * Initialize an existing exception structure.
 * \param p_exception the exception to initialize.
 */
VLC_PUBLIC_API void
  mediacontrol_exception_init( mediacontrol_Exception *exception );

/**
 * Clean up an existing exception structure after use.
 * \param p_exception the exception to clean up.
 */
VLC_PUBLIC_API void
mediacontrol_exception_cleanup( mediacontrol_Exception *exception );

/**
 * Free an exception structure created with mediacontrol_exception_create().
 * \return the exception
 */
VLC_PUBLIC_API void mediacontrol_exception_free(mediacontrol_Exception *exception);

/*****************************************************************************
 * Core functions
 *****************************************************************************/

/**
 * Create a MediaControl instance with parameters
 * \param argc the number of arguments
 * \param argv parameters
 * \param exception an initialized exception pointer
 * \return a mediacontrol_Instance
 */
VLC_PUBLIC_API mediacontrol_Instance *
mediacontrol_new( int argc, char **argv, mediacontrol_Exception *exception );

/**
 * Create a MediaControl instance from an existing libvlc instance
 * \param p_instance the libvlc instance
 * \param exception an initialized exception pointer
 * \return a mediacontrol_Instance
 */
VLC_PUBLIC_API mediacontrol_Instance *
mediacontrol_new_from_instance( libvlc_instance_t* p_instance,
                mediacontrol_Exception *exception );

/**
 * Get the associated libvlc instance
 * \param self: the mediacontrol instance
 * \return a libvlc instance
 */
VLC_PUBLIC_API libvlc_instance_t*
mediacontrol_get_libvlc_instance( mediacontrol_Instance* self );

/**
 * Get the associated libvlc_media_player
 * \param self: the mediacontrol instance
 * \return a libvlc_media_player_t instance
 */
VLC_PUBLIC_API libvlc_media_player_t*
mediacontrol_get_media_player( mediacontrol_Instance* self );

/**
 * Get the current position
 * \param self the mediacontrol instance
 * \param an_origin the position origin
 * \param a_key the position unit
 * \param exception an initialized exception pointer
 * \return a mediacontrol_Position
 */
VLC_PUBLIC_API mediacontrol_Position * mediacontrol_get_media_position(
                         mediacontrol_Instance *self,
                         const mediacontrol_PositionOrigin an_origin,
                         const mediacontrol_PositionKey a_key,
                         mediacontrol_Exception *exception );

/**
 * Set the position
 * \param self the mediacontrol instance
 * \param a_position a mediacontrol_Position
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API void mediacontrol_set_media_position( mediacontrol_Instance *self,
                                      const mediacontrol_Position *a_position,
                                      mediacontrol_Exception *exception );

/**
 * Play the movie at a given position
 * \param self the mediacontrol instance
 * \param a_position a mediacontrol_Position
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API void mediacontrol_start( mediacontrol_Instance *self,
                         const mediacontrol_Position *a_position,
                         mediacontrol_Exception *exception );

/**
 * Pause the movie at a given position
 * \param self the mediacontrol instance
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API void mediacontrol_pause( mediacontrol_Instance *self,
                         mediacontrol_Exception *exception );

/**
 * Resume the movie at a given position
 * \param self the mediacontrol instance
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API void mediacontrol_resume( mediacontrol_Instance *self,
                          mediacontrol_Exception *exception );

/**
 * Stop the movie at a given position
 * \param self the mediacontrol instance
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API void mediacontrol_stop( mediacontrol_Instance *self,
                        mediacontrol_Exception *exception );

/**
 * Exit the player
 * \param self the mediacontrol instance
 */
VLC_PUBLIC_API void mediacontrol_exit( mediacontrol_Instance *self );

/**
 * Set the MRL to be played.
 * \param self the mediacontrol instance
 * \param psz_file the MRL
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API void mediacontrol_set_mrl( mediacontrol_Instance *self,
                                     const char* psz_file,
                                     mediacontrol_Exception *exception );

/**
 * Get the MRL to be played.
 * \param self the mediacontrol instance
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API char * mediacontrol_get_mrl( mediacontrol_Instance *self,
                                            mediacontrol_Exception *exception );

/*****************************************************************************
 * A/V functions
 *****************************************************************************/
/**
 * Get a snapshot
 * \param self the mediacontrol instance
 * \param a_position the desired position (ignored for now)
 * \param exception an initialized exception pointer
 * \return a RGBpicture
 */
VLC_PUBLIC_API mediacontrol_RGBPicture *
  mediacontrol_snapshot( mediacontrol_Instance *self,
                         const mediacontrol_Position *a_position,
                         mediacontrol_Exception *exception );

/**
 *  Displays the message string, between "begin" and "end" positions.
 * \param self the mediacontrol instance
 * \param message the message to display
 * \param begin the begin position
 * \param end the end position
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API void mediacontrol_display_text( mediacontrol_Instance *self,
                                const char *message,
                                const mediacontrol_Position *begin,
                                const mediacontrol_Position *end,
                                mediacontrol_Exception *exception );

/**
 *  Get information about a stream
 * \param self the mediacontrol instance
 * \param a_key the time unit
 * \param exception an initialized exception pointer
 * \return a mediacontrol_StreamInformation
 */
VLC_PUBLIC_API mediacontrol_StreamInformation *
  mediacontrol_get_stream_information( mediacontrol_Instance *self,
                                       mediacontrol_PositionKey a_key,
                                       mediacontrol_Exception *exception );

/**
 * Get the current audio level, normalized in [0..100]
 * \param self the mediacontrol instance
 * \param exception an initialized exception pointer
 * \return the volume
 */
VLC_PUBLIC_API unsigned short
  mediacontrol_sound_get_volume( mediacontrol_Instance *self,
                                 mediacontrol_Exception *exception );
/**
 * Set the audio level
 * \param self the mediacontrol instance
 * \param volume the volume (normalized in [0..100])
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API void mediacontrol_sound_set_volume( mediacontrol_Instance *self,
                                    const unsigned short volume,
                                    mediacontrol_Exception *exception );

/**
 * Set the video output window
 * \param self the mediacontrol instance
 * \param visual_id the Xid or HWND, depending on the platform
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API int mediacontrol_set_visual( mediacontrol_Instance *self,
                                    WINDOWHANDLE visual_id,
                                    mediacontrol_Exception *exception );

/**
 * Get the current playing rate, in percent
 * \param self the mediacontrol instance
 * \param exception an initialized exception pointer
 * \return the rate
 */
VLC_PUBLIC_API int mediacontrol_get_rate( mediacontrol_Instance *self,
               mediacontrol_Exception *exception );

/**
 * Set the playing rate, in percent
 * \param self the mediacontrol instance
 * \param rate the desired rate
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API void mediacontrol_set_rate( mediacontrol_Instance *self,
                const int rate,
                mediacontrol_Exception *exception );

/**
 * Get current fullscreen status
 * \param self the mediacontrol instance
 * \param exception an initialized exception pointer
 * \return the fullscreen status
 */
VLC_PUBLIC_API int mediacontrol_get_fullscreen( mediacontrol_Instance *self,
                 mediacontrol_Exception *exception );

/**
 * Set fullscreen status
 * \param self the mediacontrol instance
 * \param b_fullscreen the desired status
 * \param exception an initialized exception pointer
 */
VLC_PUBLIC_API void mediacontrol_set_fullscreen( mediacontrol_Instance *self,
                  const int b_fullscreen,
                  mediacontrol_Exception *exception );

# ifdef __cplusplus
}
# endif

#endif

/** @} */
