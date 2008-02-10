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
#import "VLCLibVLCBridging.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc/libvlc_structures.h>

static VLCLibrary * sharedLibrary = nil;

void __catch_exception( void * e, const char * function, const char * file, int line_number )
{
    libvlc_exception_t * ex = (libvlc_exception_t *)e;
    if( libvlc_exception_raised( ex ) )
    {
        NSException* libvlcException = [NSException
            exceptionWithName:@"LibVLCException"
            reason:[NSString stringWithFormat:@"libvlc has thrown us an error: %s (%s:%d %s)", 
                libvlc_exception_get_message( ex ), file, line_number, function]
            userInfo:nil];
        libvlc_exception_clear( ex );
        @throw libvlcException;
    }
}

static void * DestroySharedLibraryAtExit( void )
{
    /* Release the global object that may have been alloc-ed
     * in -[VLCLibrary init] */
    [sharedLibrary release];
    sharedLibrary = nil;

    return NULL;
}

@implementation VLCLibrary
+ (VLCLibrary *)sharedLibrary
{
    if (!sharedLibrary) 
    {
        /* Initialize a shared instance */
        sharedLibrary = [[self alloc] init];
        
        /* Make sure, this will get released at some point */
        atexit( (void *)DestroySharedLibraryAtExit );
    }
    return [[sharedLibrary retain] autorelease];
}

- (id)init 
{
    if (self = [super init]) 
    {
        libvlc_exception_t ex;
        libvlc_exception_init( &ex );
        
        const char * lib_vlc_params[] = { 
            "-I", "dummy", "--vout=opengllayer", 
            "--no-video-title-show", "--no-sout-keep"
            //, "--control=motion", "--motion-use-rotate", "--video-filter=rotate"
        };
        
        instance = (void *)libvlc_new( sizeof(lib_vlc_params)/sizeof(lib_vlc_params[0]), lib_vlc_params, &ex );
        catch_exception( &ex );
        
        // Assignment unneeded, as the audio unit will do it for us
        /*audio = */ [[VLCAudio alloc] initWithLibrary:self];
    }
    return self;
}

- (void)dealloc 
{
    if( instance ) 
        libvlc_release( instance );
    
    if( self == sharedLibrary ) 
        sharedLibrary = nil;
    
    instance = nil;
    [audio release];
    [super dealloc];
}

@synthesize audio;
@end

@implementation VLCLibrary (VLCLibVLCBridging)
+ (void *)sharedInstance
{
    return [self sharedLibrary].instance;
}

- (void *)instance
{
    return instance;
}
@end

@implementation VLCLibrary (VLCAudioBridging)
- (void)setAudio:(VLCAudio *)value
{
    if (!audio)
        audio = value;
}
@end
