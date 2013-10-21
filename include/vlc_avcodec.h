/*****************************************************************************
 * vlc_avcodec.h: VLC thread support for libavcodec
 *****************************************************************************
 * Copyright (C) 2009-2010 Rémi Denis-Courmont
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

#ifndef VLC_AVCODEC_H
# define VLC_AVCODEC_H 1

static inline void vlc_avcodec_lock (void)
{
    vlc_global_lock (VLC_AVCODEC_MUTEX);
}

static inline void vlc_avcodec_unlock (void)
{
    vlc_global_unlock (VLC_AVCODEC_MUTEX);
}

#endif
