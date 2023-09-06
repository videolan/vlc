/*****************************************************************************
 * VLCLibraryHeroView.m: MacOS X interface module
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

#import "VLCLibraryHeroView.h"

#import "extensions/NSView+VLCAdditions.h"

#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryImageCache.h"

@implementation VLCLibraryHeroView

+ (instancetype)fromNibWithOwner:(id)owner
{
    return (VLCLibraryHeroView*)[NSView fromNibNamed:@"VLCLibraryHeroView"
                                                     withClass:VLCLibraryHeroView.class
                                                     withOwner:owner];
}

- (void)updateRepresentedItem
{
    NSAssert(self.representedItem != nil, @"Should not update nil represented item!");
    self.largeImageView.image = [VLCLibraryImageCache thumbnailForLibraryItem:self.representedItem];
    self.titleTextField.stringValue = self.representedItem.displayString;
    self.detailTextField.stringValue = self.representedItem.detailString;
}

- (void)setRepresentedItem:(id<VLCMediaLibraryItemProtocol>)representedItem
{
    NSParameterAssert(representedItem != nil);
    if (representedItem == self.representedItem) {
        return;
    }

    _representedItem = representedItem;
    [self updateRepresentedItem];
}

@end
