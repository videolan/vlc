/*****************************************************************************
 * os_window.h: Wrapper for the OSWindow class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: os_window.h,v 1.3 2003/04/16 21:40:07 ipkiss Exp $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
 *          Emmanuel Puig    <karibu@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/


#if defined( WIN32 )
    #include "win32/win32_dragdrop.h"
    #include "win32/win32_window.h"
    #define OSWindow Win32Window
#else    
    #include "gtk2/gtk2_dragdrop.h"
    #include "gtk2/gtk2_window.h"
    #define OSWindow GTK2Window
#endif
