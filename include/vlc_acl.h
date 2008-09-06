/*****************************************************************************
 * vlc_acl.h: interface to the network Access Control List internal API
 *****************************************************************************
 * Copyright (C) 2005 Rémi Denis-Courmont
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Rémi Denis-Courmont <rem # videolan.org>
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

#ifndef VLC_ACL_H
# define VLC_ACL_H

#define ACL_Create(a, b) __ACL_Create(VLC_OBJECT(a), b)
#define ACL_Duplicate(a,b) __ACL_Duplicate(VLC_OBJECT(a),b)

VLC_EXPORT( int, ACL_Check, ( vlc_acl_t *p_acl, const char *psz_ip ) );
VLC_EXPORT( vlc_acl_t *, __ACL_Create, ( vlc_object_t *p_this, bool b_allow ) LIBVLC_USED );
VLC_EXPORT( vlc_acl_t *, __ACL_Duplicate, ( vlc_object_t *p_this, const vlc_acl_t *p_acl ) LIBVLC_USED );
VLC_EXPORT( void, ACL_Destroy, ( vlc_acl_t *p_acl ) );

#define ACL_AddHost(a,b,c) ACL_AddNet(a,b,-1,c)
VLC_EXPORT( int, ACL_AddNet, ( vlc_acl_t *p_acl, const char *psz_ip, int i_len, bool b_allow ) );
VLC_EXPORT( int, ACL_LoadFile, ( vlc_acl_t *p_acl, const char *path ) );

#endif
