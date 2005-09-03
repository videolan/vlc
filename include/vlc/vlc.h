/*****************************************************************************
 * vlc.h: global header for vlc
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Derk-Jan Hartman <hartman at videolan dot org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/**
 * \defgroup libvlc Libvlc
 * This is libvlc, the base library of the VLC program.
 *
 * @{
 */


#ifndef _VLC_VLC_H
#define _VLC_VLC_H 1

# ifdef __cplusplus
extern "C" {
# endif

/*****************************************************************************
 * Our custom types
 *****************************************************************************/
typedef int vlc_bool_t;
typedef struct vlc_list_t vlc_list_t;
typedef struct vlc_object_t vlc_object_t;

#if (defined( WIN32 ) || defined( UNDER_CE )) && !defined( __MINGW32__ )
typedef signed __int64 vlc_int64_t;
# else
typedef signed long long vlc_int64_t;
#endif

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

/**
 * VLC value structure
 */
typedef union
{
    int             i_int;
    vlc_bool_t      b_bool;
    float           f_float;
    char *          psz_string;
    void *          p_address;
    vlc_object_t *  p_object;
    vlc_list_t *    p_list;
    vlc_int64_t     i_time;

    struct { char *psz_name; int i_object_id; } var;

   /* Make sure the structure is at least 64bits */
    struct { char a, b, c, d, e, f, g, h; } padding;

} vlc_value_t;

/**
 * VLC list structure
 */
struct vlc_list_t
{
    int             i_count;
    vlc_value_t *   p_values;
    int *           pi_types;

};

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

#define VLC_EEXIT         -255                             /* Program exited */
#define VLC_EGENERIC      -666                              /* Generic error */

/*****************************************************************************
 * Booleans
 *****************************************************************************/
#define VLC_FALSE 0
#define VLC_TRUE  1

/*****************************************************************************
 * Playlist
 *****************************************************************************/

/* Used by VLC_AddTarget() */
#define PLAYLIST_INSERT          0x0001
#define PLAYLIST_REPLACE         0x0002
#define PLAYLIST_APPEND          0x0004
#define PLAYLIST_GO              0x0008
#define PLAYLIST_CHECK_INSERT    0x0010

#define PLAYLIST_END           -666

/*****************************************************************************
 * Required internal headers
 *****************************************************************************/
#if defined( __VLC__ )
#   include "vlc_common.h"
#endif

/*****************************************************************************
 * Exported libvlc API
 *****************************************************************************/
#if !defined( __VLC__ )
/* Otherwise they are declared and exported in vlc_common.h */
/**
 * Retrieve libvlc version
 *
 * \return a string containing the libvlc version
 */
char const * VLC_Version ( void );

/**
 * Retrieve libvlc compile time
 *
 * \return a string containing the libvlc compile time
 */
char const * VLC_CompileTime ( void );

/**
 * Retrieve the username of the libvlc builder
 *
 * \return a string containing the username of the libvlc builder
 */
char const * VLC_CompileBy ( void );

/**
 * Retrieve the host of the libvlc builder
 *
 * \return a string containing the host of the libvlc builder
 */
char const * VLC_CompileHost ( void );

/**
 * Retrieve the domain name of the host of the libvlc builder
 *
 * \return a string containing the domain name of the host of the libvlc builder
 */
char const * VLC_CompileDomain ( void );

/**
 * Retrieve libvlc compiler version
 *
 * \return a string containing the libvlc compiler version
 */
char const * VLC_Compiler ( void );

/**
 * Retrieve libvlc changeset
 *
 * \return a string containing the libvlc subversion changeset
 */
char const * VLC_Changeset ( void );

/**
 * Return an error string
 *
 * \param i_err an error code
 * \return an error string
 */
char const * VLC_Error ( int i_err );

#endif /* __VLC__ */

/**
 * Initialize libvlc
 *
 * This function allocates a vlc_t structure and returns a negative value
 * in case of failure. Also, the thread system is initialized
 *
 * \return vlc object id or an error code
 */
int     VLC_Create( void );

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
int     VLC_Init( int, int, char *[] );

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
int     VLC_AddIntf( int, char const *, vlc_bool_t, vlc_bool_t );

/**
 * Ask vlc to die
 *
 * This function sets p_vlc->b_die to VLC_TRUE, but does not do any other
 * task. It is your duty to call VLC_CleanUp and VLC_Destroy afterwards.
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
int     VLC_Die( int );

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
int     VLC_CleanUp( int );

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
int     VLC_Destroy( int );

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
int     VLC_VariableSet( int, char const *, vlc_value_t );

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
int     VLC_VariableGet( int, char const *, vlc_value_t * );

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
int     VLC_VariableType( int, char const *, int * );

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
 * \return VLC_SUCCESS on success
 */
int     VLC_AddTarget( int, char const *, const char **, int, int, int );

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
int     VLC_Play( int );

/**
 * Pause the currently playing item. Resume it if already paused
 *
 * If an item is currently playing then pause it.
 * If the item is already paused, then resume playback.
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
int     VLC_Pause( int );

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
int             VLC_Stop( int );

/**
 * Tell if VLC is playing
 *
 * If an item is currently playing, it returns
 * VLC_TRUE, else VLC_FALSE
 *
 * \param i_object a vlc object id
 * \return VLC_TRUE or VLC_FALSE
 */
vlc_bool_t      VLC_IsPlaying( int );

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
float           VLC_PositionGet( int );

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
float           VLC_PositionSet( int, float );

/**
 * Get the current position in a input
 *
 * Return the current position in seconds from the start.
 * \note For some inputs, this will be unknown.
 *
 * \param i_object a vlc object id
 * \return the offset from 0:00 in seconds
 */
int             VLC_TimeGet( int );

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
int             VLC_TimeSet( int, int, vlc_bool_t );

/**
 * Get the total length of a input
 *
 * Return the total length in seconds from the current input.
 * \note For some inputs, this will be unknown.
 *
 * \param i_object a vlc object id
 * \return the length in seconds
 */
int             VLC_LengthGet( int );

/**
 * Play the input faster than realtime
 *
 * 2x, 4x, 8x faster than realtime
 * \note For some inputs, this will be impossible.
 *
 * \param i_object a vlc object id
 * \return the current speedrate
 */
float           VLC_SpeedFaster( int );

/**
 * Play the input slower than realtime
 *
 * 1/2x, 1/4x, 1/8x slower than realtime
 * \note For some inputs, this will be impossible.
 *
 * \param i_object a vlc object id
 * \return the current speedrate
 */
float           VLC_SpeedSlower( int );

/**
 * Return the current playlist item
 *
 * \param i_object a vlc object id
 * \return the index of the playlistitem that is currently selected for play
 */
int             VLC_PlaylistIndex( int );

/**
 * Total amount of items in the playlist
 *
 * \param i_object a vlc object id
 * \return amount of playlist items
 */
int             VLC_PlaylistNumberOfItems( int );

/**
 * Next playlist item
 *
 * Skip to the next playlistitem and play it.
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
int             VLC_PlaylistNext( int );

/**
 * Previous playlist item
 *
 * Skip to the previous playlistitem and play it.
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
int             VLC_PlaylistPrev( int );

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
int             VLC_PlaylistClear( int );

/**
 * Change the volume
 *
 * \param i_object a vlc object id
 * \param i_volume something in a range from 0-200
 * \return the new volume (range 0-200 %)
 */
int             VLC_VolumeSet( int, int );

/**
 * Get the current volume
 *
 * Retrieve the current volume.
 *
 * \param i_object a vlc object id
 * \return the current volume (range 0-200 %)
 */
int             VLC_VolumeGet( int );

/**
 * Mute/Unmute the volume
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
int            VLC_VolumeMute( int );

/**
 * Toggle Fullscreen mode
 *
 * Switch between normal and fullscreen video
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
int             VLC_FullScreen( int );


# ifdef __cplusplus
}
# endif

#endif /* <vlc/vlc.h> */
