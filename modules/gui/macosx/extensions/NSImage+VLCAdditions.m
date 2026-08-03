/*****************************************************************************
 * NSImage+VLCAdditions.m: MacOS X interface module
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

#import "NSImage+VLCAdditions.h"

#import "NSString+Helpers.h"

#import <QuickLook/QuickLook.h>
#import <QuickLookThumbnailing/QuickLookThumbnailing.h>
#import <vlc_block.h>
#import <vlc_picture.h>

static NSImage *ImageFromEmoji(NSString *emoji, NSSize size)
{
    const NSInteger pixelsWide = MAX((NSInteger)ceil(size.width), 1);
    const NSInteger pixelsHigh = MAX((NSInteger)ceil(size.height), 1);
    NSBitmapImageRep * const bitmapImageRep =
        [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                               pixelsWide:pixelsWide
                                               pixelsHigh:pixelsHigh
                                            bitsPerSample:8
                                          samplesPerPixel:4
                                                 hasAlpha:YES
                                                 isPlanar:NO
                                           colorSpaceName:NSCalibratedRGBColorSpace
                                              bytesPerRow:0
                                             bitsPerPixel:0];
    if (!bitmapImageRep) {
        return nil;
    }
    bitmapImageRep.size = size;

    NSGraphicsContext * const context =
        [NSGraphicsContext graphicsContextWithBitmapImageRep:bitmapImageRep];
    if (!context) {
        return nil;
    }

    NSGraphicsContext * const previousContext = NSGraphicsContext.currentContext;
    NSGraphicsContext.currentContext = context;
    [context saveGraphicsState];
    [NSColor.whiteColor setFill];
    NSRectFill(NSMakeRect(0., 0., size.width, size.height));

    const CGFloat initialFontSize = MIN(size.width, size.height);
    NSDictionary *attributes = @{ NSFontAttributeName : [NSFont systemFontOfSize:initialFontSize] };
    NSSize emojiSize = [emoji sizeWithAttributes:attributes];
    if (emojiSize.width > size.width || emojiSize.height > size.height) {
        const CGFloat scale = MIN(size.width / emojiSize.width, size.height / emojiSize.height);
        const CGFloat fontSize = floor(initialFontSize * scale);
        attributes = @{ NSFontAttributeName : [NSFont systemFontOfSize:MAX(fontSize, 1.)] };
        emojiSize = [emoji sizeWithAttributes:attributes];
    }

    const NSPoint point = NSMakePoint((size.width - emojiSize.width) / 2.,
                                     (size.height - emojiSize.height) / 2.);
    [emoji drawAtPoint:point withAttributes:attributes];
    [context flushGraphics];
    [context restoreGraphicsState];
    NSGraphicsContext.currentContext = previousContext;

    NSImage * const image = [[NSImage alloc] initWithSize:size];
    [image addRepresentation:bitmapImageRep];
    return image;
}

@implementation NSImage(VLCAdditions)

+ (NSImage *)VLCAppIconImage
{
    return [NSImage imageNamed:@"VLC"];
}

+ (NSImage *)VLCXmasAppIconImage
{
    return [NSImage imageNamed:@"VLC-Xmas"];
}

+ (NSImage *)VLCStatusBarIconImage
{
    return [NSImage imageNamed:@"VLCStatusBarIcon"];
}

+ (NSImage *)VLCSidebarMovieImage
{
    return [NSImage imageNamed:@"sidebar-movie"];
}

+ (NSImage *)VLCSidebarMusicImage
{
    return [NSImage imageNamed:@"sidebar-music"];
}

+ (NSImage *)VLCBWHomeImage
{
    return [NSImage imageNamed:@"bw-home"];
}

+ (NSImage *)VLCBWMusicImage
{
    return [NSImage imageNamed:@"bw-Music"];
}

+ (NSImage *)VLCBWServer1Image
{
    return [NSImage imageNamed:@"bw-Server1"];
}

+ (NSImage *)VLCBWServer2Image
{
    return [NSImage imageNamed:@"bw-server2"];
}

+ (NSImage *)VLCBWUsb1Image
{
    return [NSImage imageNamed:@"bw-usb1"];
}

+ (NSImage *)VLCBWUsb2Image
{
    return [NSImage imageNamed:@"bw-usb2"];
}

+ (NSImage *)VLCDefaultAppIconImage
{
    return [NSImage imageNamed:@"NXdefaultappicon"];
}

+ (NSImage *)VLCFollowImage
{
    return [NSImage imageNamed:@"NXFollow"];
}

+ (NSImage *)VLCNoArtImage
{
    return [NSImage imageNamed:@"noart.png"];
}

+ (NSImage *)VLCPlaceholderVideoImage
{
    return [NSImage imageNamed:@"placeholder-video"];
}

+ (NSImage *)VLCPlaceholderGroupImage
{
    return [NSImage imageNamed:@"placeholder-group2"];
}

+ (NSImage *)VLCPlaceholderMusicImage
{
    return [NSImage imageNamed:@"placeholder-music"];
}

+ (NSImage *)VLCGenericImage
{
    return [NSImage imageNamed:@"generic"];
}

+ (NSImage *)VLCPlayTemplateImage
{
    return [NSImage imageNamed:@"VLCPlayTemplate"];
}

+ (NSImage *)VLCPauseTemplateImage
{
    return [NSImage imageNamed:@"VLCPauseTemplate"];
}

+ (NSImage *)VLCBackwardTemplateImage
{
    return [NSImage imageNamed:@"VLCBackwardTemplate"];
}

+ (NSImage *)VLCForwardTemplateImage
{
    return [NSImage imageNamed:@"VLCForwardTemplate"];
}

+ (NSImage *)VLCFullscreenOffTemplateImage
{
    return [NSImage imageNamed:@"VLCFullscreenOffTemplate"];
}

+ (NSImage *)VLCVolumeOnTemplateImage
{
    return [NSImage imageNamed:@"VLCVolumeOnTemplate"];
}

+ (NSImage *)VLCVolumeOffTemplateImage
{
    return [NSImage imageNamed:@"VLCVolumeOffTemplate"];
}

+ (NSImage *)VLCStopImage
{
    return [NSImage imageNamed:@"stop"];
}

+ (NSImage *)VLCStopPressedImage
{
    return [NSImage imageNamed:@"stop-pressed"];
}

+ (NSImage *)VLCShuffleOffImage
{
    return [NSImage imageNamed:@"shuffleOff"];
}

+ (NSImage *)VLCShuffleOnImage
{
    return [NSImage imageNamed:@"shuffleOn"];
}

+ (NSImage *)VLCRepeatAllImage
{
    return [NSImage imageNamed:@"repeatAll"];
}

+ (NSImage *)VLCRepeatOneImage
{
    return [NSImage imageNamed:@"repeatOne"];
}

+ (NSImage *)VLCRepeatOffImage
{
    return [NSImage imageNamed:@"repeatOff"];
}

+ (nullable NSImage *)flagImageForCountryCode:(NSString *)countryCode
                                         size:(NSSize)size
{
    if (size.width <= 0 || size.height <= 0) {
        return nil;
    }

    NSString * const uppercaseCountryCode = countryCode.uppercaseString;
    NSString * const emoji = flagEmojiStringForCountryCode(uppercaseCountryCode);
    if (emoji == nil) {
        return nil;
    }

    return ImageFromEmoji(emoji, size);
}

+ (void)quickLookPreviewForLocalPath:(NSString *)path
                            withSize:(NSSize)size
                   completionHandler:(void (^)(NSImage *))completionHandler
{
    NSURL * const pathUrl = [NSURL fileURLWithPath:path];
    [self quickLookPreviewForLocalURL:pathUrl withSize:size completionHandler:completionHandler];
}

+ (void)quickLookPreviewForLocalURL:(NSURL *)url 
                           withSize:(NSSize)size 
                  completionHandler:(void (^)(NSImage *))completionHandler
{
    if (@available(macOS 10.15, *)) {
        const QLThumbnailGenerationRequestRepresentationTypes type = 
            QLThumbnailGenerationRequestRepresentationTypeAll;
        QLThumbnailGenerator * const generator = QLThumbnailGenerator.sharedGenerator;
        QLThumbnailGenerationRequest * const request = 
            [[QLThumbnailGenerationRequest alloc] initWithFileAtURL:url 
                                                               size:size 
                                                              scale:1. 
                                                representationTypes:type];
        [generator generateBestRepresentationForRequest:request 
                                      completionHandler:^(QLThumbnailRepresentation * const thumbnail, 
                                                          NSError * const error) {
            if (error != nil) {
                NSLog(@"Error generating thumbnail: %@", error);
                completionHandler(nil);
                return;
            }
            completionHandler(thumbnail.NSImage);
        }];
    } else {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
            NSImage * const image = [self quickLookPreviewForLocalURL:url withSize:size];
            dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler(image);
            });
        });
    }
}

+ (instancetype)quickLookPreviewForLocalPath:(NSString *)path withSize:(NSSize)size
{
    NSURL *pathUrl = [NSURL fileURLWithPath:path];
    return [self quickLookPreviewForLocalURL:pathUrl withSize:size];
}

+ (instancetype)quickLookPreviewForLocalURL:(NSURL *)url withSize:(NSSize)size
{
    NSDictionary *dict = @{(NSString*)kQLThumbnailOptionIconModeKey : [NSNumber numberWithBool:NO]};
    CFDictionaryRef dictRef = CFBridgingRetain(dict);
    if (dictRef == NULL) {
        NSLog(@"Got null dict for quickLook preview");
        return nil;
    }

    CFURLRef urlRef = CFBridgingRetain(url);
    if (urlRef == NULL) {
        NSLog(@"Got null url ref for quickLook preview");
        CFRelease(dictRef);
        return nil;
    }

    CGImageRef qlThumbnailRef = QLThumbnailImageCreate(kCFAllocatorDefault,
                                                       urlRef,
                                                       size,
                                                       dictRef);

    CFRelease(dictRef);
    CFRelease(urlRef);

    if (qlThumbnailRef == NULL) {
        return nil;
    }

    NSBitmapImageRep *bitmapImageRep = [[NSBitmapImageRep alloc] initWithCGImage:qlThumbnailRef];
    if (bitmapImageRep == nil) {
        CFRelease(qlThumbnailRef);
        return nil;
    }

    NSImage *image = [[NSImage alloc] initWithSize:[bitmapImageRep size]];
    [image addRepresentation:bitmapImageRep];
    CFRelease(qlThumbnailRef);
    return image;
}

+ (NSImage *)imageFromVLCPicture:(picture_t *)picture
                       vlcObject:(vlc_object_t *)object
                            size:(NSSize)size
{
    if (picture == NULL || object == NULL) {
        return nil;
    }

    block_t *block = NULL;
    video_format_t format;
    const int exportStatus = picture_Export(object,
                                            &block,
                                            &format,
                                            picture,
                                            VLC_CODEC_ARGB,
                                            (int)size.width,
                                            (int)size.height,
                                            true);
    if (exportStatus != VLC_SUCCESS || block == NULL) {
        return nil;
    }

    const NSUInteger bytesPerRow = (NSUInteger)format.i_width * 4;
    const NSUInteger expectedSize = bytesPerRow * (NSUInteger)format.i_height;
    if (block->i_buffer < expectedSize) {
        block_Release(block);
        return nil;
    }

    NSBitmapImageRep * const bitmap = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL
                      pixelsWide:format.i_width
                      pixelsHigh:format.i_height
                   bitsPerSample:8
                 samplesPerPixel:4
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSDeviceRGBColorSpace
                    bitmapFormat:NSBitmapFormatThirtyTwoBitLittleEndian
                     bytesPerRow:bytesPerRow
                    bitsPerPixel:32];
    if (bitmap == nil) {
        block_Release(block);
        return nil;
    }

    memcpy(bitmap.bitmapData, block->p_buffer, expectedSize);
    block_Release(block);

    NSImage * const image = [[NSImage alloc]
        initWithSize:NSMakeSize(format.i_width, format.i_height)];
    [image addRepresentation:bitmap];
    return image;
}

+ (instancetype)compositeImageWithImages:(NSArray<NSImage *> * const)images
                                  frames:(NSArray<NSValue *> * const)frames
                                    size:(const NSSize)size
{
    return [NSImage imageWithSize:size
                          flipped:NO
                   drawingHandler:^BOOL(const NSRect __unused dstRect) {

        NSUInteger counter = 0;
        for (NSValue * const rectValue in frames) {
            if (counter >= images.count) {
                break;
            }

            NSImage * const image = [images objectAtIndex:counter];
            const NSRect imageRect = rectValue.rectValue;
            [image drawInRect:imageRect
                     fromRect:NSZeroRect
                    operation:NSCompositingOperationOverlay
                     fraction:1.];

            counter += 1;
        }

        return YES;
    }];
}

- (instancetype)imageTintedWithColor:(NSColor *)color
{
    NSImage * const image = [self copy];

    if (color != nil) {
        [image lockFocus];
        [color set];
        const NSRect imageRect = {NSZeroPoint, image.size};
        NSRectFillUsingOperation(imageRect, NSCompositingOperationSourceIn);
        [image unlockFocus];
    }

    return image;
}

+ (NSArray<NSValue *> *)framesForCompositeImageSquareGridWithImages:(NSArray<NSImage *> * const)images
                                                               size:(const NSSize)size
                                                      gridItemCount:(const NSUInteger)gridItemCount
{
    const float sqrtAxisItemCount = ceil(sqrt(gridItemCount));
    const float roundAxisItemCount = roundf(sqrtAxisItemCount);

    // Default to just one item if there are not enough images
    const NSUInteger actualGridItemCount = images.count >= gridItemCount ? gridItemCount : 1;

    // Default to just one item if there are not enough images
    const NSUInteger gridDivisor = actualGridItemCount > 1 ? roundAxisItemCount : 1;
    const CGFloat itemWidth = size.width / gridDivisor;
    const CGFloat itemHeight = size.height / gridDivisor;

    NSMutableArray<NSValue *> * const rects = NSMutableArray.array;

    for (NSUInteger i = 0; i < actualGridItemCount; ++i) {
        const CGFloat xPos = (i % gridDivisor) * itemWidth;
        const CGFloat yPos = floor(i / gridDivisor) * itemHeight;
        const NSRect rect = NSMakeRect(xPos, yPos, itemWidth, itemHeight);
        NSValue * const rectVal = [NSValue valueWithRect:rect];
        [rects addObject:rectVal];
    }

    return rects.copy;
}

@end
