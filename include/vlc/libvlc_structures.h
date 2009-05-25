/*****************************************************************************
 * libvlc.h:  libvlc_* new external API structures
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 * $Id $
 *
 * Authors: Filippo Carone <littlejohn@videolan.org>
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

#ifndef LIBVLC_STRUCTURES_H
#define LIBVLC_STRUCTURES_H 1

/**
 * \file
 * This file defines libvlc_* new external API structures
 */

#include <stdint.h>

# ifdef __cplusplus
extern "C" {
# endif

/** This structure is opaque. It represents a libvlc instance */
typedef struct libvlc_instance_t libvlc_instance_t;

/*****************************************************************************
 * Exceptions
 *****************************************************************************/

/** \defgroup libvlc_exception libvlc_exception
 * \ingroup libvlc_core
 * LibVLC Exceptions handling
 * @{
 */

typedef struct libvlc_exception_t
{
    int b_raised;
    int i_code;
    char *psz_message;
} libvlc_exception_t;

/**@} */

/*****************************************************************************
 * Time
 *****************************************************************************/
/** \defgroup libvlc_time libvlc_time
 * \ingroup libvlc_core
 * LibVLC Time support in libvlc
 * @{
 */

typedef int64_t libvlc_time_t;

/**@} */

/*****************************************************************************
 * Media Descriptor
 *****************************************************************************/
/** \defgroup libvlc_media libvlc_media
 * \ingroup libvlc
 * LibVLC Media Descriptor handling
 * @{
 */


/**@} */


/*****************************************************************************
 * Playlist
 *****************************************************************************/
/** \defgroup libvlc_playlist libvlc_playlist (Deprecated)
 * \ingroup libvlc
 * LibVLC Playlist handling (Deprecated)
 * @deprecated Use media_list
 * @{
 */

typedef struct libvlc_playlist_item_t
{
    int i_id;
    char * psz_uri;
    char * psz_name;

} libvlc_playlist_item_t;

/**@} */


/*****************************************************************************
 * Message log handling
 *****************************************************************************/

/** \defgroup libvlc_log libvlc_log
 * \ingroup libvlc_core
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

/**@} */

# ifdef __cplusplus
}
# endif

#endif
