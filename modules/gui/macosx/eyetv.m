/*****************************************************************************
* eyetv.m: small class to control the notification parts of the EyeTV plugin
*****************************************************************************
* Copyright (C) 2006-2011 VLC authors and VideoLAN
* $Id$
*
* Authors: Felix Kühne <fkuehne at videolan dot org>
*          Damien Fouilleul <damienf at videolan dot org>
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

#import "eyetv.h"
/* for apple event interaction [carbon] */
//#import <Foundation/NSAppleScript>
/* for various VLC core related calls */
#import "intf.h"

@implementation VLCEyeTVController

@synthesize eyeTVRunning = b_eyeTVactive, deviceConnected = b_deviceConnected;

static VLCEyeTVController *_o_sharedInstance = nil;

+ (VLCEyeTVController *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance)
        [self dealloc];
    else {
        _o_sharedInstance = [super init];

        [[NSDistributedNotificationCenter defaultCenter]
                    addObserver: self
                       selector: @selector(globalNotificationReceived:)
                           name: NULL
                         object: @"VLCEyeTVSupport"
             suspensionBehavior: NSNotificationSuspensionBehaviorDeliverImmediately];
    }

    return _o_sharedInstance;
}

- (void)globalNotificationReceived: (NSNotification *)theNotification
{
    /* update our info on the used device */
    if ([[theNotification name] isEqualToString: @"DeviceAdded"])
        b_deviceConnected = YES;
    if ([[theNotification name] isEqualToString: @"DeviceRemoved"])
        b_deviceConnected = NO;

    /* is eyetv running? */
    if ([[theNotification name] isEqualToString: @"PluginInit"])
        b_eyeTVactive = YES;
    if ([[theNotification name] isEqualToString: @"PluginQuit"])
        b_eyeTVactive = NO;
}

- (void)launchEyeTV
{
    NSAppleScript *script = [[NSAppleScript alloc] initWithSource:
                @"tell application \"EyeTV\"\n"
                   "launch with server mode\n"
                 "end tell"];
    NSDictionary *errorDict;
    NSAppleEventDescriptor *descriptor = [script executeAndReturnError:&errorDict];
    if (nil == descriptor) {
        NSString *errorString = [errorDict objectForKey:NSAppleScriptErrorMessage];
        NSLog(@"opening EyeTV failed with error status '%@'", errorString);
    }
    [script release];
}

- (int)channel
{
    int currentChannel = 0;
    NSAppleScript *script = [[NSAppleScript alloc] initWithSource:
            @"tell application \"EyeTV\" to get current channel"];
    NSDictionary *errorDict;
    NSAppleEventDescriptor *descriptor = [script executeAndReturnError:&errorDict];
    if (nil == descriptor) {
        NSString *errorString = [errorDict objectForKey:NSAppleScriptErrorMessage];
        NSLog(@"EyeTV channel inventory failed with error status '%@'", errorString);
    } else
        currentChannel = (int)[descriptor int32Value];
    [script release];
    return currentChannel;
}

- (int)switchChannelUp:(BOOL)b_yesOrNo
{
    int currentChannel = 0;
    NSAppleScript *script;
    NSDictionary *errorDict;
    NSAppleEventDescriptor *descriptor;

    if (b_yesOrNo == YES) {
        script = [[NSAppleScript alloc] initWithSource:
                    @"tell application \"EyeTV\"\n"
                       "channel_up\n"
                       "get current channel\n"
                     "end tell"];
        NSLog(@"telling eyetv to switch 1 channel up");
    } else {
        script = [[NSAppleScript alloc] initWithSource:
                    @"tell application \"EyeTV\"\n"
                       "channel_down\n"
                       "get current channel\n"
                     "end tell"];
        NSLog(@"telling eyetv to switch 1 channel down");
    }

    descriptor = [script executeAndReturnError:&errorDict];
    if (nil == descriptor) {
        NSString *errorString = [errorDict objectForKey:NSAppleScriptErrorMessage];
        NSLog(@"EyeTV channel change failed with error status '%@'", errorString);
    } else
        currentChannel = (int)[descriptor int32Value];

    [script release];
    return currentChannel;
}

- (void)setChannel: (int)theChannelNum
{
    NSAppleScript *script;
    switch(theChannelNum) {
        case -2: // Composite
            script = [[NSAppleScript alloc] initWithSource:
                        @"tell application \"EyeTV\"\n"
                         "  input_change input source composite video input\n"
                         "  show player_window\n"
                         "end tell"];
            break;
        case -1: // S-Video
            script = [[NSAppleScript alloc] initWithSource:
                        @"tell application \"EyeTV\"\n"
                         "  input_change input source S video input\n"
                         "  show player_window\n"
                         "end tell"];
            break;
        case 0: // Last
            script = [[NSAppleScript alloc] initWithSource:
                        @"tell application \"EyeTV\"\n"
                         "  show player_window\n"
                         "end tell"];
            break;
        default:
            if (theChannelNum > 0) {
                NSString *channel_change = [NSString stringWithFormat:
                    @"tell application \"EyeTV\"\n"
                     "  channel_change channel number %d\n"
                     "  show player_window\n"
                     "end tell", theChannelNum];
                script = [[NSAppleScript alloc] initWithSource:channel_change];
            }
            else
                return;
    }
    NSDictionary *errorDict;
    NSAppleEventDescriptor *descriptor = [script executeAndReturnError:&errorDict];
    if (nil == descriptor) {
        NSString *errorString = [errorDict objectForKey:NSAppleScriptErrorMessage];
        NSLog(@"EyeTV source change failed with error status '%@'", errorString);
    }
    [script release];
}

- (NSEnumerator *)allChannels
{
    NSEnumerator *channels = nil;
    NSAppleScript *script = [[NSAppleScript alloc] initWithSource:
            @"tell application \"EyeTV\" to get name of every channel"];
    NSDictionary *errorDict;
    NSAppleEventDescriptor *descriptor = [script executeAndReturnError:&errorDict];
    if (nil == descriptor) {
        NSString *errorString = [errorDict objectForKey:NSAppleScriptErrorMessage];
        NSLog(@"EyeTV channel inventory failed with error status '%@'", errorString);
    } else {
        int count = [descriptor numberOfItems];
        int x=0;
        NSMutableArray *channelArray = [NSMutableArray arrayWithCapacity:count];
        while(x++ < count)
            [channelArray addObject:[[descriptor descriptorAtIndex:x] stringValue]];

        channels = [channelArray objectEnumerator];
    }
    [script release];
    return channels;
}

@end
