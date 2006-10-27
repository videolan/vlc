/*****************************************************************************
 * libvlc.h:  libvlc_* new external API
 *****************************************************************************
 * Copyright (C) 1998-2005 the VideoLAN team
 * $Id: vlc.h 13701 2005-12-12 17:58:56Z zorglub $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
 * \defgroup libvlc Libvlc
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
/** defgroup libvlc_exception Exceptions
 * \ingroup libvlc
 * LibVLC Exceptions handling
 * @{
 */

struct libvlc_exception_t
{
    int b_raised;
    char *psz_message;
};
typedef struct libvlc_exception_t libvlc_exception_t;

/**
 * Initialize an exception structure. This can be called several times to reuse
 * an exception structure.
 * \param p_exception the exception to initialize
 */
void libvlc_exception_init( libvlc_exception_t *p_exception );

/**
 * Has an exception been raised ?
 * \param p_exception the exception to query
 * \return 0 if no exception raised, 1 else
 */
int libvlc_exception_raised( libvlc_exception_t *p_exception );

/**
 * Raise an exception
 * \param p_exception the exception to raise
 * \param psz_message the exception message
 */
void libvlc_exception_raise( libvlc_exception_t *p_exception, const char *psz_format, ... );

/**
 * Clear an exception object so it can be reused.
 * The exception object must be initialized
 * \param p_exception the exception to clear
 */
void libvlc_exception_clear( libvlc_exception_t * );

/**
 * Get exception message
 * \param p_exception the exception to query
 * \return the exception message or NULL if not applicable (exception not raised
 * for example)
 */
char* libvlc_exception_get_message( libvlc_exception_t *p_exception );

/**@} */

/*****************************************************************************
 * Core handling
 *****************************************************************************/

/** defgroup libvlc_core Core
 * \ingroup libvlc
 * LibVLC Core
 * @{
 */

/** This structure is opaque. It represents a libvlc instance */
typedef struct libvlc_instance_t libvlc_instance_t;

/**
 * Create an initialized libvlc instance
 * \param argc the number of arguments
 * \param argv command-line-type arguments
 * \param exception an initialized exception pointer
 */
libvlc_instance_t * libvlc_new( int , char **, libvlc_exception_t *);

/**
 * Returns a libvlc instance identifier for legacy APIs. Use of this
 * function is discouraged, you should convert your program to use the
 * new API.
 * \param p_instance the instance
 */
int libvlc_get_vlc_id( libvlc_instance_t *p_instance );

/**
 * Destroy a libvlc instance.
 * \param p_instance the instance to destroy
 */
void libvlc_destroy( libvlc_instance_t *, libvlc_exception_t * );

/** @}*/

/*****************************************************************************
 * Playlist
 *****************************************************************************/
/** defgroup libvlc_playlist Playlist
 * \ingroup libvlc
 * LibVLC Playlist handling
 * @{
 */
/**
 * Set loop variable
 */
void libvlc_playlist_loop( libvlc_instance_t* , vlc_bool_t, libvlc_exception_t * );
/**
 * Start playing. You can give some additionnal playlist item options
 * that will be added to the item before playing it.
 * \param p_instance the instance
 * \param i_id the item to play. If this is a negative number, the next
 * item will be selected. Else, the item with the given ID will be played
 * \param i_options the number of options to add to the item
 * \param ppsz_options the options to add to the item
 * \param p_exception an initialized exception
 */
void libvlc_playlist_play( libvlc_instance_t*, int, int, char **,
                           libvlc_exception_t * );

/**
 * Pause a running playlist, resume if it was stopped
 * \param p_instance the instance to pause
 * \param p_exception an initialized exception
 */
void libvlc_playlist_pause( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Checks if the playlist is running
 * \param p_instance the instance
 * \param p_exception an initialized exception
 * \return 0 if the playlist is stopped or paused, 1 if it is running
 */
int libvlc_playlist_isplaying( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Get the number of items in the playlist
 * \param p_instance the instance
 * \param p_exception an initialized exception
 * \return the number of items
 */
int libvlc_playlist_items_count( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Stop playing
 * \param p_instance the instance to stop
 * \param p_exception an initialized exception
 */
void libvlc_playlist_stop( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Go to next playlist item (starts playback if it was stopped)
 * \param p_instance the instance to use
 * \param p_exception an initialized exception
 */
void libvlc_playlist_next( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Go to previous playlist item (starts playback if it was stopped)
 * \param p_instance the instance to use
 * \param p_exception an initialized exception
 */
void libvlc_playlist_prev( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Remove all playlist items
 * \param p_instance the instance
 * \param p_exception an initialized exception
 */
void libvlc_playlist_clear( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Add an item at the end of the playlist
 * If you need more advanced options, \see libvlc_playlist_add_extended
 * \param p_instance the instance
 * \param psz_uri the URI to open, using VLC format
 * \param psz_name a name that you might want to give or NULL
 * \return the identifier of the new item
 */
int libvlc_playlist_add( libvlc_instance_t *, const char *, const char *,
                         libvlc_exception_t * );

/**
 * Add an item at the end of the playlist, with additional input options
 * \param p_instance the instance
 * \param psz_uri the URI to open, using VLC format
 * \param psz_name a name that you might want to give or NULL
 * \param i_options the number of options to add
 * \param ppsz_options strings representing the options to add
 * \param p_exception an initialized exception
 * \return the identifier of the new item
 */
int libvlc_playlist_add_extended( libvlc_instance_t *, const char *,
                                  const char *, int, const char **,
                                  libvlc_exception_t * );

/** 
 * Delete the playlist item with the given ID.
 * \param p_instance the instance
 * \param i_id the id to remove
 * \param p_exception an initialized exception
 * \return
 */
int libvlc_playlist_delete_item( libvlc_instance_t *, int,
                                 libvlc_exception_t * );
    
typedef struct libvlc_input_t libvlc_input_t;

/* Get the input that is currently being played by the playlist
 * \param p_instance the instance to use
 * \param p_exception an initialized excecption
 * \return an input object
 */
libvlc_input_t *libvlc_playlist_get_input( libvlc_instance_t *,
                                           libvlc_exception_t * );

/** @}*/

/*****************************************************************************
 * Input
 *****************************************************************************/
/** defgroup libvlc_input Input
 * \ingroup libvlc
 * LibVLC Input handling
 * @{
 */

/** Free an input object
 * \param p_input the input to free
 */
void libvlc_input_free( libvlc_input_t * );

/// \bug This might go away ... to be replaced by a broader system
vlc_int64_t libvlc_input_get_length     ( libvlc_input_t *, libvlc_exception_t *);
vlc_int64_t libvlc_input_get_time       ( libvlc_input_t *, libvlc_exception_t *);
void        libvlc_input_set_time       ( libvlc_input_t *, vlc_int64_t, libvlc_exception_t *);
float       libvlc_input_get_position   ( libvlc_input_t *, libvlc_exception_t *);
void        libvlc_input_set_position   ( libvlc_input_t *, float, libvlc_exception_t *);
vlc_bool_t  libvlc_input_will_play      ( libvlc_input_t *, libvlc_exception_t *);
float       libvlc_input_get_rate       ( libvlc_input_t *, libvlc_exception_t *);
void        libvlc_input_set_rate       ( libvlc_input_t *, float, libvlc_exception_t *);
int         libvlc_input_get_state      ( libvlc_input_t *, libvlc_exception_t *);
        
/** @} */

/** defgroup libvlc_video Video
 * \ingroup libvlc
 * LibVLC Video handling
 * @{
 */

/**
 * Does this input have a video output ?
 * \param p_input the input
 * \param p_exception an initialized exception
 */
vlc_bool_t  libvlc_input_has_vout       ( libvlc_input_t *, libvlc_exception_t *);
float       libvlc_input_get_fps        ( libvlc_input_t *, libvlc_exception_t *);

/**
 * Toggle fullscreen status on video output
 * \param p_input the input
 * \param p_exception an initialized exception
 */
void libvlc_toggle_fullscreen( libvlc_input_t *, libvlc_exception_t * );

/**
 * Enable or disable fullscreen on a video output
 * \param p_input the input
 * \param b_fullscreen boolean for fullscreen status
 * \param p_exception an initialized exception
 */
void libvlc_set_fullscreen( libvlc_input_t *, int, libvlc_exception_t * );

/**
 * Get current fullscreen status
 * \param p_input the input
 * \param p_exception an initialized exception
 * \return the fullscreen status (boolean)
 */
int libvlc_get_fullscreen( libvlc_input_t *, libvlc_exception_t * );
    
/**
 * Get current video height
 * \param p_input the input
 * \param p_exception an initialized exception
 * \return the video height
 */
int libvlc_video_get_height( libvlc_input_t *, libvlc_exception_t * );

/**
 * Get current video width
 * \param p_input the input
 * \param p_exception an initialized exception
 * \return the video width
 */
int libvlc_video_get_width( libvlc_input_t *, libvlc_exception_t * );

/**
 * Take a snapshot of the current video window
 * \param p_input the input
 * \param psz_filepath the path where to save the screenshot to
 * \param p_exception an initialized exception
 */
void libvlc_video_take_snapshot( libvlc_input_t *, char *, libvlc_exception_t * );
    
int libvlc_video_destroy( libvlc_input_t *, libvlc_exception_t *);

    
/**
 * Resize the current video output window
 * \param p_instance libvlc instance
 * \param width new width for video output window
 * \param height new height for video output window
 * \param p_exception an initialized exception
 * \return the mute status (boolean)
 */
void libvlc_video_resize( libvlc_input_t *, int, int, libvlc_exception_t *);
    
/**
* Downcast to this general type as placeholder for a platform specific one, such as:
*  Drawable on X11,
*  CGrafPort on MacOSX,
*  HWND on win32
*/
typedef int libvlc_drawable_t;

/**
 * change the parent for the current the video output
 * \param p_instance libvlc instance
 * \param drawable the new parent window (Drawable on X11, CGrafPort on MacOSX, HWND on Win32)
 * \param p_exception an initialized exception
 * \return the mute status (boolean)
 */
int libvlc_video_reparent( libvlc_input_t *, libvlc_drawable_t, libvlc_exception_t * );

/**
 * Set the default video output parent
 *  this settings will be used as default for all video outputs
 * \param p_instance libvlc instance
 * \param drawable the new parent window (Drawable on X11, CGrafPort on MacOSX, HWND on Win32)
 * \param p_exception an initialized exception
 */
void libvlc_video_set_parent( libvlc_instance_t *, libvlc_drawable_t, libvlc_exception_t * );

/**
 * Set the default video output size
 *  this settings will be used as default for all video outputs
 * \param p_instance libvlc instance
 * \param width new width for video drawable
 * \param height new height for video drawable
 * \param p_exception an initialized exception
 */
void libvlc_video_set_size( libvlc_instance_t *, int, int, libvlc_exception_t * );

/**
* Downcast to this general type as placeholder for a platform specific one, such as:
*  Drawable on X11,
*  CGrafPort on MacOSX,
*  HWND on win32
*/
typedef struct
{
    int top, left;
    int bottom, right;
}
libvlc_rectangle_t;

/**
 * Set the default video output viewport for a windowless video output (MacOS X only)
 *  this settings will be used as default for all video outputs
 * \param p_instance libvlc instance
 * \param view coordinates within video drawable
 * \param clip coordinates within video drawable
 * \param p_exception an initialized exception
 */
void libvlc_video_set_viewport( libvlc_instance_t *, const libvlc_rectangle_t *, const libvlc_rectangle_t *, libvlc_exception_t * );


/** @} */

/**
 * defgroup libvlc_vlm VLM
 * \ingroup libvlc
 * LibVLC VLM handling
 * @{
 */

/** defgroup libvlc_audio Audio
 * \ingroup libvlc
 * LibVLC Audio handling
 * @{
 */

/**
 * Toggle mute status
 * \param p_instance libvlc instance
 * \param p_exception an initialized exception
 * \return void
 */
void libvlc_audio_toggle_mute( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Get current mute status
 * \param p_instance libvlc instance
 * \param p_exception an initialized exception
 * \return the mute status (boolean)
 */
vlc_bool_t libvlc_audio_get_mute( libvlc_instance_t *, libvlc_exception_t * );

/**
 * Set mute status
 * \param p_instance libvlc instance
 * \param status If status is VLC_TRUE then mute, otherwise unmute
 * \param p_exception an initialized exception
 * \return void
 */
void libvlc_audio_set_mute( libvlc_instance_t *, vlc_bool_t , libvlc_exception_t * );


/**
 * Get current audio level
 * \param p_instance libvlc instance
 * \param p_exception an initialized exception
 * \return the audio level (int)
 */
int libvlc_audio_get_volume( libvlc_instance_t *, libvlc_exception_t * );


/**
 * Set current audio level
 * \param p_instance libvlc instance
 * \param i_volume the volume (int)
 * \param p_exception an initialized exception
 * \return void
 */
void libvlc_audio_set_volume( libvlc_instance_t *, int , libvlc_exception_t *);

/** @} */


/**
 * Add a broadcast, with one input
 * \param p_instance the instance
 * \param psz_name the name of the new broadcast
 * \param psz_input the input MRL
 * \param psz_output the output MRL (the parameter to the "sout" variable)
 * \param i_options number of additional options
 * \param ppsz_options additional options
 * \param b_enabled boolean for enabling the new broadcast
 * \param b_loop Should this broadcast be played in loop ?
 * \param p_exception an initialized exception
 */
void libvlc_vlm_add_broadcast( libvlc_instance_t *, char *, char *, char* ,
                               int, char **, int, int, libvlc_exception_t * );

/**
 * Delete a media (vod or broadcast)
 * \param p_instance the instance
 * \param psz_name the media to delete
 * \param p_exception an initialized exception
 */
void libvlc_vlm_del_media( libvlc_instance_t *, char *, libvlc_exception_t * );

/**
 * Enable or disable a media (vod or broadcast)
 * \param p_instance the instance
 * \param psz_name the media to work on
 * \param b_enabled the new status
 * \param p_exception an initialized exception
 */
void libvlc_vlm_set_enabled( libvlc_instance_t *, char *, int,
                             libvlc_exception_t *);

/**
 * Set the output for a media
 * \param p_instance the instance
 * \param psz_name the media to work on
 * \param psz_output the output MRL (the parameter to the "sout" variable)
 * \param p_exception an initialized exception
 */
void libvlc_vlm_set_output( libvlc_instance_t *, char *, char*,
                            libvlc_exception_t *);

/**
 * Set a media's input MRL. This will delete all existing inputs and
 * add the specified one.
 * \param p_instance the instance
 * \param psz_name the media to work on
 * \param psz_input the input MRL
 * \param p_exception an initialized exception
 */
void libvlc_vlm_set_input( libvlc_instance_t *, char *, char*,
                           libvlc_exception_t *);

/**
 * Set output for a media
 * \param p_instance the instance
 * \param psz_name the media to work on
 * \param b_loop the new status
 * \param p_exception an initialized exception
 */
void libvlc_vlm_set_loop( libvlc_instance_t *, char *, int,
                          libvlc_exception_t *);




/**
 * Edit the parameters of a media. This will delete all existing inputs and
 * add the specified one.
 * \param p_instance the instance
 * \param psz_name the name of the new broadcast
 * \param psz_input the input MRL
 * \param psz_output the output MRL (the parameter to the "sout" variable)
 * \param i_options number of additional options
 * \param ppsz_options additional options
 * \param b_enabled boolean for enabling the new broadcast
 * \param b_loop Should this broadcast be played in loop ?
 * \param p_exception an initialized exception
 */
void libvlc_vlm_change_media( libvlc_instance_t *, char *, char *, char* ,
                              int, char **, int, int, libvlc_exception_t * );

/**
 * Plays the named broadcast.
 * \param p_instance the instance
 * \param psz_name the name of the broadcast
 * \param p_exception an initialized exception
 */ 
void libvlc_vlm_play_media ( libvlc_instance_t *, char *, libvlc_exception_t * );

/**
 * Stops the named broadcast.
 * \param p_instance the instance
 * \param psz_name the name of the broadcast
 * \param p_exception an initialized exception
 */ 
void libvlc_vlm_stop_media ( libvlc_instance_t *, char *, libvlc_exception_t * );

    
/**
 * Pauses the named broadcast.
 * \param p_instance the instance
 * \param psz_name the name of the broadcast
 * \param p_exception an initialized exception
 */ 
void libvlc_vlm_pause_media( libvlc_instance_t *, char *, libvlc_exception_t * );
    
/**
 * Seeks in the named broadcast.
 * \param p_instance the instance
 * \param psz_name the name of the broadcast
 * \param f_percentage the percentage to seek to
 * \param p_exception an initialized exception
 */ 
void libvlc_vlm_seek_media( libvlc_instance_t *, char *,
                            float, libvlc_exception_t * );
   
/**
 * Return information of the named broadcast.
 * \param p_instance the instance
 * \param psz_name the name of the broadcast
 * \param p_exception an initialized exception
 */ 
char* libvlc_vlm_show_media( libvlc_instance_t *, char *, libvlc_exception_t * );

#define LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( attr, returnType, getType, default)\
returnType libvlc_vlm_get_media_## attr( libvlc_instance_t *, \
                        char *, int , libvlc_exception_t * );

LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( position, float, Float, -1);
LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( time, int, Integer, -1);
LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( length, int, Integer, -1);
LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( rate, int, Integer, -1);
LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( title, int, Integer, 0);
LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( chapter, int, Integer, 0);
LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( seekable, int, Bool, 0);

#undef LIBVLC_VLM_GET_MEDIA_ATTRIBUTE

/** @} */
/** @} */

/*****************************************************************************
 * Message log handling
 *****************************************************************************/

/** defgroup libvlc_log Log
 * \ingroup libvlc
 * LibVLC Message Logging
 * @{
 */

/** This structure is opaque. It represents a libvlc log instance */
typedef struct libvlc_log_t libvlc_log_t;

/** This structure is opaque. It represents a libvlc log iterator */
typedef struct libvlc_log_iterator_t libvlc_log_iterator_t;

typedef struct libvlc_log_message_t
{
    unsigned    sizeof_msg;   /* sizeof() of message structure, must be filled in by user */
    int         i_severity;   /* 0=INFO, 1=ERR, 2=WARN, 3=DBG */
    const char *psz_type;     /* module type */
    const char *psz_name;     /* module name */
    const char *psz_header;   /* optional header */
    const char *psz_message;  /* message */
} libvlc_log_message_t;
/**
 * Returns the VLC messaging verbosity level
 * \param p_instance libvlc instance
 * \param exception an initialized exception pointer
 */
unsigned libvlc_get_log_verbosity( const libvlc_instance_t *p_instance, libvlc_exception_t *p_e );

/**
 * Set the VLC messaging verbosity level
 * \param p_log libvlc log instance
 * \param exception an initialized exception pointer
 */
void libvlc_set_log_verbosity( libvlc_instance_t *p_instance, unsigned level, libvlc_exception_t *p_e );

/**
 * Open an instance to VLC message log 
 * \param p_instance libvlc instance
 * \param exception an initialized exception pointer
 */
libvlc_log_t *libvlc_log_open( const libvlc_instance_t *, libvlc_exception_t *);

/**
 * Close an instance of VLC message log 
 * \param p_log libvlc log instance
 * \param exception an initialized exception pointer
 */
void libvlc_log_close( libvlc_log_t *, libvlc_exception_t *);

/**
 * Returns the number of messages in log
 * \param p_log libvlc log instance
 * \param exception an initialized exception pointer
 */
unsigned libvlc_log_count( const libvlc_log_t *, libvlc_exception_t *);

/**
 * Clear all messages in log
 *  the log should be cleared on a regular basis to avoid clogging
 * \param p_log libvlc log instance
 * \param exception an initialized exception pointer
 */
void libvlc_log_clear( libvlc_log_t *, libvlc_exception_t *);

/**
 * Allocate and returns a new iterator to messages in log
 * \param p_log libvlc log instance
 * \param exception an initialized exception pointer
 */
libvlc_log_iterator_t *libvlc_log_get_iterator( const libvlc_log_t *, libvlc_exception_t *);

/**
 * Releases a previoulsy allocated iterator
 * \param p_log libvlc log iterator 
 * \param exception an initialized exception pointer
 */
void libvlc_log_iterator_free( libvlc_log_iterator_t *p_iter, libvlc_exception_t *p_e );

/**
 * Returns whether log iterator has more messages 
 * \param p_log libvlc log iterator
 * \param exception an initialized exception pointer
 */
int libvlc_log_iterator_has_next( const libvlc_log_iterator_t *p_iter, libvlc_exception_t *p_e );

/**
 * Returns next log message
 *   the content of message must not be freed
 * \param p_log libvlc log iterator
 * \param exception an initialized exception pointer
 */
libvlc_log_message_t *libvlc_log_iterator_next( libvlc_log_iterator_t *p_iter,
                                                struct libvlc_log_message_t *buffer,
                                                libvlc_exception_t *p_e );

/** @} */

# ifdef __cplusplus
}
# endif

#endif /* <vlc/libvlc.h> */
