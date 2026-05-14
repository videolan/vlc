/*****************************************************************************
* NSMenuItem+VLCAdditions.m: MacOS X interface module
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

#import "NSMenuItem+VLCAdditions.h"

@implementation NSMenuItem (VLCAdditions)

- (void)vlc_setActionImageWithSystemSymbolName:(NSString *)symbolName
{
    if (@available(macOS 26.0, *)) {
        NSNumber * const enabled = [NSUserDefaults.standardUserDefaults objectForKey:@"NSMenuEnableActionImages"];
        if (enabled != nil && !enabled.boolValue) {
            return;
        }
        self.image = [NSImage imageWithSystemSymbolName:symbolName
                              accessibilityDescription:nil];
    }
}

@end
