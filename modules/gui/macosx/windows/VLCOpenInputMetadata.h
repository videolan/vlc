/*****************************************************************************
 * VLCOpenInputMetadata.h: macOS interface
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan dot org>
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

#import <Foundation/Foundation.h>

@interface VLCOpenInputMetadata : NSObject

/**
 * this is the MRL of the future input item and defines where to search for media
 * it is the only required property, because if unset we don't know what to play
 */
@property (readwrite, copy) NSString *MRLString;

/**
 * this is an optional property to define the item name
 * if not set, the MRL or (if suitable) a file name will be displayed to the user
 */
@property (readwrite, copy) NSString *itemName;

/**
 * this is an optional property to define custom playback options
 * this typically relies on VLC's private API and shall be considered potentially unstable
 */
@property (readwrite, copy) NSArray *playbackOptions;

@end
