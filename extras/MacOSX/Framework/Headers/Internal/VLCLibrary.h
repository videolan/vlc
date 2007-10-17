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


#import <Cocoa/Cocoa.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_structures.h>

#import <VLC/VLCPlaylist.h>

/*
 * VLCLibrary object
 */

@interface VLCLibrary : NSObject
+ (libvlc_instance_t *)sharedInstance;
@end

/*
 * Utility function
 */
#define quit_on_exception( ex ) __quit_on_exception( ex, __FUNCTION__, __FILE__, __LINE__ )
static inline void __quit_on_exception( libvlc_exception_t * ex, const char * function, const char * file, int line_number )
{
    if (libvlc_exception_raised( ex ))
    {
        /* XXX: localization */
        NSRunCriticalAlertPanel( @"Error", [NSString stringWithFormat:@"libvlc has thrown us an error: %s (%s:%d %s)", libvlc_exception_get_message(ex), file, line_number, function], @"Quit", nil, nil );
        exit( ex->i_code );
    }
}

/*
 * LibVLCBridging category
 */

@interface VLCPlaylist (LibVLCBridging)
+ (id) playlistWithLibVLCMediaList: (libvlc_media_list_t *)p_new_mlist;
- (libvlc_media_list_t *) libVLCMediaList;
@end

@interface VLCMedia (LibVLCBridging)
- (id) initWithLibVLCMediaDescriptor:  (libvlc_media_descriptor_t *)p_md;
+ (id) mediaWithLibVLCMediaDescriptor: (libvlc_media_descriptor_t *)p_md;
- (libvlc_media_descriptor_t *) libVLCMediaDescriptor;
@end
