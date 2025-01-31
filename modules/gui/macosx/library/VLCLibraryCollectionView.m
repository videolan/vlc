/*****************************************************************************
 * VLCLibraryCollectionView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#import "VLCLibraryCollectionView.h"

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryUIUnits.h"

NSString * const VLCLibraryCollectionViewItemAdjustmentBigger = @"VLCLibraryCollectionViewItemAdjustmentBigger";
NSString * const VLCLibraryCollectionViewItemAdjustmentSmaller = @"VLCLibraryCollectionViewItemAdjustmentSmaller";

@implementation VLCLibraryCollectionView

- (void)layout
{
    // Manually invalidate the collection view layout when we are going to layout the view; this
    // fixes issues with the collection view item size calculations not always being updated when
    // they should be
    [self.collectionViewLayout invalidateLayout];
    [super layout];
}

- (void)keyDown:(NSEvent *)event
{
    if (![self.collectionViewLayout isKindOfClass:VLCLibraryCollectionViewFlowLayout.class] ||
        !(event.modifierFlags & NSCommandKeyMask)) {
        return;
    }

    const unichar key = [event.charactersIgnoringModifiers.lowercaseString characterAtIndex:0];
    if (key != '+' && key != '-') {
        return;
    }

    NSUserDefaults * const defaults = NSUserDefaults.standardUserDefaults;
    NSInteger collectionViewAdjustment =
        [defaults integerForKey:VLCLibraryCollectionViewItemAdjustmentKey];
    NSNotification *notification;
    if (key == '+') {
        collectionViewAdjustment--;
        notification =
            [NSNotification notificationWithName:VLCLibraryCollectionViewItemAdjustmentBigger
                                          object:self];
    } else if (key == '-') {
        collectionViewAdjustment++;
        notification =
            [NSNotification notificationWithName:VLCLibraryCollectionViewItemAdjustmentSmaller
                                          object:self];
    }
    [defaults setInteger:collectionViewAdjustment forKey:VLCLibraryCollectionViewItemAdjustmentKey];
    [NSNotificationCenter.defaultCenter postNotification:notification];
    [self.collectionViewLayout invalidateLayout];
}

@end
