/*****************************************************************************
* eyetv.m: small class to control the notification parts of the EyeTV plugin
*****************************************************************************
* Copyright (C) 2006-2007 the VideoLAN team
* $Id$
*
* Authors: Felix Kühne <fkuehne at videolan dot org>
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

static VLCEyeTVController *_o_sharedInstance = nil;

+ (VLCEyeTVController *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init 
{
    if (_o_sharedInstance) {
        [self dealloc];
    } else {
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
    msg_Dbg( VLCIntf, "notification received in VLC with name %s and object %s",
             [[theNotification name] UTF8String], [[theNotification object] UTF8String] );

    /* update our info on the used device */
    if( [[theNotification name] isEqualToString: @"DeviceAdded"] )
        b_deviceConnected = YES;
    if( [[theNotification name] isEqualToString: @"DeviceRemoved"] )
        b_deviceConnected = NO;

    /* is eyetv running? */
    if( [[theNotification name] isEqualToString: @"PluginInit"] )
        b_eyeTVactive = YES;
    if( [[theNotification name] isEqualToString: @"PluginQuit"] )
        b_eyeTVactive = NO;
}

- (BOOL)isEyeTVrunning
{
    return b_eyeTVactive;
}

- (BOOL)isDeviceConnected
{
    return b_deviceConnected;
}


- (void)launchEyeTV
{
    NSAppleScript *script = [[NSAppleScript alloc] initWithSource:
                @"tell application \"EyeTV\" to launch with server mode"];
    NSDictionary *errorDict;
    NSAppleEventDescriptor *descriptor = [script executeAndReturnError:&errorDict];
    if( nil == descriptor ) 
    {
        NSString *errorString = [errorDict objectForKey:NSAppleScriptErrorMessage];
        msg_Err( VLCIntf, "opening EyeTV failed with error code %s", [errorString UTF8String] );
    }
    [script release];
}

- (void)switchChannelUp:(BOOL)b_yesOrNo
{
    NSAppleScript *script;
    NSDictionary *errorDict;
    NSAppleEventDescriptor *descriptor;

    if( b_yesOrNo == YES )
    {
        script = [[NSAppleScript alloc] initWithSource:
                    @"tell application \"EyeTV\" to channel_up"];
        msg_Dbg( VLCIntf, "telling eyetv to switch 1 channel up" );
    }
    else
    {
        script = [[NSAppleScript alloc] initWithSource:@"tell application \"EyeTV\" to channel_down"];
        msg_Dbg( VLCIntf, "telling eyetv to switch 1 channel down" );
    }
    
    descriptor = [script executeAndReturnError:&errorDict];
    if( nil == descriptor ) 
    {
        NSString *errorString = [errorDict objectForKey:NSAppleScriptErrorMessage];
        msg_Err( VLCIntf, "EyeTV channel change failed with error code %s", [errorString UTF8String] );
    }
    [script release];
}

- (void)selectChannel: (int)theChannelNum
{
    NSAppleScript *script;
    switch( theChannelNum )
    {
        case -2: // Composite
            script = [[NSAppleScript alloc] initWithSource:
                        @"tell application \"EyeTV\" to input_change input source composite video input"];
            break;
        case -1: // S-Video
            script = [[NSAppleScript alloc] initWithSource:
                        @"tell application \"EyeTV\" to input_change input source S video input"];
            break;
        case 0: // Tuner
            script = [[NSAppleScript alloc] initWithSource:
                        @"tell application \"EyeTV\" to input_change input source tuner input"];
            break;
        default:
            if( theChannelNum > 0 )
            {
                NSString *channel_change = [NSString stringWithFormat:
                    @"tell application \"EyeTV\" to channel_change channel number %d", theChannelNum];
                script = [[NSAppleScript alloc] initWithSource:channel_change];
            }
            else
                return;
    }
    NSDictionary *errorDict;
    NSAppleEventDescriptor *descriptor = [script executeAndReturnError:&errorDict];
    if( nil == descriptor ) 
    {
        NSString *errorString = [errorDict objectForKey:NSAppleScriptErrorMessage];
        msg_Err( VLCIntf, "EyeTV source change failed with error code %s", [errorString UTF8String] );
    }
    [script release];
}

- (NSEnumerator *)getChannels
{
    NSEnumerator *channels = nil;
    NSAppleScript *script = [[NSAppleScript alloc] initWithSource:
            @"tell application \"EyeTV\" to get name of every channel"];
    NSDictionary *errorDict;
    NSAppleEventDescriptor *descriptor = [script executeAndReturnError:&errorDict];
    if( nil == descriptor ) 
    {
        NSString *errorString = [errorDict objectForKey:NSAppleScriptErrorMessage];
        msg_Err( VLCIntf, "EyeTV channel inventory failed with error code %s", [errorString UTF8String] );
    }
    else
    {
        int count = [descriptor numberOfItems];
        int x=0; 
        NSMutableArray *channelArray = [NSMutableArray arrayWithCapacity:count];
        while( x++ < count ) {
            [channelArray addObject:[[descriptor descriptorAtIndex:x] stringValue]];
        }
        channels = [channelArray objectEnumerator];
    }
    [script release];
    return channels;
}

@end
