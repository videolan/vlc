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
#import "VLCAudio.h"
#import "VLCMediaList.h"
#import "VLCMedia.h"

@class VLCAudio;

/**
 * VLCLibrary object.
 * TODO: Documentation VLCLibrary
 */
@interface VLCLibrary : NSObject 
{
    void * instance;
    VLCAudio * audio;
}

/* Factories */
/**
 * TODO: Documentation + [VLCLibrary sharedLibrary]
 */
+ (VLCLibrary *)sharedLibrary;

/* Properties */
/**
 * TODO: Documentation VLCLibrary.audio
 */
@property (readonly) VLCAudio * audio;
@end

/*
 * Utility function
 */
// TODO: Figure out a better way to raise error messages
#define quit_on_exception( ex ) __quit_on_exception( (void *)(ex), __FUNCTION__, __FILE__, __LINE__ )
extern void __quit_on_exception( void * e, const char * function, const char * file, int line_number );
