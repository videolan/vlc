/*****************************************************************************
 * vlc.h: global header for vlc
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: vlc.h,v 1.2 2002/06/02 13:38:03 gbazin Exp $
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
 * Error values
 *****************************************************************************/
typedef signed int vlc_error_t;

#define VLC_SUCCESS         -0                                   /* No error */
#define VLC_EGENERIC        -1                              /* Generic error */
#define VLC_ENOMEM          -2                          /* Not enough memory */
#define VLC_ESTATUS         -3                             /* Invalid status */
#define VLC_EEXIT         -255                             /* Program exited */

/*****************************************************************************
 * Booleans
 *****************************************************************************/
typedef int vlc_bool_t;

#define VLC_FALSE 0
#define VLC_TRUE  1

/*****************************************************************************
 * Main structure status
 *****************************************************************************/
typedef int vlc_status_t;

#define VLC_STATUS_NONE     0x00000000
#define VLC_STATUS_CREATED  0x02020202
#define VLC_STATUS_STOPPED  0x12121212
#define VLC_STATUS_RUNNING  0x42424242

/*****************************************************************************
 * Structure types
 *****************************************************************************/
#define VLC_DECLARE_STRUCT( name ) \
    struct name##_s;         \
    typedef struct name##_s name##_t;
VLC_DECLARE_STRUCT(vlc)
VLC_DECLARE_STRUCT(vlc_object)

/*****************************************************************************
 * Required internal headers
 *****************************************************************************/
#if defined( __VLC__ )
#   include "defs.h"
#   include "config.h"
#   include "modules_inner.h"
#   include "vlc_common.h"
#   include "vlc_messages.h"
#   include "mtime.h"
#   include "modules.h"
#   include "main.h"
#   include "configuration.h"
#   include "vlc_objects.h"
#endif

/*****************************************************************************
 * Exported libvlc base API
 *****************************************************************************/
vlc_t *         vlc_create     ( void );
vlc_error_t     vlc_init       ( vlc_t *, int, char *[] );
vlc_error_t     vlc_run        ( vlc_t * );
vlc_error_t     vlc_stop       ( vlc_t * );
vlc_error_t     vlc_end        ( vlc_t * );
vlc_error_t     vlc_destroy    ( vlc_t * );

vlc_error_t     vlc_add_intf   ( vlc_t *, char *, vlc_bool_t );
vlc_error_t     vlc_add_target ( vlc_t *, char * );

vlc_status_t    vlc_status     ( vlc_t * );

# ifdef __cplusplus
}
# endif

#endif /* <vlc/vlc.h> */
