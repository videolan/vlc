/*****************************************************************************
 * vlc.h: global header for vlc
 *****************************************************************************
 * Copyright (C) 1998-2004 VideoLAN
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

#if defined( WIN32 ) && !defined( __MINGW32__ )
    signed __int64   i_time;
# else
    signed long long i_time;
#endif

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

/**
 * Retrieve libvlc version
 *
 * \return a string containing the libvlc version
 */
char const * VLC_Version ( void );

/**
 * Return an error string
 *
 * \param i_err an error code
 * \return an error string
 */
char const * VLC_Error ( int i_err );

/**
 * Initialize libvlc
 *
 * This function allocates a vlc_t structure and returns a negative value
 * in case of failure. Also, the thread system is initialized
 *
 * \return vlc object id or an error code
 */
int     VLC_Create       ( void );

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
int     VLC_Init         ( int, int, char *[] );

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
int     VLC_AddIntf      ( int, char const *, vlc_bool_t, vlc_bool_t );

/**
 * Ask vlc to die
 *
 * This function sets p_vlc->b_die to VLC_TRUE, but does not do any other
 * task. It is your duty to call VLC_CleanUp and VLC_Destroy afterwards.
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
int     VLC_Die          ( int );

/**
 * Clean up all the intf, playlist, vout and aout
 *
 * This function requests all intf, playlist, vout and aout objects to finish
 * and CleanUp. Only a blank VLC object should remain after this.
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
int     VLC_CleanUp      ( int );

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
int     VLC_Destroy      ( int );

/**
 * Set a VLC variable
 *
 * This function sets a variable of VLC
 *
 * \param i_object a vlc object id
 * \param psz_var a vlc variable name
 * \param value a vlc_value_t structure
 * \return VLC_SUCCESS on success
 */
int     VLC_Set          ( int, char const *, vlc_value_t );

/**
 * Get a VLC variable
 *
 * This function gets the value of a variable of VLC
 * It stores it in the p_value argument
 *
 * \param i_object a vlc object id
 * \param psz_var a vlc variable name
 * \param p_value a pointer to a vlc_value_t structure
 * \return VLC_SUCCESS on success
 */
int     VLC_Get          ( int, char const *, vlc_value_t * );

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
int     VLC_AddTarget    ( int, char const *, const char **, int, int, int );

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
int     VLC_Play         ( int );

/**
 * Pause the currently playing item. Resume it if already paused
 *
 * If an item is currently playing then pause it.
 * If the item is already paused, then resume playback.
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
int     VLC_Pause        ( int );

/**
 * Stop the playlist
 *
 * If an item is currently playing then stop it.
 * Set the playlist to a stopped state.
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
int     VLC_Stop         ( int );

/**
 * Stop the playlist
 *
 * If an item is currently playing then stop it.
 * Set the playlist to a stopped state.
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
vlc_bool_t VLC_IsPlaying ( int );

/**
 * Clear the contents of the playlist
 *
 * Completly empty the entire playlist.
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
int     VLC_ClearPlaylist( int );

/**
 * Toggle Fullscreen mode
 *
 * Switch between normal and fullscreen video
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
int     VLC_FullScreen   ( int );


# ifdef __cplusplus
}
# endif

#endif /* <vlc/vlc.h> */
