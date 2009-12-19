/*****************************************************************************
 * VLCLibrary.m: VLCKit.framework VLCLibrary implementation
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
                libvlc_errmsg(), file, line_number, function]
            userInfo:nil];
        libvlc_exception_clear( ex );
        @throw libvlcException;
    }
}

@implementation VLCLibrary
+ (VLCLibrary *)sharedLibrary
{
    if (!sharedLibrary) 
    {
        /* Initialize a shared instance */
        sharedLibrary = [[self alloc] init];
    }
    return sharedLibrary;
}

- (id)init 
{
    if (self = [super init]) 
    {
        libvlc_exception_t ex;
        libvlc_exception_init( &ex );
        
        NSArray *vlcParams = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"VLCParams"];
        if (!vlcParams) {
            NSMutableArray *defaultParams = [NSMutableArray array];
            [defaultParams addObject:@"-I dummy"];                                  // No interface
            [defaultParams addObject:@"--no-video-title-show"];                     // Don't show the title on overlay when starting to play
            [defaultParams addObject:@"--no-sout-keep"];
            [defaultParams addObject:@"--ignore-config"];                           // Don't read and write VLC config files
            [defaultParams addObject:@"--opengl-provider=minimal_macosx"];          // Use minimal_macosx
            [defaultParams addObject:@"--vout=minimal_macosx"];
            [defaultParams addObject:@"--verbose=2"];                               // Don't polute the log
            [defaultParams addObject:@"--vout=minimal_macosx"];
            [defaultParams addObject:@"--play-and-pause"];                          // When ending a stream pause it instead of stopping it
            vlcParams = defaultParams;
        }
    
        int paramNum = 0;
        const char *lib_vlc_params[[vlcParams count]];
        while (paramNum < [vlcParams count]) {
            NSString *vlcParam = [vlcParams objectAtIndex:paramNum];
            lib_vlc_params[paramNum] = [vlcParam cStringUsingEncoding:NSASCIIStringEncoding];
            paramNum++;
        }
        instance = (void *)libvlc_new( sizeof(lib_vlc_params)/sizeof(lib_vlc_params[0]), lib_vlc_params, &ex );
        catch_exception( &ex );
        NSAssert(instance, @"libvlc failed to initialize");
        
        // Assignment unneeded, as the audio unit will do it for us
        /*audio = */ [[VLCAudio alloc] initWithLibrary:self];
    }
    return self;
}

- (NSString *)version 
{
    return [NSString stringWithUTF8String:libvlc_get_version()];
}

- (NSString *)changeset 
{
    return [NSString stringWithUTF8String:libvlc_get_changeset()];
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
