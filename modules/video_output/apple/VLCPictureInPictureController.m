/*****************************************************************************
 * VLCPictureInPictureController.m: Picture In Picture controller for iOS and
 * tvOS
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Authors: Maxime Chapelet <umxprime at videolabs dot io>
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>

#import <TargetConditionals.h>
#import <Foundation/Foundation.h>
#import <AVKit/AVKit.h>
#import <AVFoundation/AVFoundation.h>
#import "VLCDrawable.h"

#include "vlc_pip_controller.h"

API_AVAILABLE(ios(15.0), tvos(15.0), macosx(12.0))
@interface VLCPictureInPictureController: NSObject
    <AVPictureInPictureSampleBufferPlaybackDelegate, 
     AVPictureInPictureControllerDelegate,
     VLCPictureInPictureWindowControlling>

@property (nonatomic, readonly) NSObject *avPipController;
@property (nonatomic, readonly) pip_controller_t *pipcontroller;
@property (nonatomic, readonly, weak) id<VLCPictureInPictureDrawable> drawable;
@property (nonatomic) void(^stateChangeEventHandler)(BOOL isStarted);

- (instancetype)initWithPipController:(pip_controller_t *)pipcontroller;
- (void)invalidatePlaybackState;

@end

@implementation VLCPictureInPictureController

- (instancetype)initWithPipController:(pip_controller_t *)pipcontroller {
    self = [super init];
    if (!self)
        return nil;
    _pipcontroller = pipcontroller;

    id drawable = (__bridge id)var_InheritAddress (pipcontroller, "drawable-nsobject");
    
    if (![drawable conformsToProtocol:@protocol(VLCPictureInPictureDrawable)])
        return nil;
    
    _drawable = drawable;

    return self;
}

- (void)prepare:(AVSampleBufferDisplayLayer *)layer {
    if (![AVPictureInPictureController isPictureInPictureSupported]) {
        msg_Err(_pipcontroller, "Picture In Picture isn't supported");
        return;
    }
        
    assert(layer);
    AVPictureInPictureControllerContentSource *avPipSource;
    avPipSource = [
        [AVPictureInPictureControllerContentSource alloc]
            initWithSampleBufferDisplayLayer:layer
                            playbackDelegate:self
    ];
    AVPictureInPictureController *avPipController = [
        [AVPictureInPictureController alloc]
            initWithContentSource:avPipSource
    ];
    void *mediaController = (__bridge void*)_drawable.mediaController;
    BOOL isMediaSeekable = (BOOL)_pipcontroller->media_cbs->is_media_seekable(mediaController);
    avPipController.requiresLinearPlayback = !isMediaSeekable;
    avPipController.delegate = self;
#if TARGET_OS_IOS
    // Check if the drawable implements the new method to Controls whether PiP 
    // can start automatically when video enters inline mode
    if ([_drawable respondsToSelector:@selector(canStartPictureInPictureAutomaticallyFromInline)]) {
        avPipController.canStartPictureInPictureAutomaticallyFromInline = 
            [_drawable canStartPictureInPictureAutomaticallyFromInline];
    } else {
        // Use default value if method not implemented
        avPipController.canStartPictureInPictureAutomaticallyFromInline = YES;
    }
#endif
    _avPipController = avPipController;


    [_avPipController addObserver:self
                       forKeyPath:@"isPictureInPicturePossible"
                          options:NSKeyValueObservingOptionInitial|NSKeyValueObservingOptionNew
                          context:NULL];

    _drawable.pictureInPictureReady(self);
}

- (void)close {
    AVPictureInPictureController *avPipController =
        (AVPictureInPictureController *)_avPipController;
    NSObject *observer = self;
    dispatch_async(dispatch_get_main_queue(),^{
        avPipController.contentSource = nil;
        [avPipController removeObserver:observer forKeyPath:@"isPictureInPicturePossible"];
    });
    _avPipController = nil;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey,id> *)change
                       context:(void *)context {
    if (object==_avPipController) {
        if ([keyPath isEqualToString:@"isPictureInPicturePossible"]) {
            msg_Dbg(_pipcontroller, "isPictureInPicturePossible:%d", [change[NSKeyValueChangeNewKey] boolValue]);
        }
    } else {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
    }
}

#pragma mark - AVPictureInPictureSampleBufferPlaybackDelegate

- (void)pictureInPictureController:(AVPictureInPictureController *)pictureInPictureController
         didTransitionToRenderSize:(CMVideoDimensions)newRenderSize {

}

- (void)pictureInPictureController:(AVPictureInPictureController *)pictureInPictureController
                        setPlaying:(BOOL)playing {
    assert(_pipcontroller->media_cbs);
    void *mediaController = (__bridge void*)_drawable.mediaController;
    bool isPlaying = _pipcontroller->media_cbs->is_media_playing(mediaController);
    if(isPlaying && !playing)
        _pipcontroller->media_cbs->pause(mediaController);    
    if(!isPlaying && playing)
        _pipcontroller->media_cbs->play(mediaController);
}

- (void)pictureInPictureController:(AVPictureInPictureController *)pictureInPictureController
                    skipByInterval:(CMTime)skipInterval
                 completionHandler:(void (^)(void))completionHandler {
    assert(_pipcontroller->media_cbs);
    Float64 time_sec = CMTimeGetSeconds(skipInterval);
    
    void *mediaController = (__bridge void*)_drawable.mediaController;
    int64_t offset = time_sec * 1e3;
    _pipcontroller->media_cbs->seek_by(offset, completionHandler, mediaController);
}

- (BOOL)pictureInPictureControllerIsPlaybackPaused:(AVPictureInPictureController *)pictureInPictureController {
    assert(_pipcontroller->media_cbs);
    void *mediaController = (__bridge void*)_drawable.mediaController;
    return ! _pipcontroller->media_cbs->is_media_playing(mediaController);
}

- (CMTimeRange)pictureInPictureControllerTimeRangeForPlayback:(AVPictureInPictureController *)pictureInPictureController {
    assert(_pipcontroller->media_cbs);
    //TODO: Handle media duration
    void *mediaController = (__bridge void*)_drawable.mediaController;
    const CMTimeRange live = CMTimeRangeMake(kCMTimeNegativeInfinity, kCMTimePositiveInfinity);
    
    int64_t length = _pipcontroller->media_cbs->media_length(mediaController);
    if (length == VLC_TICK_INVALID)
        return live;

    int64_t time = _pipcontroller->media_cbs->media_time(mediaController);

    CFTimeInterval ca_now = CACurrentMediaTime();
    CFTimeInterval time_sec = ((Float64)time) * 1e-3;
    CFTimeInterval start = ca_now - time_sec;
    CFTimeInterval duration = ((Float64)length) * 1e-3;
    return CMTimeRangeMake(CMTimeMakeWithSeconds(start, 1000000), CMTimeMakeWithSeconds(duration, 1000000));
}

- (BOOL)pictureInPictureControllerShouldProhibitBackgroundAudioPlayback:(AVPictureInPictureController *)pictureInPictureController {
    return NO;
}

#pragma mark - AVPictureInPictureControllerDelegate

- (void)pictureInPictureControllerWillStartPictureInPicture:(AVPictureInPictureController *)pictureInPictureController {
    /**
     * Invalidation seems mandatory as
     * pictureInPictureControllerTimeRangeForPlayback: isn't automatically
     * called each time PiP is activated
     */
    [self invalidatePlaybackState];
}

- (void)pictureInPictureControllerDidStartPictureInPicture:(AVPictureInPictureController *)pictureInPictureController {
    if (_stateChangeEventHandler)
        _stateChangeEventHandler(YES);
}

- (void)pictureInPictureControllerDidStopPictureInPicture:(AVPictureInPictureController *)pictureInPictureController {
    if(_stateChangeEventHandler)
        _stateChangeEventHandler(NO);
}

#pragma mark - VLCDisplayPictureInPictureControlling

- (void)startPictureInPicture {
    AVPictureInPictureController *avPipController =
        (AVPictureInPictureController *)_avPipController;
    [avPipController startPictureInPicture];
}

- (void)stopPictureInPicture {
    AVPictureInPictureController *avPipController =
        (AVPictureInPictureController *)_avPipController;
    [avPipController stopPictureInPicture];
}

- (void)invalidatePlaybackState {
    AVPictureInPictureController *avPipController =
        (AVPictureInPictureController *)_avPipController;
    [avPipController invalidatePlaybackState];
}

@end

static void SetDisplayLayer( pip_controller_t *pipcontroller, void *layer) {
    if (@available(macOS 12.0, iOS 15.0, tvos 15.0, *)) {
        VLCPictureInPictureController *sys = 
            (__bridge VLCPictureInPictureController*)pipcontroller->p_sys;
        
        AVSampleBufferDisplayLayer *displayLayer =
            (__bridge AVSampleBufferDisplayLayer *)layer;
        
        [sys prepare:displayLayer];
    }
}

static void PipControllerMediaPlay(void *opaque) {
    id<VLCPictureInPictureMediaControlling> mediaController;
    mediaController = (__bridge id<VLCPictureInPictureMediaControlling>)opaque;
    [mediaController play];
}

static void PipControllerMediaPause(void *opaque) {
    id<VLCPictureInPictureMediaControlling> mediaController;
    mediaController = (__bridge id<VLCPictureInPictureMediaControlling>)opaque;
    [mediaController pause];
}

static void PipControllerMediaSeekBy(vlc_tick_t time, dispatch_block_t completion, void *opaque) {
    id<VLCPictureInPictureMediaControlling> mediaController;
    mediaController = (__bridge id<VLCPictureInPictureMediaControlling>)opaque;
    [mediaController seekBy:time completion:completion];
}

static vlc_tick_t PipControllerMediaGetLength(void *opaque) {
    id<VLCPictureInPictureMediaControlling> mediaController;
    mediaController = (__bridge id<VLCPictureInPictureMediaControlling>)opaque;
    return [mediaController mediaLength];
}

static vlc_tick_t PipControllerMediaGetTime(void *opaque) {
    id<VLCPictureInPictureMediaControlling> mediaController;
    mediaController = (__bridge id<VLCPictureInPictureMediaControlling>)opaque;
    return [mediaController mediaTime];
}

static bool PipControllerMediaIsSeekable(void *opaque) {
    id<VLCPictureInPictureMediaControlling> mediaController;
    mediaController = (__bridge id<VLCPictureInPictureMediaControlling>)opaque;
    return (bool)[mediaController isMediaSeekable];
}

static bool PipControllerMediaIsPlaying(void *opaque) {
    id<VLCPictureInPictureMediaControlling> mediaController;
    mediaController = (__bridge id<VLCPictureInPictureMediaControlling>)opaque;
    return [mediaController isMediaPlaying];
}

static int CloseController( pip_controller_t *pipcontroller )
{
    if (@available(macOS 12.0, iOS 15.0, tvos 15.0, *)) {
        VLCPictureInPictureController *sys = 
            (__bridge_transfer VLCPictureInPictureController*)pipcontroller->p_sys;

        [sys close];
    }
    
    return VLC_SUCCESS;
}

static int OpenController( pip_controller_t *pipcontroller )
{
    static const struct pip_controller_operations ops = {
        SetDisplayLayer, 
        CloseController
    };

    pipcontroller->ops = &ops;

    static const struct pip_controller_media_callbacks cbs = {
        PipControllerMediaPlay,
        PipControllerMediaPause,
        PipControllerMediaSeekBy,
        PipControllerMediaGetLength,
        PipControllerMediaGetTime,
        PipControllerMediaIsSeekable,
        PipControllerMediaIsPlaying
    };
    
    pipcontroller->media_cbs = &cbs;

    if (@available(macOS 12.0, iOS 15.0, tvos 15.0, *)) {
        VLCPictureInPictureController *sys = 
        [[VLCPictureInPictureController alloc] 
            initWithPipController:pipcontroller];
        
        if (sys == nil) {
            return VLC_EGENERIC;
        }

        pipcontroller->p_sys = (__bridge_retained void*)sys;
    } else {
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*
 * Module descriptor
 */
vlc_module_begin()
    set_description(N_("Picture in picture controller for Apple systems"))
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_callback(OpenController)
    set_capability("pictureinpicture", 100)
vlc_module_end()
