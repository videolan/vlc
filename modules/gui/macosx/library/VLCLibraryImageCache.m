/*****************************************************************************
* VLCLibraryImageCache.m: MacOS X interface module
*****************************************************************************
* Copyright (C) 2020-2026 VLC authors and VideoLAN
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

#import "VLCLibraryImageCache.h"

#import "extensions/NSImage+VLCAdditions.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryDataTypes.h"

#import "main/VLCMain.h"

#import "playqueue/VLCPlayQueueItem.h"

#import <ImageIO/ImageIO.h>

NSUInteger kVLCMaximumLibraryImageCacheSize = 500;
/* 256 MB cost limit based on estimated pixel data size per image */
static const NSUInteger kVLCLibraryImageCacheCostLimit = 256 * 1024 * 1024;
uint32_t kVLCDesiredThumbnailWidth = 512;
uint32_t kVLCDesiredThumbnailHeight = 512;
float kVLCDefaultThumbnailPosition = .15;
const NSUInteger kVLCCompositeImageDefaultCompositedGridItemCount = 4;


@interface VLCLibraryImageCache()
{
    NSCache *_imageCache;
    NSImage *_noArtImage;
    vlc_medialibrary_t *_p_libraryInstance;
}

@end

@implementation VLCLibraryImageCache

+ (NSImage *)downsampledImageFromURL:(NSURL *)url maxPixelSize:(uint32_t)maxPixelSize
{
    CGImageSourceRef const imageSource = CGImageSourceCreateWithURL((__bridge CFURLRef)url, NULL);
    if (!imageSource) {
        return nil;
    }

    NSDictionary * const downsampleOptions = @{
        (NSString *)kCGImageSourceCreateThumbnailFromImageAlways : @YES,
        (NSString *)kCGImageSourceCreateThumbnailWithTransform : @YES,
        (NSString *)kCGImageSourceThumbnailMaxPixelSize : @(maxPixelSize),
        (NSString *)kCGImageSourceShouldCacheImmediately : @YES,
    };

    CGImageRef const downsampledImage =
        CGImageSourceCreateThumbnailAtIndex(imageSource, 0, (__bridge CFDictionaryRef)downsampleOptions);
    CFRelease(imageSource);

    if (!downsampledImage) {
        return nil;
    }

    NSImage * const image = [[NSImage alloc] initWithCGImage:downsampledImage
                                                        size:NSZeroSize];
    CGImageRelease(downsampledImage);
    return image;
}

+ (NSUInteger)costForImage:(NSImage *)image
{
    NSBitmapImageRep *bitmapRep = nil;
    for (NSImageRep *rep in image.representations) {
        if ([rep isKindOfClass:[NSBitmapImageRep class]]) {
            bitmapRep = (NSBitmapImageRep *)rep;
            break;
        }
    }
    if (bitmapRep) {
        return bitmapRep.pixelsWide * bitmapRep.pixelsHigh * bitmapRep.bitsPerPixel / 8;
    }
    return (NSUInteger)(image.size.width * image.size.height * 4);
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        _imageCache = [[NSCache alloc] init];
        _imageCache.countLimit = kVLCMaximumLibraryImageCacheSize;
        _imageCache.totalCostLimit = kVLCLibraryImageCacheCostLimit;
        _noArtImage = [NSImage imageNamed:@"noart.png"];
    }
    return self;
}

+ (instancetype)sharedImageCache
{
    static dispatch_once_t onceToken;
    static VLCLibraryImageCache *sharedImageCache;
    dispatch_once(&onceToken, ^{
        sharedImageCache = [[VLCLibraryImageCache alloc] init];
    });
    return sharedImageCache;
}

- (void)imageForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
             withCompletion:(void(^)(const NSImage *))completionHandler
{
    NSString * const artworkMRL = libraryItem.smallArtworkMRL;
    NSImage * const cachedImage = [_imageCache objectForKey:artworkMRL];
    if (cachedImage) {
        completionHandler(cachedImage);
        return;
    }

    if (libraryItem.smallArtworkGenerated) {
        NSURL * const artworkURL = [NSURL URLWithString:artworkMRL];
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
            NSImage * const image =
                [VLCLibraryImageCache downsampledImageFromURL:artworkURL
                                                maxPixelSize:kVLCDesiredThumbnailWidth];
            if (image) {
                const NSUInteger cost = [VLCLibraryImageCache costForImage:image];
                [self->_imageCache setObject:image forKey:artworkMRL cost:cost];
            }
            dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler(image ?: self->_noArtImage);
            });
        });
    } else if ([libraryItem isKindOfClass:[VLCMediaLibraryMediaItem class]]) {
        VLCMediaLibraryMediaItem * const mediaItem = (VLCMediaLibraryMediaItem *)libraryItem;
        if (mediaItem.mediaType != VLC_ML_MEDIA_TYPE_AUDIO) {
            [self generateThumbnailForMediaItem:mediaItem.libraryID];
        }
        completionHandler(_noArtImage);
    } else {
        completionHandler(_noArtImage);
    }
}

- (void)generateThumbnailForMediaItem:(int64_t)mediaID
{
    if (!_p_libraryInstance) {
        _p_libraryInstance = vlc_ml_instance_get(getIntf());
    }
    vlc_ml_media_generate_thumbnail(_p_libraryInstance,
                                    mediaID,
                                    VLC_ML_THUMBNAIL_SMALL,
                                    kVLCDesiredThumbnailWidth,
                                    kVLCDesiredThumbnailHeight,
                                    kVLCDefaultThumbnailPosition);
}

+ (void)thumbnailForInputItem:(VLCInputItem *)inputItem 
               withCompletion:(nonnull void (^)(const NSImage * _Nonnull))completionHandler
{
    [VLCLibraryImageCache.sharedImageCache imageForInputItem:inputItem withCompletion:completionHandler];
}

- (void)imageForInputItem:(VLCInputItem *)inputItem 
           withCompletion:(nonnull void (^)(const NSImage * _Nonnull))completionHandler
{
    NSImage * const cachedImage = [_imageCache objectForKey:inputItem.MRL];
    if (cachedImage) {
        completionHandler(cachedImage);
        return;
    }
    [self generateImageForInputItem:inputItem withCompletion:completionHandler];
}

- (void)generateImageForInputItem:(VLCInputItem *)inputItem
                   withCompletion:(void(^)(const NSImage *))completionHandler
{
    NSURL * const artworkURL = inputItem.artworkURL;
    const NSSize imageSize = NSMakeSize(kVLCDesiredThumbnailWidth, kVLCDesiredThumbnailHeight);

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSImage * const image = artworkURL ?
            [VLCLibraryImageCache downsampledImageFromURL:artworkURL
                                            maxPixelSize:kVLCDesiredThumbnailWidth] : nil;

        if (image) {
            const NSUInteger cost = [VLCLibraryImageCache costForImage:image];
            [self->_imageCache setObject:image forKey:inputItem.MRL cost:cost];
            dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler(image);
            });
            return;
        }

        [inputItem thumbnailWithSize:imageSize completionHandler:^(NSImage * const thumbnail) {
            if (thumbnail) {
                const NSUInteger cost = [VLCLibraryImageCache costForImage:thumbnail];
                [self->_imageCache setObject:thumbnail forKey:inputItem.MRL cost:cost];
            } else {
                NSLog(@"Failed to generate thumbnail for input item %@", inputItem.MRL);
            }
            dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler(thumbnail ?: self->_noArtImage);
            });
        }];
    });
}

+ (void)thumbnailForPlayQueueItem:(VLCPlayQueueItem *)playQueueItem
                  withCompletion:(nonnull void (^)(const NSImage * _Nonnull))completionHandler
{
    return [VLCLibraryImageCache.sharedImageCache imageForInputItem:playQueueItem.inputItem
                                                     withCompletion:completionHandler];
}

+ (void)thumbnailForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
                 withCompletion:(void(^)(const NSImage *))completionHandler
{
    VLCLibraryImageCache * const cache = [VLCLibraryImageCache sharedImageCache];

    if (![libraryItem isKindOfClass:VLCMediaLibraryAlbum.class] &&
        ![libraryItem isKindOfClass:VLCMediaLibraryMediaItem.class]) {

        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
            NSMutableArray<NSImage *> * const itemImages = [NSMutableArray array];
            dispatch_group_t const group = dispatch_group_create();

            [libraryItem iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem * const item) {
                dispatch_group_enter(group);
                [cache imageForLibraryItem:item withCompletion:^(const NSImage * thumbnail) {
                    NSImage * const mutableRef = (NSImage *)thumbnail;
                    @synchronized (itemImages) {
                        if (mutableRef && ![mutableRef isEqual:cache->_noArtImage]) {
                            [itemImages addObject:mutableRef];
                        }
                    }
                    dispatch_group_leave(group);
                }];
            }];

            dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

            NSArray<NSImage *> *uniqueImages;
            @synchronized (itemImages) {
                uniqueImages = [NSOrderedSet orderedSetWithArray:itemImages].array;
            }

            const NSSize size = NSMakeSize(kVLCDesiredThumbnailWidth, kVLCDesiredThumbnailHeight);
            NSArray<NSValue *> * const frames =
                [NSImage framesForCompositeImageSquareGridWithImages:uniqueImages size:size gridItemCount:kVLCCompositeImageDefaultCompositedGridItemCount];
            NSImage * const compositeImage =
                [NSImage compositeImageWithImages:uniqueImages frames:frames size:size];

            dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler(compositeImage ?: cache->_noArtImage);
            });
        });
    } else {
        [cache imageForLibraryItem:libraryItem withCompletion:completionHandler];
    }
}

+ (NSImage *)thumbnailAtMrl:(NSString *)smallArtworkMRL
{
    NSImage * const cachedImage = 
        [VLCLibraryImageCache.sharedImageCache->_imageCache objectForKey:smallArtworkMRL];
    return cachedImage ?
        cachedImage : [[NSImage alloc] initWithContentsOfURL:[NSURL URLWithString:smallArtworkMRL]];
}

@end
