/*****************************************************************************
 * VLCLibrary.h: VLC.framework VLCLibrary implementation
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import "VLCLibrary.h"

static libvlc_instance_t * shared_instance = NULL;

@implementation VLCLibrary
+ (libvlc_instance_t *)sharedInstance
{
    if(!shared_instance)
    {
        libvlc_exception_t ex;
        char *lib_vlc_params[] = { "vlc", "-I", "dummy", "-vvvvvv" };
        libvlc_exception_init( &ex );
        
        shared_instance = libvlc_new( 4, lib_vlc_params, &ex );
        quit_on_exception( &ex );
    }
    return shared_instance;
}
@end

