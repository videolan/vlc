/*****************************************************************************
 * VLCStreamSession.m: VLCKit.framework VLCStreamSession implementation
 *****************************************************************************
 * Copyright (C) 2008 Pierre d'Herbemont
 * Copyright (C) 2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import "VLCStreamSession.h"
#import "VLCLibVLCBridging.h"

@interface VLCStreamSession ()
@property (readwrite) BOOL isComplete;
@end

@implementation VLCStreamSession
@synthesize media=originalMedia;
@synthesize streamOutput;
@synthesize isComplete;

- (id)init
{
    if( self = [super init] )
    {
        reattemptedConnections = 0;
        [self addObserver:self forKeyPath:@"state" options:NSKeyValueObservingOptionNew context:nil];
        self.isComplete = NO;
    }
    return self;
}

- (void)dealloc
{
    [self removeObserver:self forKeyPath:@"state"];
    [super dealloc];
}

+ (id)streamSession
{
    return [[[self alloc] init] autorelease];
}


- (void)startStreaming
{
    self.isComplete = NO;
    [self play];
}

- (void)stopStreaming
{
    self.isComplete = YES;
    [super stop];
}

- (BOOL)play
{
    NSString * libvlcArgs;
    if( self.drawable )
        libvlcArgs = [NSString stringWithFormat:@"#duplicate{dst=display,dst=\"%@\"}",[streamOutput representedLibVLCOptions]];
    else
        libvlcArgs = [streamOutput representedLibVLCOptions];
    if( libvlcArgs )
    {
        [super setMedia: [VLCMedia mediaWithMedia:originalMedia andLibVLCOptions:
                                [NSDictionary dictionaryWithObject: libvlcArgs forKey: @"sout"]]];
    }
    else
    {
        [super setMedia: self.media];
    }
    [super play];
	return YES;
}

+ (NSSet *)keyPathsForValuesAffectingDescription
{
    return [NSSet setWithObjects:@"isCompleted", @"state", nil];
}

- (NSString *)description
{
    if([self isComplete])
        return @"Done.";
    else if([self state] == VLCMediaPlayerStateError)
        return @"Error while Converting. Open Console.app to diagnose.";
    else
        return @"Converting...";
}

+ (NSSet *)keyPathsForValuesAffectingEncounteredError
{
    return [NSSet setWithObjects:@"state", nil];
}

- (BOOL)encounteredError;
{
    return ([self state] == VLCMediaPlayerStateError);
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if([keyPath isEqualToString:@"state"])
    {
        if( (([self position] == 1.0 || [self state] == VLCMediaPlayerStateEnded || ([self state] == VLCMediaPlayerStateStopped && self.media)) ||
            [self encounteredError] ) && ![super.media subitems] )
        {
            self.isComplete = YES;
            return;
        }
        if( reattemptedConnections > 4 )
            return;

        /* Our media has in fact gained subitems, let's change our playing media */
        if( [[super.media subitems] count] > 0 )
        {
            [self stop];
            self.media = [[super.media subitems] mediaAtIndex:0];
            [self play];
            reattemptedConnections++;
        }
        return;
    }
    [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}

@end
