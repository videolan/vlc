/*****************************************************************************
 * CoreInteraction.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2012 Felix Paul Kühne
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
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

#import <Cocoa/Cocoa.h>
#import <vlc_common.h>
#import <vlc_interface.h>

@interface VLCCoreInteraction : NSObject {
    int i_currentPlaybackRate;
    BOOL b_lockAspectRatio;
}
+ (VLCCoreInteraction *)sharedInstance;

- (void)play;
- (void)pause;
- (void)stop;
- (void)faster;
- (void)slower;
- (void)normalSpeed;
- (void)toggleRecord;
- (void)setPlaybackRate:(int)i_value;
- (int)playbackRate;
- (void)next;
- (void)previous;
- (BOOL)isPlaying;
- (int)currentTime;
- (void)setCurrentTime:(int)i_value;
- (int)durationOfCurrentPlaylistItem;
- (NSURL*)URLOfCurrentPlaylistItem;
- (NSString*)nameOfCurrentPlaylistItem;
- (void)forward;        //LEGACY SUPPORT
- (void)backward;       //LEGACY SUPPORT
- (void)forwardExtraShort;
- (void)backwardExtraShort;
- (void)forwardShort;
- (void)backwardShort;
- (void)forwardMedium;
- (void)backwardMedium;
- (void)forwardLong;
- (void)backwardLong;

- (void)repeatOne;
- (void)repeatAll;
- (void)repeatOff;
- (void)shuffle;

- (void)volumeUp;
- (void)volumeDown;
- (void)mute;
- (BOOL)isMuted;
- (int)volume;
- (void)setVolume:(int)i_value;

- (void)setAspectRatioLocked:(BOOL)b_value;
- (BOOL)aspectRatioIsLocked;
- (void)toggleFullscreen;
@end
