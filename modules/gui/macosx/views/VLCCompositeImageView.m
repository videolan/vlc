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

@interface VLCCompositeImageView ()

@property (readonly) NSArray<NSValue *> *imageFrames;

@end

@implementation VLCCompositeImageView

- (NSArray<NSValue *> *)imageFrames
{
    const CGSize size = self.frame.size;
    const CGFloat halfWidth = size.width / 2;
    const CGFloat halfHeight = size.height / 2;

    return @[
        [NSValue valueWithRect:NSMakeRect(0, 0, halfWidth, halfHeight)],
        [NSValue valueWithRect:NSMakeRect(halfWidth, 0, halfWidth, halfHeight)],
        [NSValue valueWithRect:NSMakeRect(0, halfHeight, halfWidth, halfHeight)],
        [NSValue valueWithRect:NSMakeRect(halfWidth, halfHeight, halfWidth, halfHeight)],
    ];
}

- (void)setImages:(NSArray<NSImage *> *)images
{
    _images = images;
    _compositedImage = [NSImage compositeImageWithImages:self.images
                                                  frames:self.imageFrames
                                                    size:self.frame.size];
}

@end
