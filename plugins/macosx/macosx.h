/*****************************************************************************
 * macosx.h : MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $$
 *
 * Authors: Florian G. Pflug <fgp@phlo.org>
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

/*****************************************************************************
 * The vout struct as access from both, the output and the interface module
 *****************************************************************************/
#include <QuickTime/QuickTime.h>

#define OSX_INTF_VOUT_QDPORT_CHANGE	0x0001
#define OSX_INTF_VOUT_SIZE_CHANGE	0x0002
#define OSX_VOUT_INTF_REQUEST_QDPORT	0x0004
#define OSX_VOUT_INTF_RELEASE_QDPORT	0x0008

/* This struct is included as the _FIRST_ member in vout_sys_t */
/* That way the interface can cast the vout_sys_t to osx_com_t */
/* and doesn't need the definition of vout_sys_t */
#ifndef OSX_COM_TYPE
    #define OSX_COM_TYPE osx_com_t
    #define OSX_COM_STRUCT osx_com_s
#endif
typedef struct OSX_COM_STRUCT {
    unsigned int i_changes ;
    
    CGrafPtr p_qdport ;
} OSX_COM_TYPE ;
