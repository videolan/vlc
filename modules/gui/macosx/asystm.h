//
//  asystm.h
//  
//
//  Created by Heiko Panther on Tue Sep 10 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>
#import "adev_discovery.h"
#import "intf.h"

/*****************************************************************************
* MacOSXSoundOption
* Each audio device can give several sound options: there might be several
* streams on one device, each can have different formats which might qualify
* as an option.
* We record format and channels, since these attributes are requirements
* from the user and the aout should deliver what the user wants. This
* selection is basically done when the user chooses the output option.
* We do not record sample rate and bit depth, since these attributes are
* tied to the media source, and the device format that matches these media
* formats best should be selected. This selection is done when the aout
* module is created with a certain stream, and asks the asystm for a device.
*****************************************************************************/
@interface MacOSXSoundOption:NSObject
{
    NSString *name;
    AudioDeviceID deviceID;
    UInt32 streamIndex;
    UInt32 mFormatID;
    UInt32 mChannels;
}
- (id)initWithName:(NSString*)_name deviceID:(AudioDeviceID)devID streamIndex:(UInt32)strInd formatID:(UInt32)formID chans:(UInt32)chans;
- (AudioDeviceID)deviceID;
- (UInt32)streamIndex;
- (UInt32)mFormatID;
- (UInt32)mChannels;
- (void)dealloc;
- (NSString*)name;
@end


@interface MacOSXAudioSystem : NSObject {
    VLCMain *main;
    /* selected output device */
    NSMenuItem *selectedOutput;
    NSMenu *newMenu;
}
- (id)initWithGUI:(VLCMain*)main;
- (AudioStreamID) getStreamIDForIndex:(UInt32)streamIndex device:(AudioDeviceID)deviceID;
- (void)CheckDevice:(AudioDeviceID)deviceID isInput:(bool)isInput;
- (void)registerSoundOption:(MacOSXSoundOption*)option;
- (void)selectAction:(id)sender;
- (AudioStreamID)getSelectedDeviceSetToRate:(int)preferredSampleRate;
- (void)dealloc;
@end
