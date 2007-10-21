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

#include <vlc/vlc.h>
#include <vlc/libvlc_structures.h>

static VLCLibrary *sharedLibrary = nil;

// TODO: Change from a terminal error to raising an exception?
void __quit_on_exception( void * e, const char * function, const char * file, int line_number )
{
    libvlc_exception_t *ex = (libvlc_exception_t *)e;
    if (libvlc_exception_raised( ex ))
    {
        /* XXX: localization */
        NSRunCriticalAlertPanel( @"Error", [NSString stringWithFormat:@"libvlc has thrown us an error: %s (%s:%d %s)", 
            libvlc_exception_get_message( ex ), file, line_number, function], @"Quit", nil, nil );
        exit( ex->i_code );
    }
}

static void *DestroySharedLibraryAtExit()
{
    /* Release the global object that may have been alloc-ed
     * in -[VLCLibrary init] */
    [sharedLibrary release];
    sharedLibrary = nil;

    return nil;
}

@implementation VLCLibrary
+ (VLCLibrary *)sharedLibrary
{
    if (!sharedLibrary) 
    {
        // Initialize a shared instance
        [[self alloc] init];
    }
    return [[sharedLibrary retain] autorelease];
}

+ (void *)sharedInstance
{
    return [[self sharedLibrary] instance];
}

- (id)init 
{
    if (self = [super init]) 
    {
        libvlc_exception_t ex;
        libvlc_exception_init( &ex );
        
        // Figure out the frameworks path
        char *applicationPath = strdup( [[NSString stringWithFormat:@"%@/Versions/Current/VLC", 
            [[NSBundle bundleForClass:[VLCLibrary class]] bundlePath]] UTF8String] );
        // TODO: Raise error if there is no memory available
        
        char *lib_vlc_params[] = { 
            applicationPath, "-I", "dummy", "-vvvv", 
            "--opengl-provider", "minimal_macosx", 
            "--no-video-title-show", NULL
        };
        
        instance = (void *)libvlc_new( 7, lib_vlc_params, &ex );
        quit_on_exception( &ex );
        
        if (!sharedLibrary) 
            sharedLibrary = self;
        
        // Assignment unneeded, as the audio unit will do it for us
        /*audio = */ [[VLCAudio alloc] initWithLibrary:self];
        
        // free allocated resources
        free( applicationPath );
        atexit(DestroySharedLibraryAtExit);
    }
    return self;
}

- (void)dealloc 
{
    if (instance) 
    {
        libvlc_exception_t ex;
        libvlc_exception_init( &ex );
        
        libvlc_destroy( instance, &ex );
    }
    instance = nil;
    [audio release];
    [super dealloc];
}

- (void *)instance
{
    return instance;
}

- (VLCAudio *)audio
{
    return audio;
}
@end

@implementation VLCLibrary (VLCAudioBridging)
- (void)setAudio:(VLCAudio *)value
{
    if (!audio)
        audio = value;
}
@end

