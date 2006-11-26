/*****************************************************************************
 * libvlc_internal.h : Definition of opaque structures for libvlc exported API
 * Also contains some internal utility functions
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id: control_structures.h 13752 2005-12-15 10:14:42Z oaubert $
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

#ifndef _LIBVLC_INTERNAL_H
#define _LIBVLC_INTERNAL_H 1

# ifdef __cplusplus
extern "C" {
# endif

#include <vlc/vlc.h>

/***************************************************************************
 * Internal creation and destruction functions
 ***************************************************************************/
libvlc_int_t *libvlc_InternalCreate();
int libvlc_InternalInit( libvlc_int_t *, int, char *ppsz_argv[] );
int libvlc_InternalCleanup( libvlc_int_t * );
int libvlc_InternalDestroy( libvlc_int_t *, vlc_bool_t );

int libvlc_InternalAddIntf( libvlc_int_t *, const char *, vlc_bool_t,
                            vlc_bool_t, int, const char *const * );

/***************************************************************************
 * Opaque structures for libvlc API
 ***************************************************************************/

struct libvlc_instance_t
{
    libvlc_int_t *p_libvlc_int;
    vlm_t      *p_vlm;
};

struct libvlc_input_t
{
    int i_input_id;  ///< Input object id. We don't use a pointer to
                     /// avoid any crash
    struct libvlc_instance_t *p_instance; ///< Parent instance
};

#define RAISENULL( psz,a... ) { libvlc_exception_raise( p_e, psz,##a ); \
                                return NULL; }
#define RAISEVOID( psz,a... ) { libvlc_exception_raise( p_e, psz,##a ); \
                                return; }
#define RAISEZERO( psz,a... ) { libvlc_exception_raise( p_e, psz,##a ); \
                                return 0; }

# ifdef __cplusplus
}
# endif

#endif
