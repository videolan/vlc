/*****************************************************************************
 * vlc.h: global header for vlc
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: vlc.h,v 1.19 2002/12/13 01:56:29 gbazin Exp $
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

typedef union
{
    int             i_int;
    vlc_bool_t      b_bool;
    float           f_float;
    char *          psz_string;
    void *          p_address;
    vlc_object_t *  p_object;

    /* Make sure the structure is at least 64bits */
    struct { char a, b, c, d, e, f, g, h; } padding;

} vlc_value_t;

struct vlc_list_t
{
    int             i_count;
    vlc_value_t *   p_values;

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

/* Playlist parsing mode */
#define PLAYLIST_REPEAT_CURRENT   0             /* Keep playing current item */
#define PLAYLIST_FORWARD          1              /* Parse playlist until end */
#define PLAYLIST_BACKWARD        -1                       /* Parse backwards */
#define PLAYLIST_FORWARD_LOOP     2               /* Parse playlist and loop */
#define PLAYLIST_BACKWARD_LOOP   -2              /* Parse backwards and loop */
#define PLAYLIST_RANDOM           3                          /* Shuffle play */
#define PLAYLIST_REVERSE_RANDOM  -3                  /* Reverse shuffle play */

/* Playlist commands */
#define PLAYLIST_PLAY   1                         /* Starts playing. No arg. */
#define PLAYLIST_PAUSE  2                 /* Toggles playlist pause. No arg. */
#define PLAYLIST_STOP   3                          /* Stops playing. No arg. */
#define PLAYLIST_SKIP   4                          /* Skip X items and play. */
#define PLAYLIST_GOTO   5                                  /* Goto Xth item. */
#define PLAYLIST_MODE   6                          /* Set playlist mode. ??? */

/*****************************************************************************
 * Required internal headers
 *****************************************************************************/
#if defined( __VLC__ )
#   include "vlc_common.h"
#endif

/*****************************************************************************
 * Exported libvlc API
 *****************************************************************************/
char const * VLC_Version ( void );
char const * VLC_Error   ( int );

int     VLC_Create       ( void );
int     VLC_Init         ( int, int, char *[] );
int     VLC_Die          ( int );
int     VLC_Destroy      ( int );

int     VLC_Set          ( int, char const *, vlc_value_t );
int     VLC_Get          ( int, char const *, vlc_value_t * );
int     VLC_AddIntf      ( int, char const *, vlc_bool_t );
int     VLC_AddTarget    ( int, char const *, int, int );

int     VLC_Play         ( int );
int     VLC_Pause        ( int );
int     VLC_Stop         ( int );
int     VLC_FullScreen   ( int );

# ifdef __cplusplus
}
# endif

#endif /* <vlc/vlc.h> */
