/*****************************************************************************
 * vlc_acl.h: interface to the network Access Control List internal API
 *****************************************************************************
 * Copyright (C) 2005 Rémi Denis-Courmont
 * Copyright (C) 2005 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Rémi Denis-Courmont <rem # videolan.org>
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

#ifndef VLC_ACL_H
# define VLC_ACL_H


VLC_API int ACL_Check( vlc_acl_t *p_acl, const char *psz_ip );
VLC_API vlc_acl_t * ACL_Create( vlc_object_t *p_this, bool b_allow ) VLC_USED VLC_MALLOC;
#define ACL_Create(a, b) ACL_Create(VLC_OBJECT(a), b)
VLC_API vlc_acl_t * ACL_Duplicate( vlc_object_t *p_this, const vlc_acl_t *p_acl ) VLC_USED VLC_MALLOC;
#define ACL_Duplicate(a,b) ACL_Duplicate(VLC_OBJECT(a),b)
VLC_API void ACL_Destroy( vlc_acl_t *p_acl );

#define ACL_AddHost(a,b,c) ACL_AddNet(a,b,-1,c)
VLC_API int ACL_AddNet( vlc_acl_t *p_acl, const char *psz_ip, int i_len, bool b_allow );
VLC_API int ACL_LoadFile( vlc_acl_t *p_acl, const char *path );

#endif
