/*****************************************************************************
 * access.h: send info to access plugin.
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: access.h,v 1.1 2002/07/23 19:56:19 stef Exp $
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

union dvdplay_ctrl_u;

void dvdAccessSendControl( struct input_thread_t *, union dvdplay_ctrl_u * );
