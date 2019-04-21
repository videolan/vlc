/*****************************************************************************
 * VLCImageView.m: MacOS X interface module
 *****************************************************************************
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
 *****************************************************************************/

#import "VLCImageView.h"

@implementation VLCImageView

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        [self setupLayer];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder
{
    self = [super initWithCoder:decoder];
    if (self) {
        [self setupLayer];
    }
    return self;
}

- (void)setupLayer
{
    self.layer = [[CALayer alloc] init];
    self.contentGravity = VLCImageViewContentGravityResizeAspectFill;
    self.wantsLayer = YES;
}

- (void)setImage:(NSImage *)image
{
    _image = image;
    CGFloat desiredScaleFactor = [self.window backingScaleFactor];
    CGFloat actualScaleFactor = [image recommendedLayerContentsScale:desiredScaleFactor];

    id layerContents = [image layerContentsForContentsScale:actualScaleFactor];

    [self setCAContentGravity:_contentGravity];
    [self.layer setContents:layerContents];
    [self.layer setContentsScale:actualScaleFactor];
}

- (void)setCAContentGravity:(VLCImageViewContentGravity)contentGravity
{
    switch (contentGravity) {
        case VLCImageViewContentGravityCenter:
            self.layer.contentsGravity = kCAGravityCenter;
            break;
        case VLCImageViewContentGravityTop:
            self.layer.contentsGravity = kCAGravityTop;
            break;
        case VLCImageViewContentGravityBottom:
            self.layer.contentsGravity = kCAGravityBottom;
            break;
        case VLCImageViewContentGravityLeft:
            self.layer.contentsGravity = kCAGravityLeft;
            break;
        case VLCImageViewContentGravityRight:
            self.layer.contentsGravity = kCAGravityRight;
            break;
        case VLCImageViewContentGravityTopLeft:
            self.layer.contentsGravity = kCAGravityTopLeft;
            break;
        case VLCImageViewContentGravityTopRight:
            self.layer.contentsGravity = kCAGravityTopRight;
            break;
        case VLCImageViewContentGravityBottomLeft:
            self.layer.contentsGravity = kCAGravityBottomLeft;
            break;
        case VLCImageViewContentGravityBottomRight:
            self.layer.contentsGravity = kCAGravityBottomRight;
            break;
        case VLCImageViewContentGravityResize:
            self.layer.contentsGravity = kCAGravityResize;
            break;
        case VLCImageViewContentGravityResizeAspect:
            self.layer.contentsGravity = kCAGravityResizeAspect;
            break;
        case VLCImageViewContentGravityResizeAspectFill:
        default:
            self.layer.contentsGravity = kCAGravityResizeAspectFill;
            break;
    }
}

@end
