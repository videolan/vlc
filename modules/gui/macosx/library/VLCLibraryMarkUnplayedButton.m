/*****************************************************************************
 * VLCLibraryMarkUnplayedButton.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#import "VLCLibraryMarkUnplayedButton.h"

#import "extensions/NSString+Helpers.h"
#import "views/VLCUIUnits.h"

@implementation VLCLibraryMarkUnplayedButton

- (NSSize)intrinsicContentSize
{
    const CGFloat size = VLCUIUnits.smallIconButtonSize;
    return NSMakeSize(size, size);
}

- (void)configureForMediaType:(vlc_ml_media_type_t)mediaType
{
    NSString *actionTitle;
    NSString *symbolName;
    switch (mediaType) {
        case VLC_ML_MEDIA_TYPE_AUDIO:
            actionTitle = _NS("Mark as Unheard");
            if (@available(macOS 26.0, *)) {
                symbolName = @"music.note.slash";
            } else {
                symbolName = @"speaker.slash";
            }
            break;
        case VLC_ML_MEDIA_TYPE_VIDEO:
            actionTitle = _NS("Mark as Unseen");
            symbolName = @"eye.slash";
            break;
        default:
            actionTitle = _NS("Mark as Unplayed");
            symbolName = @"arrow.counterclockwise";
            break;
    }

    NSImage *image = nil;
    if (@available(macOS 11.0, *)) {
        image = [NSImage imageWithSystemSymbolName:symbolName accessibilityDescription:actionTitle];
    }
    self.image = image ?: [NSImage imageNamed:NSImageNameRefreshTemplate];
    self.imagePosition = NSImageOnly;
    self.imageScaling = NSImageScaleProportionallyDown;
    self.bordered = YES;
    self.bezelStyle = NSBezelStyleRecessed;
    self.showsBorderOnlyWhileMouseInside = YES;
    self.toolTip = actionTitle;
    self.accessibilityLabel = actionTitle;
}

@end
