/*****************************************************************************
* NSMenuItem+VLCAdditions.h: MacOS X interface module
*****************************************************************************
* Copyright (C) 2026 VLC authors and VideoLAN
*
* Author: Serhii Bykov <esphynox@gmail.com>
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

NS_ASSUME_NONNULL_BEGIN

@interface NSMenuItem (VLCAdditions)

/* Sets the receiver's image to the given SF Symbol to match the macOS 26
 * system behaviour. This is a no-op before macOS 26, and when the user opted
 * out via `defaults write -g NSMenuEnableActionImages -bool NO`, leaving the
 * menu item without an image. */
- (void)vlc_setActionImageWithSystemSymbolName:(NSString *)symbolName;

@end

NS_ASSUME_NONNULL_END
