/*****************************************************************************
 * vlc.h: global header for vlc
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: vlc.h,v 1.12 2002/08/26 08:36:12 sam Exp $
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
typedef struct vlc_t vlc_t;
typedef struct vlc_list_t vlc_list_t;
typedef struct vlc_object_t vlc_object_t;

typedef signed int vlc_error_t;
typedef int        vlc_bool_t;
typedef int        vlc_status_t;

/*****************************************************************************
 * Error values
 *****************************************************************************/
#define VLC_SUCCESS         -0                                   /* No error */
#define VLC_ENOMEM          -1                          /* Not enough memory */
#define VLC_EMODULE         -2                           /* Module not found */
#define VLC_ESTATUS         -3                             /* Invalid status */
#define VLC_ETHREAD         -4                     /* Could not spawn thread */
#define VLC_EEXIT         -255                             /* Program exited */
#define VLC_EGENERIC      -666                              /* Generic error */

/*****************************************************************************
 * Booleans
 *****************************************************************************/
#define VLC_FALSE 0
#define VLC_TRUE  1

/*****************************************************************************
 * Main structure status
 *****************************************************************************/
#define VLC_STATUS_NONE     0x00000000
#define VLC_STATUS_CREATED  0x02020202
#define VLC_STATUS_STOPPED  0x12121212
#define VLC_STATUS_RUNNING  0x42424242

/*****************************************************************************
 * Playlist
 *****************************************************************************/

/* Used by playlist_Add */
#define PLAYLIST_INSERT      0x0001
#define PLAYLIST_REPLACE     0x0002
#define PLAYLIST_APPEND      0x0004
#define PLAYLIST_GO          0x0008

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
#   include "config.h"
#   include "vlc_config.h"
#   include "modules_inner.h"
#   include "vlc_common.h"
#   include "os_specific.h"
#   include "vlc_messages.h"
#   include "mtime.h"
#   include "modules.h"
#   include "main.h"
#   include "configuration.h"
#   include "vlc_objects.h"
#endif

/*****************************************************************************
 * Exported libvlc API
 *****************************************************************************/
vlc_status_t    vlc_status       ( void );

vlc_error_t     vlc_create       ( void );
vlc_error_t     vlc_init         ( int, char *[] );
vlc_error_t     vlc_run          ( void );
vlc_error_t     vlc_die          ( void );
vlc_error_t     vlc_destroy      ( void );

vlc_error_t     vlc_set          ( const char *, const char * );
vlc_error_t     vlc_add_intf     ( const char *, vlc_bool_t );
vlc_error_t     vlc_add_target   ( const char *, int, int );

/*****************************************************************************
 * Exported libvlc reentrant API
 *****************************************************************************/
vlc_status_t    vlc_status_r     ( vlc_t * );

vlc_t *         vlc_create_r     ( void );
vlc_error_t     vlc_init_r       ( vlc_t *, int, char *[] );
vlc_error_t     vlc_run_r        ( vlc_t * );
vlc_error_t     vlc_die_r        ( vlc_t * );
vlc_error_t     vlc_destroy_r    ( vlc_t * );

vlc_error_t     vlc_set_r        ( vlc_t *, const char *, const char * );
vlc_error_t     vlc_add_intf_r   ( vlc_t *, const char *, vlc_bool_t );
vlc_error_t     vlc_add_target_r ( vlc_t *, const char *, int, int );

# ifdef __cplusplus
}
# endif

#endif /* <vlc/vlc.h> */
