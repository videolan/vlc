/*****************************************************************************
 * es.h: functions to handle elementary streams.
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: demux.h,v 1.1 2002/08/04 17:23:42 sam Exp $
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

void dvdplay_DeleteES( struct input_thread_s * );
void dvdplay_Video( struct input_thread_s * );
void dvdplay_Audio( struct input_thread_s * );
void dvdplay_Subp( struct input_thread_s * );
void dvdplay_ES( struct input_thread_s * );
void dvdplay_LaunchDecoders( struct input_thread_s * );
