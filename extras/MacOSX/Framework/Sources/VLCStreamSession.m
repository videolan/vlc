//
//  VLCStreamSession.m
//  VLCKit
//
//  Created by Pierre d'Herbemont on 1/12/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import "VLCStreamSession.h"
#import "VLCLibVLCBridging.h"

@implementation VLCStreamSession
@synthesize media=originalMedia;
@synthesize streamOutput;

+ (id)streamSession
{
    return [[[self alloc] init] autorelease];
}


- (void)startStreaming;
{
    [self play];
}

- (void)play;
{
    NSString * libvlcArgs;
    if( self.drawable )
    {
        libvlcArgs = [NSString stringWithFormat:@"duplicate{dst=display,dst=\"%@\"}",[streamOutput representedLibVLCOptions]];
    }
    else
    {
        libvlcArgs = [streamOutput representedLibVLCOptions];
    }
    [super setMedia: [VLCMedia mediaWithMedia:originalMedia andLibVLCOptions:
                            [NSDictionary dictionaryWithObject: libvlcArgs
                                                        forKey: @"sout"]]];
    [super play];
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

+ (NSSet *)keyPathsForValuesAffectingIsComplete
{
    return [NSSet setWithObjects:@"playing", @"state", @"position", nil];
}

- (BOOL)isComplete
{
    return ([self position] == 1.0 || [self state] == VLCMediaPlayerStateEnded || ([self state] == VLCMediaPlayerStateStopped && self.media));
}
@end
