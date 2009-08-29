/*****************************************************************************
 * stats.h : stats plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2008 the VideoLAN team
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Pierre d'Herbemont <pdherbemont@videolan.org>
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

/*****************************************************************************
 * External prototypes
 *****************************************************************************/

int  OpenDecoder  ( vlc_object_t * );
void CloseDecoder ( vlc_object_t * );

int  OpenEncoder  ( vlc_object_t * );
void CloseEncoder ( vlc_object_t * );

int  OpenDemux    ( vlc_object_t * );
void CloseDemux   ( vlc_object_t * );

#define kBufferSize 0x500
