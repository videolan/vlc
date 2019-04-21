/*****************************************************************************
 * applescript.h: MacOS X AppleScript support
 *****************************************************************************
 * Copyright (C) 2002-2019 VLC authors and VideoLAN
 *
 * Authors: Derk-Jan Hartman <thedj@users.sourceforge.net>
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

/*****************************************************************************
 * VLGetURLScriptCommand interface
 *****************************************************************************/
@interface VLGetURLScriptCommand : NSScriptCommand
@end

/*****************************************************************************
 * VLControlScriptCommand interface
 *****************************************************************************/
@interface VLControlScriptCommand : NSScriptCommand
@end

/*****************************************************************************
* Category that adds AppleScript support to NSApplication
*****************************************************************************/
@interface NSApplication(ScriptSupport)

@property (readwrite) BOOL scriptFullscreenMode;
@property (readwrite) float audioVolume;
@property (readwrite) long long audioDesync;
@property (readwrite) int currentTime;
@property (readwrite) float playbackRate;
@property (readonly) NSInteger durationOfCurrentItem;
@property (readonly) NSString *pathOfCurrentItem;
@property (readonly) NSString *nameOfCurrentItem;
@property (readonly) BOOL playbackShowsMenu;
@property (readonly) BOOL recordable;
@property (readwrite) BOOL recordingEnabled;
@property (readwrite) BOOL shuffledPlayback;
@property (readwrite) BOOL repeatOne;
@property (readwrite) BOOL repeatAll;

@end
