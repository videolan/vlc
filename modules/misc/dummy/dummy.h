/*****************************************************************************
 * dummy.h : dummy plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001, 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
int  OpenIntf     ( vlc_object_t * );

int  OpenDemux    ( vlc_object_t * );
void CloseDemux   ( vlc_object_t * );

int  OpenDecoder    ( vlc_object_t * );
int  OpenDecoderDump( vlc_object_t * );
void CloseDecoder   ( vlc_object_t * );

int  OpenEncoder  ( vlc_object_t * );
void CloseEncoder ( vlc_object_t * );

int  OpenVideo    ( vlc_object_t * );
int  OpenVideoStat( vlc_object_t * );
void CloseVideo   ( vlc_object_t * );

int  OpenRenderer ( vlc_object_t * );
