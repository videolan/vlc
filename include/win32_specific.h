/*****************************************************************************
 * win32_specific.h: Win32 specific features 
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: win32_specific.h,v 1.3 2002/06/01 12:31:58 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef BOOL (WINAPI *SIGNALOBJECTANDWAIT)( HANDLE, HANDLE, DWORD, BOOL );

/*****************************************************************************
 * main_sys_t: system specific descriptor
 *****************************************************************************
 * This structure is a system specific descriptor. It describes the Win32
 * properties of the program.
 *****************************************************************************/
struct main_sys_s
{
    SIGNALOBJECTANDWAIT SignalObjectAndWait;
    vlc_bool_t b_fast_pthread;
};

