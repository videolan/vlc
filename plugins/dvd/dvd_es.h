/* dvd_es.h: functions to find and select ES
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: dvd_es.h,v 1.1 2002/03/06 01:20:56 stef Exp $
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

void DVDLaunchDecoders  ( input_thread_t * );
void DVDReadVideo       ( input_thread_t * );
void DVDReadAudio       ( input_thread_t * );
void DVDReadSPU         ( input_thread_t * );

