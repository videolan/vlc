/*****************************************************************************
 * charset.h: Determine a canonical name for the current locale's character encoding.
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: charset.h,v 1.1 2003/08/23 12:59:31 hartman Exp $
 *
 * Author: Derk-Jan Hartman <thedj at users.sourceforge.net>
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

VLC_EXPORT( vlc_bool_t, vlc_current_charset, ( char ** ) );
