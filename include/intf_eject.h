/*****************************************************************************
 * intf_eject.h: CD/DVD-ROM ejection handling functions
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: intf_eject.h,v 1.1 2002/01/09 02:01:14 sam Exp $
 *
 * Author: Julien Blache <jb@technologeek.org>
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

#ifndef PLUGIN
int intf_Eject( const char * );
#else
#   define intf_Eject p_symbols->intf_Eject
#endif
