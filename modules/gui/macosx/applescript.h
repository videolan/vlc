/*****************************************************************************
 * applescript.h: MacOS X AppleScript support
 *****************************************************************************
 * Copyright (C) 2002-2012 VLC authors and VideoLAN
 * $Id$
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
@property (readwrite) int audioVolume;
@property (readwrite) int currentTime;
@property (readonly) int durationOfCurrentItem;
@property (readonly) NSString *pathOfCurrentItem;
@property (readonly) NSString *nameOfCurrentItem;

@end
