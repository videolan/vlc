/*****************************************************************************
 * VLCTintedImageButtonCell.m
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
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

#import "VLCTintedImageButtonCell.h"
#import "CompatibilityFixes.h"

@interface VLCTintedImageButtonCell () {
    NSMutableDictionary *_imageCache;
}
@end


@implementation VLCTintedImageButtonCell

+ (void)load
{
    /* On 10.10+ we do not want custom drawing, therefore we swap out the implementation
     * of the selectors below with their original implementations.
     * Just calling super is not enough here, as the button would still draw in a different
     * way, non vibrant with weird highlighting behaviour.
     */
    if (OSX_YOSEMITE_AND_HIGHER) {
        swapoutOverride([VLCTintedImageButtonCell class], @selector(initWithCoder:));
        swapoutOverride([VLCTintedImageButtonCell class], @selector(drawImage:withFrame:inView:));
    }
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        _imageCache = [NSMutableDictionary dictionary];
        _imageTintColor = [NSColor whiteColor];
    }
    return self;
}

- (NSImage *)image:(NSImage*)image tintedWithColor:(NSColor *)tint
{
    image = [image copy];
    [image setTemplate:NO];
    if (tint) {
        [image lockFocus];
        NSRect imageRect = {NSZeroPoint, [image size]};
        [tint setFill];
        NSRectFillUsingOperation(imageRect, NSCompositeSourceAtop);
        [image unlockFocus];
    }
    return image;
}

- (NSImage*)tintedImage:(NSImage*)image
{
    NSNumber *key = @((NSInteger)image);
    if (![_imageCache objectForKey:key]) {
        NSImage *tintedImg = [self image:image tintedWithColor:_imageTintColor];
        [_imageCache setObject:tintedImg forKey:key];
    }
    return [_imageCache objectForKey:key];
}

- (void)drawImage:(NSImage *)image withFrame:(NSRect)frame inView:(NSView *)controlView
{
    image = [self tintedImage:image];
    [super drawImage:image withFrame:frame inView:controlView];
}

@end
