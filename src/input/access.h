/*****************************************************************************
 * access.h: Input access functions
 *****************************************************************************
 * Copyright (C) 1998-2008 VLC authors and VideoLAN
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef LIBVLC_INPUT_ACCESS_H
#define LIBVLC_INPUT_ACCESS_H 1

#include <vlc_common.h>
#include <vlc_access.h>

access_t *access_New( vlc_object_t *p_obj, input_thread_t *p_input,
                      const char *psz_access, const char *psz_demux,
                      const char *psz_path );
#define access_New( a, b, c, d, e ) access_New(VLC_OBJECT(a), b, c, d, e )
void access_Delete( access_t * );

char *get_path(const char *location);

#endif

