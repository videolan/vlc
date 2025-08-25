/*****************************************************************************
* VLCLibraryImageCache.m: MacOS X interface module
*****************************************************************************
* Copyright (C) 2020 VLC authors and VideoLAN
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

NSUInteger kVLCMaximumLibraryImageCacheSize = 50;
uint32_t kVLCDesiredThumbnailWidth = 512;
uint32_t kVLCDesiredThumbnailHeight = 512;
float kVLCDefaultThumbnailPosition = .15;
const NSUInteger kVLCCompositeImageDefaultCompositedGridItemCount = 4;


@interface VLCLibraryImageCache()
{
    NSCache *_imageCache;
    vlc_medialibrary_t *_p_libraryInstance;
}

@end

@implementation VLCLibraryImageCache

- (instancetype)init
{
    self = [super init];
    if (self) {
        _imageCache = [[NSCache alloc] init];
        _imageCache.countLimit = kVLCMaximumLibraryImageCacheSize;
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

+ (NSImage *)thumbnailForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    return [[VLCLibraryImageCache sharedImageCache] imageForLibraryItem:libraryItem];
}

- (NSImage *)imageForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    NSImage *cachedImage = [_imageCache objectForKey:libraryItem.smallArtworkMRL];
    if (cachedImage) {
        return cachedImage;
    }
    return [self smallThumbnailForLibraryItem:libraryItem];
}

- (NSImage *)smallThumbnailForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    NSImage *image;
    NSString * const artworkMRL = libraryItem.smallArtworkMRL;

    if (libraryItem.smallArtworkGenerated) {
        image = [[NSImage alloc] initWithContentsOfURL:[NSURL URLWithString:artworkMRL]];
    } else if ([libraryItem isKindOfClass:[VLCMediaLibraryMediaItem class]]) {
        VLCMediaLibraryMediaItem * const mediaItem = (VLCMediaLibraryMediaItem*)libraryItem;
        
        if (mediaItem.mediaType != VLC_ML_MEDIA_TYPE_AUDIO) {
            [self generateThumbnailForMediaItem:mediaItem.libraryID];
        }
    }

    if (image) {
        [_imageCache setObject:image forKey:artworkMRL];
    } else { // If nothing so far worked, then fall back on default image
        image = [NSImage imageNamed:@"noart.png"];
    }

    return image;
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
        NSImage * image = [[NSImage alloc] initWithContentsOfURL:artworkURL];
        
        dispatch_async(dispatch_get_main_queue(), ^{
            if (image) {
                image.size = imageSize;
                [self->_imageCache setObject:image forKey:inputItem.MRL];
                completionHandler(image);
            } else {
                [inputItem thumbnailWithSize:imageSize completionHandler:^(NSImage * const image) {
                    dispatch_async(dispatch_get_main_queue(), ^{
                        if (image) {
                            [self->_imageCache setObject:image forKey:inputItem.MRL];
                            completionHandler(image);
                        } else {
                            NSLog(@"Failed to generate thumbnail for input item %@", inputItem.MRL);
                            completionHandler([NSImage imageNamed:@"noart.png"]);
                        }
                    });
                }];
            }
        });
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
    if (![libraryItem isKindOfClass:VLCMediaLibraryAlbum.class] &&
        ![libraryItem isKindOfClass:VLCMediaLibraryMediaItem.class]) {

        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
            NSMutableSet<NSImage *> * const itemImages = NSMutableSet.set;

            [libraryItem iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem * const item) {
                NSImage * const itemImage = [VLCLibraryImageCache thumbnailForLibraryItem:item];
                if (itemImage == nil || [itemImages containsObject:itemImage]) {
                    return;
                }
                [itemImages addObject:itemImage];
            }];

            const NSSize size = NSMakeSize(kVLCDesiredThumbnailWidth, kVLCDesiredThumbnailHeight);
            NSArray<NSImage *> * const itemImagesArray = itemImages.allObjects;
            NSArray<NSValue *> * const frames =
                [NSImage framesForCompositeImageSquareGridWithImages:itemImagesArray size:size gridItemCount:kVLCCompositeImageDefaultCompositedGridItemCount];
            NSImage * const compositeImage =
                [NSImage compositeImageWithImages:itemImagesArray frames:frames size:size];

            dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler(compositeImage);
            });
        });
    } else {
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
            NSImage * const image = [VLCLibraryImageCache thumbnailForLibraryItem:libraryItem];
            dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler(image);
            });
        });
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
