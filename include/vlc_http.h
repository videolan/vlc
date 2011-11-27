/*****************************************************************************
 * vlc_http.h: Shared code for HTTP clients
 *****************************************************************************
 * Copyright (C) 2001-2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Rémi Denis-Courmont <rem # videolan.org>
 *          Antoine Cellerier <dionoea at videolan dot org>
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

#ifndef VLC_HTTP_H
#define VLC_HTTP_H 1

/**
 * \file
 * This file defines functions, structures, enums and macros shared between
 * HTTP clients.
 */

/* RFC 2617: Basic and Digest Access Authentication */
typedef struct http_auth_t
{
    char *psz_realm;
    char *psz_domain;
    char *psz_nonce;
    char *psz_opaque;
    char *psz_stale;
    char *psz_algorithm;
    char *psz_qop;
    int i_nonce;
    char *psz_cnonce;
    char *psz_HA1; /* stored H(A1) value if algorithm = "MD5-sess" */
} http_auth_t;


VLC_API void http_auth_Init( http_auth_t * );
VLC_API void http_auth_Reset( http_auth_t * );
VLC_API void http_auth_ParseWwwAuthenticateHeader
            ( vlc_object_t *, http_auth_t * ,
              const char * );
VLC_API int http_auth_ParseAuthenticationInfoHeader
            ( vlc_object_t *, http_auth_t *,
              const char *, const char *,
              const char *, const char *,
              const char * );
VLC_API char *http_auth_FormatAuthorizationHeader
            ( vlc_object_t *, http_auth_t *,
              const char *, const char *,
              const char *, const char * ) VLC_USED;

#endif /* VLC_HTTP_H */
