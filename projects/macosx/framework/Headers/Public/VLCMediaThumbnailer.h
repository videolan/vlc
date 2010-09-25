//
//  VLCMediaThumbnailer.h
//  VLCKit
//
//  Created by Pierre d'Herbemont on 7/10/10.
//  Copyright 2010 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>
#if TARGET_OS_IPHONE
# import <CoreGraphics/CoreGraphics.h>
#endif

@class VLCMedia;
@protocol VLCMediaThumbnailerDelegate;

@interface VLCMediaThumbnailer : NSObject {
    id<VLCMediaThumbnailerDelegate> _delegate;
    VLCMedia *_media;
    void *_mp;
    CGImageRef _thumbnail;
    void *_data;
    NSTimer *_parsingTimeoutTimer;
    NSTimer *_thumbnailingTimeoutTimer;

    CGFloat _thumbnailHeight,_thumbnailWidth;
    CGFloat _effectiveThumbnailHeight,_effectiveThumbnailWidth;
    int _numberOfReceivedFrames;
    BOOL _shouldRejectFrames;
}

+ (VLCMediaThumbnailer *)thumbnailerWithMedia:(VLCMedia *)media andDelegate:(id<VLCMediaThumbnailerDelegate>)delegate;
- (void)fetchThumbnail;

@property (readwrite, assign) id<VLCMediaThumbnailerDelegate> delegate;
@property (readwrite, retain) VLCMedia *media;
@property (readwrite, assign) CGImageRef thumbnail;

/**
 * Thumbnail Height
 * You shouldn't change this after -fetchThumbnail
 * has been called.
 * @return thumbnail height. Default value 240.
 */
@property (readwrite, assign) CGFloat thumbnailHeight;

/**
 * Thumbnail Width
 * You shouldn't change this after -fetchThumbnail
 * has been called.
 * @return thumbnail height. Default value 320
 */
@property (readwrite, assign) CGFloat thumbnailWidth;
@end

@protocol VLCMediaThumbnailerDelegate
@required
- (void)mediaThumbnailerDidTimeOut:(VLCMediaThumbnailer *)mediaThumbnailer;
- (void)mediaThumbnailer:(VLCMediaThumbnailer *)mediaThumbnailer didFinishThumbnail:(CGImageRef)thumbnail;
@end
