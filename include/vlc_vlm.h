/*****************************************************************************
 * vlc_vlm.h: VLM core structures
 *****************************************************************************
 * Copyright (C) 2000, 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Simon Latapie <garf@videolan.org>
 *          Laurent Aimar <fenrir@videolan.org>
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

#if !defined( __LIBVLC__ )
  #error You are not libvlc or one of its plugins. You cannot include this file
#endif

#ifndef _VLC_VLM_H
#define _VLC_VLM_H 1

#ifdef __cpluplus
extern "C" {
#endif

#include <vlc_input.h>

/* VLM specific - structures and functions */

/* ok, here is the structure of a vlm_message:
   The parent node is ( name_of_the_command , NULL ), or
   ( name_of_the_command , message_error ) on error.
   If a node has children, it should not have a value (=NULL).*/
struct vlm_message_t
{
    char *psz_name;
    char *psz_value;

    int           i_child;
    vlm_message_t **child;
};


#define vlm_New( a ) __vlm_New( VLC_OBJECT(a) )
VLC_EXPORT( vlm_t *, __vlm_New, ( vlc_object_t * ) );
VLC_EXPORT( void,      vlm_Delete, ( vlm_t * ) );
VLC_EXPORT( int,       vlm_ExecuteCommand, ( vlm_t *, const char *, vlm_message_t ** ) );

VLC_EXPORT( vlm_message_t *, vlm_MessageNew, ( const char *, const char *, ... ) );
VLC_EXPORT( vlm_message_t *, vlm_MessageAdd, ( vlm_message_t *, vlm_message_t * ) );
VLC_EXPORT( void,            vlm_MessageDelete, ( vlm_message_t * ) );

#ifdef __cpluplus
}
#endif

#endif
