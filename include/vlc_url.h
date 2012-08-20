/*****************************************************************************
 * vlc_url.h: URL related macros
 *****************************************************************************
 * Copyright (C) 2002-2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Rémi Denis-Courmont <rem # videolan.org>
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

#ifndef VLC_URL_H
# define VLC_URL_H

/**
 * \file
 * This file defines functions for manipulating URL in vlc
 */

VLC_API char *vlc_path2uri (const char *path, const char *scheme) VLC_MALLOC;

struct vlc_url_t
{
    char *psz_protocol;
    char *psz_username;
    char *psz_password;
    char *psz_host;
    unsigned i_port;
    char *psz_path;
    char *psz_option;

    char *psz_buffer; /* to be freed */
};

VLC_API char * decode_URI_duplicate( const char *psz ) VLC_MALLOC;
VLC_API char * decode_URI( char *psz );
VLC_API char * encode_URI_component( const char *psz ) VLC_MALLOC;
VLC_API char * make_path( const char *url ) VLC_MALLOC;

VLC_API void vlc_UrlParse (vlc_url_t *, const char *, unsigned char);
VLC_API void vlc_UrlClean (vlc_url_t *);
#endif
