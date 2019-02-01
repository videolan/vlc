/*****************************************************************************
 * NSSound+VLCAdditions.m: Category that adds system volume control
 *****************************************************************************
 * Copyright (C) 2003-2014 VLC authors and VideoLAN
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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

#import "NSSound+VLCAdditions.h"

#import <CoreAudio/CoreAudio.h>

#import "main/VLCMain.h"

@implementation NSSound (VLCAdditions)

+ (float)systemVolumeForChannel:(int)channel
{
    AudioDeviceID i_device;
    float f_volume;
    OSStatus err;
    UInt32 i_size;

    i_size = sizeof( i_device );
    AudioObjectPropertyAddress deviceAddress = { kAudioHardwarePropertyDefaultOutputDevice, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    err = AudioObjectGetPropertyData( kAudioObjectSystemObject, &deviceAddress, 0, NULL, &i_size, &i_device );
    if (err != noErr) {
        msg_Warn( getIntf(), "couldn't get main audio output device" );
        return .0;
    }

    AudioObjectPropertyAddress propertyAddress = { kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, channel };
    i_size = sizeof( f_volume );
    err = AudioObjectGetPropertyData(i_device, &propertyAddress, 0, NULL, &i_size, &f_volume);
    if (err != noErr) {
        msg_Warn( getIntf(), "couldn't get volume value" );
        return .0;
    }

    return f_volume;
}

+ (bool)setSystemVolume:(float)f_volume forChannel:(int)i_channel
{
    /* the following code will fail on S/PDIF devices. there is an easy work-around, but we'd like to match the OS behavior */

    AudioDeviceID i_device;
    OSStatus err;
    UInt32 i_size;
    Boolean b_writeable;

    i_size = sizeof( i_device );
    AudioObjectPropertyAddress deviceAddress = { kAudioHardwarePropertyDefaultOutputDevice, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    err = AudioObjectGetPropertyData( kAudioObjectSystemObject, &deviceAddress, 0, NULL, &i_size, &i_device );
    if (err != noErr) {
        msg_Warn( getIntf(), "couldn't get main audio output device" );
        return NO;
    }

    AudioObjectPropertyAddress propertyAddress = { kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, i_channel };
    i_size = sizeof( f_volume );
    err = AudioObjectIsPropertySettable( i_device, &propertyAddress, &b_writeable );
    if (err != noErr || !b_writeable ) {
        msg_Warn( getIntf(), "we can't set the main audio devices' volume" );
        return NO;
    }
    err = AudioObjectSetPropertyData(i_device, &propertyAddress, 0, NULL, i_size, &f_volume);
    if (err != noErr ) {
        msg_Warn( getIntf(), "failed to set the main device volume" );
        return NO;
    }

    return YES;
}

+ (void)increaseSystemVolume
{
    float f_volume = [NSSound systemVolumeForChannel:1]; // we trust that mono is always available and that all channels got the same volume
    f_volume += .0625; // 1/16 to match the OS
    bool b_returned = YES;

    /* since core audio doesn't provide a reasonable way to see how many channels we got, let's see how long we can do this */
    for (int x = 1; b_returned ; x++)
        b_returned = [NSSound setSystemVolume: f_volume forChannel:x];
}

+ (void)decreaseSystemVolume
{
    float f_volume = [NSSound systemVolumeForChannel:1]; // we trust that mono is always available and that all channels got the same volume
    f_volume -= .0625; // 1/16 to match the OS
    bool b_returned = YES;

    /* since core audio doesn't provide a reasonable way to see how many channels we got, let's see how long we can do this */
    for (int x = 1; b_returned ; x++)
        b_returned = [NSSound setSystemVolume: f_volume forChannel:x];
}

@end
