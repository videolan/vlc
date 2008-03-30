/*****************************************************************************
 * deprecated.h:  libvlc deprecated API
 *****************************************************************************
 * Copyright (C) 1998-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Paul Saman <jpsaman _at_ m2x _dot_ nl>
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

#ifndef _LIBVLC_DEPRECATED_H
#define _LIBVLC_DEPRECATED_H 1

# ifdef __cplusplus
extern "C" {
# endif


/*****************************************************************************
 * Playlist (Deprecated)
 *****************************************************************************/
/** \defgroup libvlc_playlist libvlc_playlist (Deprecated)
 * \ingroup libvlc
 * LibVLC Playlist handling (Deprecated)
 * @deprecated Use media_list
 * @{
 */

/**
 * Set the playlist's loop attribute. If set, the playlist runs continuously
 * and wraps around when it reaches the end.
 *
 * \param p_instance the playlist instance
 * \param loop the loop attribute. 1 sets looping, 0 disables it
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_loop( libvlc_instance_t* , int,
                                          libvlc_exception_t * );

/**
 * Start playing.
 *
 * Additionnal playlist item options can be specified for addition to the
 * item before it is played.
 *
 * \param p_instance the playlist instance
 * \param i_id the item to play. If this is a negative number, the next
 *        item will be selected. Otherwise, the item with the given ID will be
 *        played
 * \param i_options the number of options to add to the item
 * \param ppsz_options the options to add to the item
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_play( libvlc_instance_t*, int, int,
                                          char **, libvlc_exception_t * );

/**
 * Toggle the playlist's pause status.
 *
 * If the playlist was running, it is paused. If it was paused, it is resumed.
 *
 * \param p_instance the playlist instance to pause
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_pause( libvlc_instance_t *,
                                           libvlc_exception_t * );

/**
 * Checks whether the playlist is running
 *
 * \param p_instance the playlist instance
 * \param p_e an initialized exception pointer
 * \return 0 if the playlist is stopped or paused, 1 if it is running
 */
VLC_DEPRECATED_API int libvlc_playlist_isplaying( libvlc_instance_t *,
                                              libvlc_exception_t * );

/**
 * Get the number of items in the playlist
 *
 * \param p_instance the playlist instance
 * \param p_e an initialized exception pointer
 * \return the number of items
 */
VLC_DEPRECATED_API int libvlc_playlist_items_count( libvlc_instance_t *,
                                                libvlc_exception_t * );

/**
 * Lock the playlist.
 *
 * \param p_instance the playlist instance
 */
VLC_DEPRECATED_API void libvlc_playlist_lock( libvlc_instance_t * );

/**
 * Unlock the playlist.
 *
 * \param p_instance the playlist instance
 */
VLC_DEPRECATED_API void libvlc_playlist_unlock( libvlc_instance_t * );

/**
 * Stop playing.
 *
 * \param p_instance the playlist instance to stop
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_stop( libvlc_instance_t *,
                                          libvlc_exception_t * );

/**
 * Go to the next playlist item. If the playlist was stopped, playback
 * is started.
 *
 * \param p_instance the playlist instance
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_next( libvlc_instance_t *,
                                          libvlc_exception_t * );

/**
 * Go to the previous playlist item. If the playlist was stopped, playback
 * is started.
 *
 * \param p_instance the playlist instance
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_prev( libvlc_instance_t *,
                                          libvlc_exception_t * );

/**
 * Empty a playlist. All items in the playlist are removed.
 *
 * \param p_instance the playlist instance
 * \param p_e an initialized exception pointer
 */
VLC_DEPRECATED_API void libvlc_playlist_clear( libvlc_instance_t *,
                                           libvlc_exception_t * );

/**
 * Append an item to the playlist. The item is added at the end. If more
 * advanced options are required, \see libvlc_playlist_add_extended instead.
 *
 * \param p_instance the playlist instance
 * \param psz_uri the URI to open, using VLC format
 * \param psz_name a name that you might want to give or NULL
 * \param p_e an initialized exception pointer
 * \return the identifier of the new item
 */
VLC_DEPRECATED_API int libvlc_playlist_add( libvlc_instance_t *, const char *,
                                        const char *, libvlc_exception_t * );

/**
 * Append an item to the playlist. The item is added at the end, with
 * additional input options.
 *
 * \param p_instance the playlist instance
 * \param psz_uri the URI to open, using VLC format
 * \param psz_name a name that you might want to give or NULL
 * \param i_options the number of options to add
 * \param ppsz_options strings representing the options to add
 * \param p_e an initialized exception pointer
 * \return the identifier of the new item
 */
VLC_DEPRECATED_API int libvlc_playlist_add_extended( libvlc_instance_t *, const char *,
                                                 const char *, int, const char **,
                                                 libvlc_exception_t * );

/**
 * Delete the playlist item with the given ID.
 *
 * \param p_instance the playlist instance
 * \param i_id the id to remove
 * \param p_e an initialized exception pointer
 * \return 0 in case of success, a non-zero value otherwise
 */
VLC_DEPRECATED_API int libvlc_playlist_delete_item( libvlc_instance_t *, int,
                                                libvlc_exception_t * );

/** Get the input that is currently being played by the playlist.
 *
 * \param p_instance the playlist instance to use
 * \param p_e an initialized exception pointern
 * \return a media instance object
 */
VLC_DEPRECATED_API libvlc_media_player_t * libvlc_playlist_get_media_player(
                                libvlc_instance_t *, libvlc_exception_t * );

/** @}*/

/**
 * \defgroup libvlc_old Libvlc Old (Deprecated)
 * This is libvlc, the base library of the VLC program. (Deprecated)
 * This is the legacy API. Please consider using the new libvlc API
 *
 * @deprecated
 * @{
 */


/*****************************************************************************
 * Exported vlc API (Deprecated)
 *****************************************************************************/

/*****************************************************************************
 * Playlist
 *****************************************************************************/

/* Used by VLC_AddTarget() */
#define PLAYLIST_INSERT          0x0001
#define PLAYLIST_APPEND          0x0002
#define PLAYLIST_GO              0x0004
#define PLAYLIST_PREPARSE        0x0008
#define PLAYLIST_SPREPARSE       0x0010
#define PLAYLIST_NO_REBUILD      0x0020

#define PLAYLIST_END           -666

/*****************************************************************************
 * Error values
 *****************************************************************************/
#define VLC_SUCCESS         -0                                   /* No error */
#define VLC_ENOMEM          -1                          /* Not enough memory */
#define VLC_ETHREAD         -2                               /* Thread error */
#define VLC_ETIMEOUT        -3                                    /* Timeout */

#define VLC_ENOMOD         -10                           /* Module not found */

#define VLC_ENOOBJ         -20                           /* Object not found */
#define VLC_EBADOBJ        -21                            /* Bad object type */

#define VLC_ENOVAR         -30                         /* Variable not found */
#define VLC_EBADVAR        -31                         /* Bad variable value */

#define VLC_ENOITEM        -40                           /**< Item not found */

#define VLC_EEXIT         -255                             /* Program exited */
#define VLC_EEXITSUCCESS  -999                /* Program exited successfully */
#define VLC_EGENERIC      -666                              /* Generic error */

/*****************************************************************************
 * Booleans
 *****************************************************************************/
#define VLC_FALSE false
#define VLC_TRUE  true

/**
 * \defgroup var_type Variable types
 * These are the different types a vlc variable can have.
 * @{
 */
#define VLC_VAR_VOID      0x0010
#define VLC_VAR_BOOL      0x0020
#define VLC_VAR_INTEGER   0x0030
#define VLC_VAR_HOTKEY    0x0031
#define VLC_VAR_STRING    0x0040
#define VLC_VAR_MODULE    0x0041
#define VLC_VAR_FILE      0x0042
#define VLC_VAR_DIRECTORY 0x0043
#define VLC_VAR_VARIABLE  0x0044
#define VLC_VAR_FLOAT     0x0050
#define VLC_VAR_TIME      0x0060
#define VLC_VAR_ADDRESS   0x0070
#define VLC_VAR_MUTEX     0x0080
#define VLC_VAR_LIST      0x0090
/**@}*/


#if !defined( __LIBVLC__ )
/* Otherwise they are declared and exported in vlc_common.h */
/**
 * Retrieve libvlc version
 *
 * \return a string containing the libvlc version
 */
VLC_DEPRECATED_API char const * VLC_Version ( void );

/**
 * Retrieve libvlc compile time
 *
 * \return a string containing the libvlc compile time
 */
VLC_DEPRECATED_API char const * VLC_CompileTime ( void );

/**
 * Retrieve the username of the libvlc builder
 *
 * \return a string containing the username of the libvlc builder
 */
VLC_DEPRECATED_API char const * VLC_CompileBy ( void );

/**
 * Retrieve the host of the libvlc builder
 *
 * \return a string containing the host of the libvlc builder
 */
VLC_DEPRECATED_API char const * VLC_CompileHost ( void );

/**
 * Retrieve the domain name of the host of the libvlc builder
 *
 * \return a string containing the domain name of the host of the libvlc builder
 */
VLC_DEPRECATED_API char const * VLC_CompileDomain ( void );

/**
 * Retrieve libvlc compiler version
 *
 * \return a string containing the libvlc compiler version
 */
VLC_DEPRECATED_API char const * VLC_Compiler ( void );

/**
 * Retrieve libvlc changeset
 *
 * \return a string containing the libvlc subversion changeset
 */
VLC_DEPRECATED_API char const * VLC_Changeset ( void );

/**
 * Return an error string
 *
 * \param i_err an error code
 * \return an error string
 */
VLC_DEPRECATED_API char const * VLC_Error ( int i_err );

#endif /* __LIBVLC__ */

/**
 * Initialize libvlc
 *
 * This function allocates a vlc_t structure and returns a negative value
 * in case of failure. Also, the thread system is initialized
 *
 * \return vlc object id or an error code
 */
VLC_DEPRECATED_API int     VLC_Create( void );

/**
 * Initialize a vlc_t structure
 *
 * This function initializes a previously allocated vlc_t structure:
 *  - CPU detection
 *  - gettext initialization
 *  - message queue, module bank and playlist initialization
 *  - configuration and commandline parsing
 *
 *  \param i_object a vlc object id
 *  \param i_argc the number of arguments
 *  \param ppsz_argv an array of arguments
 *  \return VLC_SUCCESS on success
 */
VLC_DEPRECATED_API int     VLC_Init( int, int, const char *[] );

/**
 * Add an interface
 *
 * This function opens an interface plugin and runs it. If b_block is set
 * to 0, VLC_AddIntf will return immediately and let the interface run in a
 * separate thread. If b_block is set to 1, VLC_AddIntf will continue until
 * user requests to quit.
 *
 * \param i_object a vlc object id
 * \param psz_module a vlc module name of an interface
 * \param b_block make this interface blocking
 * \param b_play start playing when the interface is done loading
 * \return VLC_SUCCESS on success
 */
VLC_DEPRECATED_API int     VLC_AddIntf( int, char const *, vlc_bool_t, vlc_bool_t );

/**
 * Ask vlc to die
 *
 * This function sets p_libvlc->b_die to VLC_TRUE, but does not do any other
 * task. It is your duty to call VLC_CleanUp and VLC_Destroy afterwards.
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
VLC_DEPRECATED_API int     VLC_Die( int );

/**
 * Clean up all the intf, playlist, vout and aout
 *
 * This function requests all intf, playlist, vout and aout objects to finish
 * and CleanUp. Only a blank VLC object should remain after this.
 *
 * \note This function was previously called VLC_Stop
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
VLC_DEPRECATED_API int     VLC_CleanUp( int );

/**
 * Destroy all threads and the VLC object
 *
 * This function requests the running threads to finish, waits for their
 * termination, and destroys their structure.
 * Then it will de-init all VLC object initializations.
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
VLC_DEPRECATED_API int     VLC_Destroy( int );

/**
 * Set a VLC variable
 *
 * This function sets a variable of VLC
 *
 * \note Was previously called VLC_Set
 *
 * \param i_object a vlc object id
 * \param psz_var a vlc variable name
 * \param value a vlc_value_t structure
 * \return VLC_SUCCESS on success
 */
VLC_DEPRECATED_API int     VLC_VariableSet( int, char const *, vlc_value_t );

/**
 * Get a VLC variable
 *
 * This function gets the value of a variable of VLC
 * It stores it in the p_value argument
 *
 * \note Was previously called VLC_Get
 *
 * \param i_object a vlc object id
 * \param psz_var a vlc variable name
 * \param p_value a pointer to a vlc_value_t structure
 * \return VLC_SUCCESS on success
 */
VLC_DEPRECATED_API int     VLC_VariableGet( int, char const *, vlc_value_t * );

/**
 * Get a VLC variable type
 *
 * This function gets the type of a variable of VLC
 * It stores it in the p_type argument
 *
 * \param i_object a vlc object id
 * \param psz_var a vlc variable name
 * \param pi_type a pointer to an integer
 * \return VLC_SUCCESS on success
 */
VLC_DEPRECATED_API int     VLC_VariableType( int, char const *, int * );

/**
 * Add a target to the current playlist
 *
 * This funtion will add a target to the current playlist. If a playlist does
 * not exist, it will be created.
 *
 * \param i_object a vlc object id
 * \param psz_target the URI of the target to play
 * \param ppsz_options an array of strings with input options (ie. :input-repeat)
 * \param i_options the amount of options in the ppsz_options array
 * \param i_mode the insert mode to insert the target into the playlist (PLAYLIST_* defines)
 * \param i_pos the position at which to add the new target (PLAYLIST_END for end)
 * \return the item id on success and -1 on error
 */
VLC_DEPRECATED_API int     VLC_AddTarget( int, char const *, const char **, int, int, int );

/**
 * Start the playlist and play the currently selected playlist item
 *
 * If there is something in the playlist, and the playlist is not running,
 * then start the playlist and play the currently selected playlist item.
 * If an item is currently paused, then resume it.
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
VLC_DEPRECATED_API int     VLC_Play( int );

/**
 * Pause the currently playing item. Resume it if already paused
 *
 * If an item is currently playing then pause it.
 * If the item is already paused, then resume playback.
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
VLC_DEPRECATED_API int     VLC_Pause( int );

/**
 * Stop the playlist
 *
 * If an item is currently playing then stop it.
 * Set the playlist to a stopped state.
 *
 * \note This function is new. The old VLC_Stop is now called VLC_CleanUp
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
VLC_DEPRECATED_API int             VLC_Stop( int );

/**
 * Tell if VLC is playing
 *
 * If an item is currently playing, it returns
 * VLC_TRUE, else VLC_FALSE
 *
 * \param i_object a vlc object id
 * \return VLC_TRUE or VLC_FALSE
 */
VLC_DEPRECATED_API vlc_bool_t      VLC_IsPlaying( int );

/**
 * Get the current position in a input
 *
 * Return the current position as a float
 * This method should be used for time sliders etc
 * \note For some inputs, this will be unknown.
 *
 * \param i_object a vlc object id
 * \return a float in the range of 0.0 - 1.0
 */
VLC_DEPRECATED_API float           VLC_PositionGet( int );

/**
 * Set the current position in a input
 *
 * Set the current position as a float
 * This method should be used for time sliders etc
 * \note For some inputs, this will be unknown.
 *
 * \param i_object a vlc object id
 * \param i_position a float in the range of 0.0 - 1.0
 * \return a float in the range of 0.0 - 1.0
 */
VLC_DEPRECATED_API float           VLC_PositionSet( int, float );

/**
 * Get the current position in a input
 *
 * Return the current position in seconds from the start.
 * \note For some inputs, this will be unknown.
 *
 * \param i_object a vlc object id
 * \return the offset from 0:00 in seconds
 */
VLC_DEPRECATED_API int             VLC_TimeGet( int );

/**
 * Seek to a position in the current input
 *
 * Seek i_seconds in the current input. If b_relative is set,
 * then the seek will be relative to the current position, otherwise
 * it will seek to i_seconds from the beginning of the input.
 * \note For some inputs, this will be unknown.
 *
 * \param i_object a vlc object id
 * \param i_seconds seconds from current position or from beginning of input
 * \param b_relative seek relative from current position
 * \return VLC_SUCCESS on success
 */
VLC_DEPRECATED_API int             VLC_TimeSet( int, int, vlc_bool_t );

/**
 * Get the total length of a input
 *
 * Return the total length in seconds from the current input.
 * \note For some inputs, this will be unknown.
 *
 * \param i_object a vlc object id
 * \return the length in seconds
 */
VLC_DEPRECATED_API int             VLC_LengthGet( int );

/**
 * Play the input faster than realtime
 *
 * 2x, 4x, 8x faster than realtime
 * \note For some inputs, this will be impossible.
 *
 * \param i_object a vlc object id
 * \return the current speedrate
 */
VLC_DEPRECATED_API float           VLC_SpeedFaster( int );

/**
 * Play the input slower than realtime
 *
 * 1/2x, 1/4x, 1/8x slower than realtime
 * \note For some inputs, this will be impossible.
 *
 * \param i_object a vlc object id
 * \return the current speedrate
 */
VLC_DEPRECATED_API float           VLC_SpeedSlower( int );

/**
 * Return the current playlist item
 *
 * \param i_object a vlc object id
 * \return the index of the playlistitem that is currently selected for play
 */
VLC_DEPRECATED_API int             VLC_PlaylistIndex( int );

/**
 * Total amount of items in the playlist
 *
 * \param i_object a vlc object id
 * \return amount of playlist items
 */
VLC_DEPRECATED_API int             VLC_PlaylistNumberOfItems( int );

/**
 * Next playlist item
 *
 * Skip to the next playlistitem and play it.
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
VLC_DEPRECATED_API int             VLC_PlaylistNext( int );

/**
 * Previous playlist item
 *
 * Skip to the previous playlistitem and play it.
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
VLC_DEPRECATED_API int             VLC_PlaylistPrev( int );

/**
 * Clear the contents of the playlist
 *
 * Completly empty the entire playlist.
 *
 * \note Was previously called VLC_ClearPlaylist
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
VLC_DEPRECATED_API int             VLC_PlaylistClear( int );

/**
 * Change the volume
 *
 * \param i_object a vlc object id
 * \param i_volume something in a range from 0-200
 * \return the new volume (range 0-200 %)
 */
VLC_DEPRECATED_API int             VLC_VolumeSet( int, int );

/**
 * Get the current volume
 *
 * Retrieve the current volume.
 *
 * \param i_object a vlc object id
 * \return the current volume (range 0-200 %)
 */
VLC_DEPRECATED_API int             VLC_VolumeGet( int );

/**
 * Mute/Unmute the volume
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
VLC_DEPRECATED_API int            VLC_VolumeMute( int );

/**
 * Toggle Fullscreen mode
 *
 * Switch between normal and fullscreen video
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
VLC_DEPRECATED_API int             VLC_FullScreen( int );

/**@} */

# ifdef __cplusplus
}
# endif

#endif /* _LIBVLC_DEPRECATED_H */
