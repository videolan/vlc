/*****************************************************************************
 * VLCLibraryTwoPaneSplitViewDelegate.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
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

#import "VLCLibraryTwoPaneSplitViewDelegate.h"

#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"

#import "main/VLCMain.h"

@implementation VLCLibraryTwoPaneSplitViewDelegate

- (CGFloat)splitView:(NSSplitView *)splitView
constrainMaxCoordinate:(CGFloat)proposedMinimumPosition
         ofSubviewAt:(NSInteger)dividerIndex
{
    if (dividerIndex != 0) {
        return proposedMinimumPosition;
    }

    VLCLibraryWindow * const libraryWindow = VLCMain.sharedInstance.libraryWindow;
    const CGFloat libraryWindowWidth = libraryWindow.frame.size.width;

    NSNumber * const leftPaneIndex = [NSNumber numberWithLong:0];
    NSNumber * const leftPaneMaxWidth = [NSNumber numberWithDouble:libraryWindowWidth - [VLCLibraryUIUnits librarySplitViewMainViewMinimumWidth]];

    return leftPaneMaxWidth.floatValue;
}

@end
