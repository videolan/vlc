/*****************************************************************************
 * vlc_aout_intf.h : audio output control
 *****************************************************************************
 * Copyright (C) 2002-2011 VLC authors and VideoLAN
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

#ifndef VLC_AOUT_INTF_H
#define VLC_AOUT_INTF_H 1

/**
 * \file
 * This file defines functions, structures and macros for audio output object
 */

#define AOUT_VOLUME_DEFAULT             256
#define AOUT_VOLUME_MAX                 512

VLC_API float aout_VolumeGet( vlc_object_t * );
#define aout_VolumeGet(a) aout_VolumeGet(VLC_OBJECT(a))
VLC_API int aout_VolumeSet( vlc_object_t *, float );
#define aout_VolumeSet(a, b) aout_VolumeSet(VLC_OBJECT(a), b)
VLC_API int aout_MuteSet( vlc_object_t *, bool );
#define aout_MuteSet(a, b) aout_MuteSet(VLC_OBJECT(a), b)
VLC_API int aout_MuteGet( vlc_object_t * );
#define aout_MuteGet(a) aout_MuteGet(VLC_OBJECT(a))

#endif /* _VLC_AOUT_H */
