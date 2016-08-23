/*****************************************************************************
 * vlc_vaapi.h: VAAPI helper for VLC
 *****************************************************************************
 * Copyright (C) 2017 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Thomas Guillem <thomas@gllm.fr>
 *          Petri Hintukainen <phintuka@gmail.com>
 *          Victorien Le Couviour--Tuffet <victorien.lecouviour.tuffet@gmail.com>
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

#ifndef VLC_VAAPI_H
# define VLC_VAAPI_H

#include <va/va.h>

/**************************
 * VA instance management *
 **************************/

/* Allocates the VA instance and sets the reference counter to 1. */
int
vlc_vaapi_SetInstance(VADisplay dpy);

/* Retrieve the VA instance and increases the reference counter by 1. */
VADisplay
vlc_vaapi_GetInstance(void);

/* Decreases the reference counter by 1 and frees the instance if that counter
   reaches 0. */
void
vlc_vaapi_ReleaseInstance(VADisplay *);

#endif /* VLC_VAAPI_H */
