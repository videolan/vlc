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
int  E_(OpenIntf)     ( vlc_object_t * );

int  E_(OpenAccess)   ( vlc_object_t * );

int  E_(OpenDemux)    ( vlc_object_t * );
void E_(CloseDemux)   ( vlc_object_t * );

int  E_(OpenDecoder)  ( vlc_object_t * );
void E_(CloseDecoder) ( vlc_object_t * );

int  E_(OpenEncoder)  ( vlc_object_t * );
void E_(CloseEncoder) ( vlc_object_t * );

int  E_(OpenAudio)    ( vlc_object_t * );

int  E_(OpenVideo)    ( vlc_object_t * );

int  E_(OpenRenderer) ( vlc_object_t * );
