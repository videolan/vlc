/*****************************************************************************
 * macosx.c : MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $$
 *
 * Authors: Colin Delacroix <colin@zoy.org>
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
 * Constants & more
 *****************************************************************************/

#ifndef __CARBONPREFIX__
    #define __CARBONPREFIX__

    // Needed for carbonization
    #define TARGET_API_MAC_CARBON 1

    // For the pascal to C or C to pascal string conversions in carbon
    #define OLDP2C 1
#endif

#include <Carbon/Carbon.h>


#define PLAYING		0
#define PAUSED		1
#define STOPPED		2


/*****************************************************************************
 * Type declarations that unfortunately need to be known to both
 * ...
 * Kind of a hack due to the fact that on Mac OS, there is little difference 
 * between the interface and the video output, and hence little separation
 * between those elements.
 *****************************************************************************/

/*****************************************************************************
 * vout_sys_t: MacOS X video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the MacOS X specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    Rect	wrect;
    WindowRef	p_window;
    short gwLocOffscreen;
    GWorldPtr p_gw[ 2 ];
    Boolean gNewNewGWorld;      /* can we allocate in VRAm or AGP memory ? */
    
    GDHandle  theGDList;
    Ptr				theBase;
    int				theRow;
    int				theDepth;
} vout_sys_t;
