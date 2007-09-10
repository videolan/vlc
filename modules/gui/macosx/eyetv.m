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
#import <ApplicationServices/ApplicationServices.h> 
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
    OSStatus stat;
    FSRef eyetvPath;

    stat = LSFindApplicationForInfo ( 'EyTV',
                                       CFSTR("com.elgato.eyetv"),
                                       NULL,
                                       &eyetvPath,
                                       NULL );
    if( stat != noErr )
        msg_Err( VLCIntf, "finding EyeTV failed with error code %i", (int)stat );
    
    stat = LSOpenFSRef( &eyetvPath, NULL );
    if( stat != noErr )
        msg_Err( VLCIntf, "opening EyeTV failed with error code %i", (int)stat );
    
}

- (void)switchChannelUp:(BOOL)b_yesOrNo
{
    OSErr err;
    AppleEvent ourAE = {typeNull, nil};
    AEBuildError theBuildError;
    const OSType eyetvSignature = 'EyTV';   /* carbon FOURCC style */
    OSType eyetvCommand;
    
    if( b_yesOrNo == YES )
    {
        eyetvCommand = 'Chup';     /* same style */
        msg_Dbg( VLCIntf, "telling eyetv to switch 1 channel up" );
    }
    else
    {
        eyetvCommand = 'Chdn';     /* same style again */
        msg_Dbg( VLCIntf, "telling eyetv to switch 1 channel down" );
    }
    
    err = AEBuildAppleEvent(
                            /* EyeTV script suite */ eyetvSignature,
                            /* command */ eyetvCommand,
                            /* signature type */ typeApplSignature,
                            /* signature */ &eyetvSignature,
                            /* signature size */ sizeof(eyetvSignature),
                            /* basic return id */ kAutoGenerateReturnID,
                            /* generic transaction id */ kAnyTransactionID,
                            /* to-be-created AE */ &ourAE,
                            /* get some possible errors */ &theBuildError, 
                            /* got no params for now */ "" );
    if( err != aeBuildSyntaxNoErr )
    {
        msg_Err( VLCIntf, "Error %i encountered while trying to the build the AE to launch eyetv.\n" \
                 "additionally, the following info was returned: AEBuildErrorCode:%i at pos:%i", 
                 (int)err, theBuildError.fError, theBuildError.fErrorPos);
        return;
    }
    else
        msg_Dbg( VLCIntf, "AE created successfully, trying to send now" );
    
    err = AESendMessage(
                        /* our AE */ &ourAE,
                        /* no need for a response-AE */ NULL,
                        /* we neither want a response nor user interaction */ kAENoReply | kAENeverInteract,
                        /* we don't need a special time-out */ kAEDefaultTimeout );
    if( err != noErr )
        msg_Err( VLCIntf, "Error %i encountered while trying to tell EyeTV to switch channel", (int)err );
    
    err = AEDisposeDesc(&ourAE);
}

- (void)selectChannel: (int)theChannelNum
{
}

- (int)getNumberOfChannels
{
    return 2;
}

- (NSString *)getNameOfChannel: (int)theChannelNum
{
    return @"dummy name";
}

@end
