/*****************************************************************************
 * VLCAudio.m: VLCKit.framework VLCAudio implementation
 *****************************************************************************
 * Copyright (C) 2007 Faustino E. Osuna
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Faustino E. Osuna <enrique.osuna # gmail.com>
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

#import "VLCAudio.h"
#import "VLCLibVLCBridging.h"

#define VOLUME_STEP                6
#define VOLUME_MAX                 200
#define VOLUME_MIN                 0

/* Notification Messages */
NSString * VLCMediaPlayerVolumeChanged = @"VLCMediaPlayerVolumeChanged"; 

/* libvlc event callback */
// TODO: Callbacks

@implementation VLCAudio

- (id)init
{
    return nil;
}

- (id)initWithLibrary:(VLCLibrary *)aLibrary
{
    if (![library audio] && (self = [super init]))
    {
        library = aLibrary;
        [library setAudio:self];
    }
    return self;
}

- (void)setMute:(BOOL)value
{
    libvlc_audio_set_mute([library instance], value);
}

- (BOOL)isMuted
{
    return libvlc_audio_get_mute([library instance]);
}

- (void)setVolume:(int)value
{
    if (value < VOLUME_MIN)
        value = VOLUME_MIN;
    else if (value > VOLUME_MAX)
        value = VOLUME_MAX;
    libvlc_audio_set_volume([library instance], value, NULL);
}

- (void)volumeUp
{
    int tempVolume = [self volume] + VOLUME_STEP;
    if (tempVolume > VOLUME_MAX)
        tempVolume = VOLUME_MAX;
    else if (tempVolume < VOLUME_MIN)
        tempVolume = VOLUME_MIN;
    [self setVolume: tempVolume];
}

- (void)volumeDown
{
    int tempVolume = [self volume] - VOLUME_STEP;
    if (tempVolume > VOLUME_MAX)
        tempVolume = VOLUME_MAX;
    else if (tempVolume < VOLUME_MIN)
        tempVolume = VOLUME_MIN;
    [self setVolume: tempVolume];
}

- (int)volume
{
    return libvlc_audio_get_volume([library instance]);
}
@end
