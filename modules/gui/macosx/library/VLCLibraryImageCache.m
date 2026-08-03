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
#import "library/VLCLibraryModel.h"
#import "library/VLCThumbnailRequest.h"

#import "main/VLCMain.h"

#import "playqueue/VLCPlayQueueItem.h"

#import <ImageIO/ImageIO.h>

#import <vlc_preparser.h>
#import <vlc_picture.h>

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
    NSMutableDictionary<NSString *, VLCThumbnailRequest *> *_pendingThumbnailRequests;
}

- (void)generateVLCThumbnailForInputItem:(VLCInputItem *)inputItem
                                cacheKey:(NSString *)cacheKey
                              completion:(VLCThumbnailCompletion)completion;
- (void)thumbnailRequest:(VLCThumbnailRequest *)request
     didFinishWithStatus:(int)status
               thumbnail:(picture_t *)thumbnail;

@end

static void vlcThumbnailerOnEnded(vlc_preparser_req *req, int status,
                                  picture_t *thumbnail, void *data)
{
    VLCThumbnailRequest * const request = (__bridge VLCThumbnailRequest *)data;
    [request.imageCache thumbnailRequest:request didFinishWithStatus:status thumbnail:thumbnail];
    vlc_preparser_req_Release(req);
}

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
        _noArtImage = NSImage.VLCNoArtImage;
        _pendingThumbnailRequests = NSMutableDictionary.dictionary;

        NSNotificationCenter * const notificationCenter = [NSNotificationCenter defaultCenter];
        [notificationCenter addObserver:self
                               selector:@selector(mediaItemThumbnailGenerated:)
                                   name:VLCLibraryModelMediaItemThumbnailGenerated
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(mediaItemUpdated:)
                                   name:VLCLibraryModelAudioMediaItemUpdated
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(mediaItemUpdated:)
                                   name:VLCLibraryModelVideoMediaItemUpdated
                                 object:nil];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
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

- (void)mediaItemThumbnailGenerated:(NSNotification *)aNotification
{
    VLCMediaLibraryMediaItem * const mediaItem = aNotification.object;
    NSString * const artworkMRL = mediaItem.smallArtworkMRL;
    if (mediaItem == nil || artworkMRL == nil) {
        return;
    }
    [_imageCache removeObjectForKey:artworkMRL];
}

- (void)mediaItemUpdated:(NSNotification *)aNotification
{
    VLCMediaLibraryMediaItem * const mediaItem = aNotification.object;
    NSString * const artworkMRL = mediaItem.smallArtworkMRL;
    if (mediaItem == nil || artworkMRL == nil) {
        return;
    }
    [_imageCache removeObjectForKey:artworkMRL];
}

- (void)imageForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
             withCompletion:(void(^)(const NSImage *))completionHandler
{
    NSString * const artworkMRL = libraryItem.smallArtworkMRL;
    if (artworkMRL) {
        NSImage * const cachedImage = [_imageCache objectForKey:artworkMRL];
        if (cachedImage) {
            completionHandler(cachedImage);
            return;
        }
    }

    if (libraryItem.smallArtworkGenerated && artworkMRL) {
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

- (void)imageForCountryCode:(NSString *)countryCode
             withCompletion:(void(^)(const NSImage * _Nullable))completionHandler
{
    NSString * const normalizedCountryCode = countryCode.uppercaseString;
    NSString * const cacheKey = [@"flag://" stringByAppendingString:normalizedCountryCode];
    NSImage * const cachedImage = [_imageCache objectForKey:cacheKey];
    if (cachedImage) {
        completionHandler(cachedImage);
        return;
    }

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        const NSSize imageSize =
            NSMakeSize(kVLCDesiredThumbnailWidth, kVLCDesiredThumbnailHeight);
        NSImage * const image = [NSImage flagImageForCountryCode:normalizedCountryCode
                                                           size:imageSize];
        if (image) {
            const NSUInteger cost = [VLCLibraryImageCache costForImage:image];
            [self->_imageCache setObject:image forKey:cacheKey cost:cost];
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            completionHandler(image);
        });
    });
}

- (void)imageForInputItem:(VLCInputItem *)inputItem 
           withCompletion:(nonnull void (^)(const NSImage * _Nonnull))completionHandler
{
    NSString * const memoryCacheKey = inputItem.MRL;
    NSImage * const cachedImage = [_imageCache objectForKey:memoryCacheKey];
    if (cachedImage) {
        completionHandler(cachedImage);
        return;
    }
    [self generateImageForInputItem:inputItem withCompletion:completionHandler];
}

- (void)generateArtworkForInputItem:(VLCInputItem *)inputItem
                     withCompletion:(void(^)(const NSImage *))completionHandler
{
    NSURL * const artworkURL = inputItem.artworkURL;
    const NSSize imageSize = NSMakeSize(kVLCDesiredThumbnailWidth, kVLCDesiredThumbnailHeight);
    NSString * const memoryCacheKey = inputItem.MRL;

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSImage * const image = artworkURL ?
            [VLCLibraryImageCache downsampledImageFromURL:artworkURL
                                            maxPixelSize:kVLCDesiredThumbnailWidth] : nil;

        if (image) {
            const NSUInteger cost = [VLCLibraryImageCache costForImage:image];
            [self->_imageCache setObject:image forKey:memoryCacheKey cost:cost];
            dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler(image);
            });
            return;
        }

        if (inputItem.inputType == ITEM_TYPE_FILE &&
            !inputItem.isStream &&
            input_item_Playable(inputItem.path.UTF8String)) {
            [self generateVLCThumbnailForInputItem:inputItem
                                          cacheKey:inputItem.MRL
                                        completion:^(NSImage * const thumbnail) {
                if (thumbnail != nil) {
                    const NSUInteger cost = [VLCLibraryImageCache costForImage:thumbnail];
                    [self->_imageCache setObject:thumbnail forKey:memoryCacheKey cost:cost];
                    dispatch_async(dispatch_get_main_queue(), ^{
                        completionHandler(thumbnail);
                    });
                    return;
                }

                [inputItem thumbnailWithSize:imageSize completionHandler:^(NSImage * const quickLookThumbnail) {
                    if (quickLookThumbnail) {
                        const NSUInteger cost = [VLCLibraryImageCache costForImage:quickLookThumbnail];
                        [self->_imageCache setObject:quickLookThumbnail forKey:memoryCacheKey cost:cost];
                    } else {
                        NSLog(@"Failed to generate thumbnail for input item %@", inputItem.MRL);
                    }
                    dispatch_async(dispatch_get_main_queue(), ^{
                        completionHandler(quickLookThumbnail ?: self->_noArtImage);
                    });
                }];
            }];
            return;
        }

        [inputItem thumbnailWithSize:imageSize completionHandler:^(NSImage * const quickLookThumbnail) {
            if (quickLookThumbnail) {
                const NSUInteger cost = [VLCLibraryImageCache costForImage:quickLookThumbnail];
                [self->_imageCache setObject:quickLookThumbnail forKey:memoryCacheKey cost:cost];
            } else {
                NSLog(@"Failed to generate thumbnail for input item %@", inputItem.MRL);
            }
            dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler(quickLookThumbnail ?: self->_noArtImage);
            });
        }];
    });
}

- (void)generateImageForInputItem:(VLCInputItem *)inputItem
                   withCompletion:(void(^)(const NSImage *))completionHandler
{
    NSString * const radioCountryCode = inputItem.radioCountryCodeForFlagArtwork;
    if (!radioCountryCode) {
        [self generateArtworkForInputItem:inputItem withCompletion:completionHandler];
        return;
    }

    [self imageForCountryCode:radioCountryCode
               withCompletion:^(const NSImage * const flagImage) {
        if (flagImage) {
            completionHandler(flagImage);
            return;
        }
        [self generateArtworkForInputItem:inputItem withCompletion:completionHandler];
    }];
}

- (void)generateVLCThumbnailForInputItem:(VLCInputItem *)inputItem
                                cacheKey:(NSString *)cacheKey
                              completion:(VLCThumbnailCompletion)completion
{
    VLCThumbnailRequest *request;
    @synchronized (self) {
        request = _pendingThumbnailRequests[cacheKey];
        if (request != nil) {
            [request.completionHandlers addObject:[completion copy]];
            return;
        }

        request = [[VLCThumbnailRequest alloc] init];
        request.imageCache = self;
        request.cacheKey = cacheKey;
        request.completionHandlers = [NSMutableArray arrayWithObject:[completion copy]];
        _pendingThumbnailRequests[cacheKey] = request;
    }

    vlc_preparser_t * const thumbnailer = getThumbnailer();
    if (thumbnailer == NULL) {
        [self thumbnailRequest:request didFinishWithStatus:VLC_EGENERIC thumbnail:NULL];
        return;
    }

    struct vlc_thumbnailer_arg thumbnailerArgument = {
        .seek = {
            .type = VLC_THUMBNAILER_SEEK_POS,
            .pos = kVLCDefaultThumbnailPosition,
            .speed = VLC_THUMBNAILER_SEEK_FAST,
        },
        .hw_dec = false,
    };
    static const struct vlc_thumbnailer_cbs callbacks = {
        .on_ended = vlcThumbnailerOnEnded,
    };

    vlc_preparser_req * const requestHandle = vlc_preparser_GenerateThumbnail(
        thumbnailer,
        inputItem.vlcInputItem,
        &thumbnailerArgument,
        &callbacks,
        (__bridge void *)request);
    if (requestHandle == NULL) {
        [self thumbnailRequest:request didFinishWithStatus:VLC_EGENERIC thumbnail:NULL];
    }
}

- (void)thumbnailRequest:(VLCThumbnailRequest *)request
     didFinishWithStatus:(int)status
               thumbnail:(picture_t *)thumbnail
{
    NSArray<VLCThumbnailCompletion> *completionHandlers;
    @synchronized (self) {
        [_pendingThumbnailRequests removeObjectForKey:request.cacheKey];
        completionHandlers = [request.completionHandlers copy];
    }

    if (status == -EINTR) {
        for (VLCThumbnailCompletion completion in completionHandlers) {
            completion(nil);
        }
        return;
    }

    NSImage * const image = status == VLC_SUCCESS
        ? [NSImage imageFromVLCPicture:thumbnail
                             vlcObject:VLC_OBJECT(getIntf())
                                  size:NSMakeSize(kVLCDesiredThumbnailWidth,
                                                  kVLCDesiredThumbnailHeight)]
        : nil;

    for (VLCThumbnailCompletion completion in completionHandlers) {
        completion(image);
    }
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
            NSMutableSet<NSString *> * const seenMRLs = NSMutableSet.set;
            NSMutableArray<VLCMediaLibraryMediaItem *> * const uniqueItems =
                [NSMutableArray arrayWithCapacity:kVLCCompositeImageDefaultCompositedGridItemCount];

            [libraryItem enumerateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem * const item, BOOL * const stop) {
                if (uniqueItems.count == (NSUInteger)kVLCCompositeImageDefaultCompositedGridItemCount) {
                    *stop = YES;
                    return;
                }
                NSString * const mrl = item.smallArtworkMRL;
                if (mrl == nil || [seenMRLs containsObject:mrl]) {
                    return;
                }
                [seenMRLs addObject:mrl];
                [uniqueItems addObject:item];
            }];

            if (uniqueItems.count == 0) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    completionHandler(cache->_noArtImage);
                });
                return;
            }

            NSMutableArray<NSImage *> * const itemImages =
                [NSMutableArray arrayWithCapacity:uniqueItems.count];
            dispatch_group_t const group = dispatch_group_create();

            for (VLCMediaLibraryMediaItem * const item in uniqueItems) {
                dispatch_group_enter(group);
                [cache imageForLibraryItem:item withCompletion:^(const NSImage * thumbnail) {
                    NSImage * const mutableRef = (NSImage *)thumbnail;
                    if (mutableRef && ![mutableRef isEqual:cache->_noArtImage]) {
                        @synchronized (itemImages) {
                            [itemImages addObject:mutableRef];
                        }
                    }
                    dispatch_group_leave(group);
                }];
            }

            dispatch_group_notify(group, dispatch_get_main_queue(), ^{
                if (itemImages.count == 0) {
                    completionHandler(cache->_noArtImage);
                    return;
                }

                const NSSize size = NSMakeSize(kVLCDesiredThumbnailWidth, kVLCDesiredThumbnailHeight);
                NSArray<NSValue *> * const frames =
                    [NSImage framesForCompositeImageSquareGridWithImages:itemImages
                                                                    size:size
                                                           gridItemCount:kVLCCompositeImageDefaultCompositedGridItemCount];
                NSImage * const compositeImage =
                    [NSImage compositeImageWithImages:itemImages frames:frames size:size];

                completionHandler(compositeImage);
            });
        });
    } else {
        [cache imageForLibraryItem:libraryItem withCompletion:completionHandler];
    }
}

@end
