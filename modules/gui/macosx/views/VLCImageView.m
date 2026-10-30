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

#import <QuartzCore/QuartzCore.h>

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSView+VLCAdditions.h"
#import "views/VLCUIUnits.h"

@interface VLCImageView()
{
    NSURL *_currentArtworkURL;
}
@end

@implementation VLCImageView

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        [self setup];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)decoder
{
    self = [super initWithCoder:decoder];
    if (self) {
        [self setup];
    }
    return self;
}

- (void)setup
{
    self.imageFrameStyle = NSImageFrameNone;
    self.editable = NO;
    self.wantsLayer = YES;
    self.cropsImagesToRoundedCorners = YES;
    [self updateBorderColor];
    self.contentGravity = VLCImageViewContentGravityResizeAspectFill;
}

- (void)setCropsImagesToRoundedCorners:(BOOL)cropsImagesToRoundedCorners
{
    self.layer.masksToBounds = cropsImagesToRoundedCorners;
    self.layer.cornerRadius = cropsImagesToRoundedCorners ? VLCUIUnits.cornerRadius : 0.;
    self.layer.borderWidth = cropsImagesToRoundedCorners ? VLCUIUnits.borderThickness : 0.;
}

- (BOOL)cropsImagesToRoundedCorners
{
    return self.layer.masksToBounds;
}

- (void)updateBorderColor
{
    self.layer.borderColor = self.shouldShowDarkAppearance ? NSColor.VLCDarkSubtleBorderColor.CGColor : NSColor.VLCLightSubtleBorderColor.CGColor;
}

- (void)viewDidChangeEffectiveAppearance
{
    [super viewDidChangeEffectiveAppearance];
    [self updateBorderColor];
}

- (void)drawRect:(NSRect)dirtyRect {
    if (self.contentGravity != VLCImageViewContentGravityResizeAspectFill || self.image == nil) {
        [super drawRect:dirtyRect];
        return;
    }

    NSSize imageSize = self.image.size;
    NSSize viewSize = self.bounds.size;
    if (imageSize.width <= 0 || imageSize.height <= 0 ||
        viewSize.width <= 0 || viewSize.height <= 0) return;

    CGFloat imageAspect = imageSize.width / imageSize.height;
    CGFloat viewAspect = viewSize.width / viewSize.height;
    NSRect sourceRect = NSMakeRect(0, 0, imageSize.width, imageSize.height);
    if (imageAspect > viewAspect) {
        CGFloat sourceWidth = imageSize.height * viewAspect;
        sourceRect.origin.x = (imageSize.width - sourceWidth) / 2.;
        sourceRect.size.width = sourceWidth;
    } else if (imageAspect < viewAspect) {
        CGFloat sourceHeight = imageSize.width / viewAspect;
        sourceRect.origin.y = (imageSize.height - sourceHeight) / 2.;
        sourceRect.size.height = sourceHeight;
    }

    [self.image drawInRect:self.bounds
                  fromRect:sourceRect
                 operation:NSCompositingOperationSourceOver
                  fraction:1.
            respectFlipped:YES
                     hints:@{NSImageHintInterpolation: @(NSImageInterpolationHigh)}];
}


- (void)setContentGravity:(VLCImageViewContentGravity)contentGravity
{
    if (_contentGravity == contentGravity)
        return;
    _contentGravity = contentGravity;
    self.needsDisplay = YES;
    self.imageAlignment = NSImageAlignCenter;
    switch (contentGravity) {
        case VLCImageViewContentGravityTop:
            self.imageAlignment = NSImageAlignTop;
            break;
        case VLCImageViewContentGravityBottom:
            self.imageAlignment = NSImageAlignBottom;
            break;
        case VLCImageViewContentGravityLeft:
            self.imageAlignment = NSImageAlignLeft;
            break;
        case VLCImageViewContentGravityRight:
            self.imageAlignment = NSImageAlignRight;
            break;
        case VLCImageViewContentGravityTopLeft:
            self.imageAlignment = NSImageAlignTopLeft;
            break;
        case VLCImageViewContentGravityTopRight:
            self.imageAlignment = NSImageAlignTopRight;
            break;
        case VLCImageViewContentGravityBottomLeft:
            self.imageAlignment = NSImageAlignBottomLeft;
            break;
        case VLCImageViewContentGravityBottomRight:
            self.imageAlignment = NSImageAlignBottomRight;
            break;
        default:
            break;
    }
    switch (contentGravity) {
        case VLCImageViewContentGravityResize:
            self.imageScaling = NSImageScaleAxesIndependently;
            break;
        case VLCImageViewContentGravityResizeAspect:
        case VLCImageViewContentGravityResizeAspectFill:
            self.imageScaling = NSImageScaleProportionallyUpOrDown;
            break;
        default:
            self.imageScaling = NSImageScaleNone;
            break;
    }
}

- (void)setImageURL:(NSURL * _Nonnull)artworkURL placeholderImage:(NSImage * _Nullable)image
{
    if([_currentArtworkURL isEqual:artworkURL]) {
        return;
    }

    _currentArtworkURL = artworkURL;
    self.image = image;

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0), ^{
        NSImage * const downloadedImage = [[NSImage alloc] initWithContentsOfURL:artworkURL];
        dispatch_async(dispatch_get_main_queue(), ^{
            self.image = downloadedImage ?: image;
        });
    });
}

@end
