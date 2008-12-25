/*****************************************************************************
 * VLCPlayerController.m: Front Row plugin
 *****************************************************************************
 * Copyright (C) 2007 - 2008 the VideoLAN Team
 * $Id$
 *
 * Authors: hyei
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

#import "VLCPlayerController.h"

#import <BackRow/BREvent.h>
#import <BackRow/BRLayer.h>
#import <BackRow/BRControllerStack.h>

@implementation VLCPlayerController

- init
{
    self = [super init];
    
    BRLayer * layer = [self layer];
    _mediaLayer = [VLCMediaLayer layer];
    _mediaLayer.frame = layer.bounds;
    _mediaLayer.autoresizingMask = kCALayerWidthSizable|kCALayerHeightSizable;
    
    [layer addSublayer:_mediaLayer];
    
    return self;
}

- (VLCMedia *)media
{
    return [_mediaLayer media];
}

- (void)setMedia:(VLCMedia *)media
{
    _mediaLayer.media = media;
}

- (void)brEventAction:(BREvent*)event
{
    BREventUsage usage = [event usage];
    BREventValue value = [event value];
    VLCMediaPlayer * player = [_mediaLayer player];
    
    NSLog(@"usage: %d value: %d", usage, value);
    
    switch(usage) {
        case BREventOKUsage:
            [_mediaLayer playPause];
            break;
        case BREventRightUsage:
        {
            NSLog(@"RIGHT");
            float position = [player position];
            position += 0.05;
            position = MIN(1.0, MAX(0.0, position));
            [player setPosition:position];
            break;
        }
        case BREventLeftUsage:
        {
            NSLog(@"LEFT");
            float position = [player position];
            position -= 0.05;
            position = MIN(1.0, MAX(0.0, position));
            [player setPosition:position];
            break;
        }
        case BREventUpUsage:
        {
            NSLog(@"UP");
            [[[VLCLibrary sharedLibrary] audio] setVolume:[[[VLCLibrary sharedLibrary] audio] volume]+20];
            break;
        }
        case BREventDownUsage:
        {
            NSLog(@"DOWN");
            [[[VLCLibrary sharedLibrary] audio] setVolume:[[[VLCLibrary sharedLibrary] audio] volume]-20];
            break;
        }
        case BREventMenuUsage:
            [[self stack] popController];
        default:
            break;
    }
}

- (BOOL)firstResponder
{
    return YES;
}

- (void)controlWillDeactivate
{
    [_mediaLayer.player pause];
    [super controlWillDeactivate];
}

- (void)controlWasActivated
{
    [super controlWasActivated];
    [_mediaLayer.player play];
}

@end
