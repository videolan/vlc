/*****************************************************************************
 * vlc.h: global header for vlc
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id$
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
 * This is libvlc.
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

/* Used by playlist_Add */
#define PLAYLIST_INSERT          0x0001
#define PLAYLIST_REPLACE         0x0002
#define PLAYLIST_APPEND          0x0004
#define PLAYLIST_GO              0x0008
#define PLAYLIST_CHECK_INSERT    0x0010

#define PLAYLIST_END           -666

/**
 * Playlist commands
 */
typedef enum {
    PLAYLIST_PLAY,                              /**< Starts playing. No arg. */
    PLAYLIST_PAUSE,                     /**< Toggles playlist pause. No arg. */
    PLAYLIST_STOP,                               /**< Stops playing. No arg. */
    PLAYLIST_SKIP,                               /**< Skip X items and play. */
    PLAYLIST_GOTO,                                       /**< Goto Xth item. */
    PLAYLIST_MODE                                /**< Set playlist mode. ??? */
} playlist_command_t;

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
 * Ask vlc to die
 *
 * This function sets p_vlc->b_die to VLC_TRUE, but does not do any other
 * task. It is your duty to call VLC_End and VLC_Destroy afterwards.
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
int     VLC_Die          ( int );

/**
 * Stop playing and destroy everything.
 *
 * This function requests the running threads to finish, waits for their
 * termination, and destroys their structure.
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

int     VLC_AddIntf      ( int, char const *, vlc_bool_t, vlc_bool_t );
int     VLC_AddTarget    ( int, char const *, const char **, int, int, int );

int     VLC_Play         ( int );
int     VLC_Pause        ( int );
int     VLC_Stop         ( int );
int     VLC_FullScreen   ( int );
int     VLC_ClearPlaylist( int );
vlc_bool_t VLC_IsPlaying ( int );

# ifdef __cplusplus
}
# endif

#endif /* <vlc/vlc.h> */
