/******************************************************************************
 * VLCLibraryCollectionViewSupplementaryElementView.m: MacOS X interface module
 ******************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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
 ******************************************************************************/

#import "VLCLibraryCollectionViewSupplementaryElementView.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSView+VLCAdditions.h"

NSString *VLCLibrarySupplementaryElementViewIdentifier = @"VLCLibrarySupplementaryElementViewIdentifier";

@implementation VLCLibraryCollectionViewSupplementaryElementView

+ (CGSize)defaultHeaderSize
{
    return CGSizeMake(100., 40.);
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        self.font = [NSFont VLClibrarySectionHeaderFont];
        self.textColor = self.shouldShowDarkAppearance ? [NSColor VLClibraryDarkTitleColor] : [NSColor VLClibraryLightTitleColor];
        self.editable = NO;
        self.selectable = NO;
        self.bordered = NO;
    }
    return self;
}

- (void)viewDidChangeEffectiveAppearance
{
    self.textColor = self.shouldShowDarkAppearance ? [NSColor VLClibraryDarkTitleColor] : [NSColor VLClibraryLightTitleColor];
}

@end
