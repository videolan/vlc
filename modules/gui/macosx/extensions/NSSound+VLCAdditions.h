/*****************************************************************************
 * NSSound+VLCAdditions.h: Category that adds system volume control
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

#import <Cocoa/Cocoa.h>

/* Adds methods to change the system volume, needed for the apple remote code.
 *
 * This is simplified code, which won't let you set the exact volume
 * (that's what the audio output is for after all), but just the system volume
 * in steps of 1/16 (matching the default AR or volume key implementation).
 */
@interface NSSound (VLCAdditions)

+ (float)systemVolumeForChannel:(int)channel;
+ (bool)setSystemVolume:(float)volume forChannel:(int)channel;
+ (void)increaseSystemVolume;
+ (void)decreaseSystemVolume;

@end
