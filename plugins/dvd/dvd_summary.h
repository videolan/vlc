/*****************************************************************************
 * dvd_summary.h: prototype of functions that print out current options.
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: dvd_summary.h,v 1.1 2001/05/19 00:39:30 stef Exp $
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

struct thread_dvd_data_s;

char * IfoLanguage( u16 );
void IfoPrintTitle( struct thread_dvd_data_s * );
void IfoPrintVideo( struct thread_dvd_data_s * );
void IfoPrintAudio( struct thread_dvd_data_s *, int );
void IfoPrintSpu  ( struct thread_dvd_data_s *, int );


