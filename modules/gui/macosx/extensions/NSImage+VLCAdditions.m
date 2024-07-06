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

#import <QuickLook/QuickLook.h>
#import <QuickLookThumbnailing/QuickLookThumbnailing.h>

@implementation NSImage(VLCAdditions)

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

+ (instancetype)compositeImageWithImages:(NSArray<NSImage *> * const)images
                                  frames:(NSArray<NSValue *> * const)frames
                                    size:(const NSSize)size
{
    return [NSImage imageWithSize:size
                          flipped:NO
                   drawingHandler:^BOOL(const NSRect dstRect) {

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
        NSRectFillUsingOperation(imageRect, NSCompositeSourceIn);
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
