/*****************************************************************************
 * xurl.h: URL manipulation functions (header file)
 *****************************************************************************
 * Copyright (C) 2003-2004 Commonwealth Scientific and Industrial Research
 *                         Organisation (CSIRO) Australia
 * Copyright (C) 2004 the VideoLAN team
 *
 * $Id$
 *
 * Authors: Andre Pang <Andre.Pang@csiro.au>
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

#ifndef __XURL_H__
#define __XURL_H__

#include <vlc_common.h>

/* Specialise boolean definitions to VLC's boolean types */
typedef bool XURL_Bool;
#define XURL_FALSE false
#define XURL_TRUE true

/* Specialise general C functions to VLC's standards */
#define xurl_malloc malloc
#define xurl_free free

/* Use DOS/Windows path separators? */
#ifdef WIN32
#  define XURL_WIN32_PATHING
#else
#  undef  XURL_WIN32_PATHING
#endif

/* Debugging */
#undef XURL_DEBUG

char *      XURL_Join                   ( char *psz_url1, char *psz_url2 );
char *      XURL_Concat                 ( char *psz_url,  char *psz_append );

XURL_Bool   XURL_IsAbsolute             ( char *psz_url );
XURL_Bool   XURL_HasAbsolutePath        ( char *psz_url );
XURL_Bool   XURL_IsFileURL              ( char *psz_url );
XURL_Bool   XURL_HasFragment            ( char *psz_url );

char *      XURL_GetHostname            ( char *psz_url );
char *      XURL_GetSchemeAndHostname   ( char *psz_url );
char *      XURL_GetScheme              ( char *psz_url );
char *      XURL_GetPath                ( char *psz_url );
char *      XURL_GetWithoutFragment     ( char *psz_url );

char *      XURL_GetHead                ( const char *psz_path );

#endif /* __XURL_H__ */

