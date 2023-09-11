/*****************************************************************************
 * VLCCompositeImageView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <mail@claudiocambra.com>
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

#import "VLCCompositeImageView.h"

#import "extensions/NSImage+VLCAdditions.h"

static const NSUInteger kVLCCompositeImageViewDefaultCompositedGridItemCount = 4;

@implementation VLCCompositeImageView

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self setup];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        [self setup];
    }
    return self;
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        [self setup];
    }
    return self;
}

- (void)setup
{
    self.compositedGridItemCount = kVLCCompositeImageViewDefaultCompositedGridItemCount;
}

- (void)setImages:(NSArray<NSImage *> *)images
{
    _images = images;

    const NSSize size = self.frame.size;
    NSArray<NSValue *> * const frames = [NSImage framesForCompositeImageSquareGridWithImages:self.images size:size gridItemCount:self.compositedGridItemCount];
    _compositedImage = [NSImage compositeImageWithImages:self.images frames:frames size:size];
}

@end
