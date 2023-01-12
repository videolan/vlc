/*****************************************************************************
 * avcapture.m: AVFoundation (Mac OS X) based video capture module
 *****************************************************************************
 * Copyright © 2008-2013 VLC authors and VideoLAN
 *
 * Authors: Michael Feurstein <michael.feurstein@gmail.com>
 *
 ****************************************************************************
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#define OS_OBJECT_USE_OBJC 0

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_demux.h>
#include <vlc_interface.h>
#include <vlc_dialog.h>
#include <vlc_access.h>

#import <AvailabilityMacros.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>

#ifndef MAC_OS_X_VERSION_10_14
@interface AVCaptureDevice (AVCaptureDeviceAuthorizationSince10_14)

+ (void)requestAccessForMediaType:(AVMediaType)mediaType completionHandler:(void (^)(BOOL granted))handler API_AVAILABLE(macos(10.14), ios(7.0));

@end
#endif

/*****************************************************************************
* Local prototypes
*****************************************************************************/
static int Open(vlc_object_t *p_this);
static void Close(vlc_object_t *p_this);
static int Demux(demux_t *p_demux);
static int Control(demux_t *, int, va_list);

/*****************************************************************************
* Module descriptor
*****************************************************************************/
vlc_module_begin ()
   set_shortname(N_("AVFoundation Video Capture"))
   set_description(N_("AVFoundation video capture module."))
   set_subcategory(SUBCAT_INPUT_ACCESS)
   add_shortcut("avcapture")
   set_capability("access", 0)
   set_callbacks(Open, Close)
vlc_module_end ()

static vlc_tick_t vlc_CMTime_to_tick(CMTime timestamp)
{
    CMTime scaled = CMTimeConvertScale(
            timestamp, CLOCK_FREQ,
            kCMTimeRoundingMethod_Default);

    return VLC_TICK_0 + scaled.value;
}

/*****************************************************************************
* AVFoundation Bridge
*****************************************************************************/
@interface VLCAVDecompressedVideoOutput :
    AVCaptureVideoDataOutput <AVCaptureVideoDataOutputSampleBufferDelegate>
{
    demux_t             *p_avcapture;

    CVImageBufferRef    currentImageBuffer;

    vlc_tick_t          currentPts;
    vlc_tick_t          previousPts;
    size_t              bytesPerRow;

    long                timeScale;
    BOOL                videoDimensionsReady;
}

@property (readwrite) CMVideoDimensions videoDimensions;

- (id)initWithDemux:(demux_t *)p_demux;
- (int)width;
- (int)height;
- (void)getVideoDimensions:(CMSampleBufferRef)sampleBuffer;
- (vlc_tick_t)currentPts;
- (void)captureOutput:(AVCaptureOutput *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection;
- (vlc_tick_t)copyCurrentFrameToBuffer:(void *)buffer;
@end

@implementation VLCAVDecompressedVideoOutput : AVCaptureVideoDataOutput

- (id)initWithDemux:(demux_t *)p_demux
{
    if (self = [super init])
    {
        p_avcapture = p_demux;
        currentImageBuffer = nil;
        currentPts = 0;
        previousPts = 0;
        bytesPerRow = 0;
        timeScale = 0;
        videoDimensionsReady = NO;
    }
    return self;
}

- (void)dealloc
{
    @synchronized (self)
    {
        CVBufferRelease(currentImageBuffer);
        currentImageBuffer = nil;
        bytesPerRow = 0;
        videoDimensionsReady = NO;
    }
}

- (long)timeScale
{
    return timeScale;
}

- (int)width
{
    return self.videoDimensions.width;
}

- (int)height
{
    return self.videoDimensions.height;
}

- (size_t)bytesPerRow
{
    return bytesPerRow;
}

- (void)getVideoDimensions:(CMSampleBufferRef)sampleBuffer
{
    if (!videoDimensionsReady)
    {
        CMFormatDescriptionRef formatDescription = CMSampleBufferGetFormatDescription(sampleBuffer);
        self.videoDimensions = CMVideoFormatDescriptionGetDimensions(formatDescription);
        bytesPerRow = CVPixelBufferGetBytesPerRow(CMSampleBufferGetImageBuffer(sampleBuffer));
        videoDimensionsReady = YES;
        msg_Dbg(p_avcapture, "Dimensions obtained height:%i width:%i bytesPerRow:%lu", [self height], [self width], bytesPerRow);
    }
}

- (vlc_tick_t)currentPts
{
    vlc_tick_t pts;

    @synchronized (self)
    {
       if ( !currentImageBuffer || currentPts == previousPts )
           return VLC_TICK_INVALID;
        pts = previousPts = currentPts;
    }

    return pts;
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection
{
    @autoreleasepool {
        CVImageBufferRef imageBufferToRelease;
        CMTime presentationtimestamp = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
        CVImageBufferRef videoFrame = CMSampleBufferGetImageBuffer(sampleBuffer);
        CVBufferRetain(videoFrame);
        [self getVideoDimensions:sampleBuffer];


        @synchronized (self) {
            imageBufferToRelease = currentImageBuffer;
            currentImageBuffer = videoFrame;
            currentPts = vlc_CMTime_to_tick(presentationtimestamp);
            timeScale = (long)presentationtimestamp.timescale;
        }

        CVBufferRelease(imageBufferToRelease);
    }
}

- (vlc_tick_t)copyCurrentFrameToBuffer:(void *)buffer
{
    CVImageBufferRef imageBuffer;
    void *pixels = NULL;

    if ( !currentImageBuffer || currentPts == previousPts )
        return 0;

    @synchronized (self)
    {
        imageBuffer = CVBufferRetain(currentImageBuffer);
        if (imageBuffer)
        {
            previousPts = currentPts;
            CVPixelBufferLockBaseAddress(imageBuffer, 0);
            pixels = CVPixelBufferGetBaseAddress(imageBuffer);
            if (pixels)
            {
                memcpy(buffer, pixels, CVPixelBufferGetHeight(imageBuffer) * CVPixelBufferGetBytesPerRow(imageBuffer));
            }
            CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
        }
    }
    CVBufferRelease(imageBuffer);

    if (pixels)
        return currentPts;
    else
        return 0;
}

@end

/*****************************************************************************
* Struct
*****************************************************************************/

@interface VLCAVCaptureDemux : NSObject {
    demux_t                         *_demux;
    AVCaptureSession                *_session;
    AVCaptureDevice                 *_device;
    VLCAVDecompressedVideoOutput    *_output;
    es_out_id_t                     *_es_video;
    es_format_t                     _fmt;
    int                             _height, _width;
}

- (VLCAVCaptureDemux*)init:(demux_t *)demux;
- (int)demux;
- (vlc_tick_t)pts;
- (void)dealloc;
@end

/*****************************************************************************
* Open:
*****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    demux_t                 *p_demux = (demux_t*)p_this;

    if (p_demux->out == NULL)
        return VLC_EGENERIC;

    @autoreleasepool {
        VLCAVCaptureDemux *demux = [[VLCAVCaptureDemux alloc] init:p_demux];
        if (demux == nil)
            return VLC_EGENERIC;
        p_demux->p_sys = (__bridge_retained void*)demux;
    }
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    return VLC_SUCCESS;
}

/*****************************************************************************
* Close:
*****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    demux_t *p_demux = (demux_t*)p_this;
    VLCAVCaptureDemux *demux =
        (__bridge_transfer VLCAVCaptureDemux*)p_demux->p_sys;

    /* Signal ARC we won't use those references anymore. */
    p_demux->p_sys = nil;
    (void)demux;
}

/*****************************************************************************
* Demux:
*****************************************************************************/
static int Demux(demux_t *p_demux)
{
    VLCAVCaptureDemux *demux = (__bridge VLCAVCaptureDemux *)p_demux->p_sys;
    return [demux demux];
}

/*****************************************************************************
* Control:
*****************************************************************************/
static int Control(demux_t *p_demux, int i_query, va_list args)
{
    VLCAVCaptureDemux *demux = (__bridge VLCAVCaptureDemux *)p_demux->p_sys;
    bool        *pb;

    switch( i_query )
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
           pb = va_arg(args, bool *);
           *pb = false;
           return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
           *va_arg(args, vlc_tick_t *) =
               VLC_TICK_FROM_MS(var_InheritInteger(p_demux, "live-caching"));
           return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            *va_arg(args, vlc_tick_t *) = [demux pts];
            return VLC_SUCCESS;

        default:
           return VLC_EGENERIC;
    }
    return VLC_EGENERIC;
}

@implementation VLCAVCaptureDemux
- (VLCAVCaptureDemux *)init:(demux_t *)p_demux
{
    NSString                *avf_currdevice_uid;
    NSArray                 *myVideoDevices;
    NSError                 *o_returnedError;

    AVCaptureDeviceInput    *input = nil;

    int                     deviceCount, ivideo;

    char                    *psz_uid = NULL;

    _demux = p_demux;

    if (_demux->psz_location && *_demux->psz_location)
        psz_uid = strdup(_demux->psz_location);

    msg_Dbg(_demux, "avcapture uid = %s", psz_uid);
    avf_currdevice_uid = [[NSString alloc] initWithFormat:@"%s", psz_uid];

    myVideoDevices = [[AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo]
                       arrayByAddingObjectsFromArray:[AVCaptureDevice devicesWithMediaType:AVMediaTypeMuxed]];
    if ( [myVideoDevices count] == 0 )
    {
        vlc_dialog_display_error(_demux, _("No video devices found"),
            _("Your Mac does not seem to be equipped with a suitable video input device. "
            "Please check your connectors and drivers."));
        msg_Err(_demux, "Can't find any suitable video device");
        return nil;
    }

    deviceCount = [myVideoDevices count];
    for ( ivideo = 0; ivideo < deviceCount; ivideo++ )
    {
        AVCaptureDevice *avf_device;
        avf_device = [myVideoDevices objectAtIndex:ivideo];
        msg_Dbg(_demux, "avcapture %i/%i %s %s", ivideo, deviceCount, [[avf_device modelID] UTF8String], [[avf_device uniqueID] UTF8String]);
        if ([[[avf_device uniqueID]stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] isEqualToString:avf_currdevice_uid]) {
            break;
        }
    }

    if ( ivideo < [myVideoDevices count] )
    {
        _device = [myVideoDevices objectAtIndex:ivideo];
    }
    else
    {
        msg_Dbg(_demux, "Cannot find designated device as %s, falling back to default.", [avf_currdevice_uid UTF8String]);
        _device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    }

    if ( !_device )
    {
        vlc_dialog_display_error(_demux, _("No video devices found"),
            _("Your Mac does not seem to be equipped with a suitable input device. "
            "Please check your connectors and drivers."));
        msg_Err(_demux, "Can't find any suitable video device");
        return nil;
    }

    AVCaptureDevice *device = _device;

    if ( [device isInUseByAnotherApplication] == YES )
    {
        msg_Err(_demux, "default capture device is exclusively in use by another application");
        return nil;
    }

    if (@available(macOS 10.14, *)) {
        msg_Dbg(_demux, "Check user consent for access to the video device");

        dispatch_semaphore_t sema = dispatch_semaphore_create(0);
        __block bool accessGranted = NO;
        [AVCaptureDevice requestAccessForMediaType: AVMediaTypeVideo completionHandler:^(BOOL granted) {
            accessGranted = granted;
            dispatch_semaphore_signal(sema);
        } ];
        dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
        dispatch_release(sema);
        if (!accessGranted) {
            msg_Err(_demux, "Can't use the video device as access has not been granted by the user");
            vlc_dialog_display_error(_demux, _("Problem accessing a system resource"),
                _("Please open \"System Preferences\" -> \"Security & Privacy\" "
                  "and allow VLC to access your camera."));

            return nil;
        }
    }

    input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&o_returnedError];

    if ( !input )
    {
        msg_Err(_demux, "can't create a valid capture input facility: %s (%ld)",[[o_returnedError localizedDescription] UTF8String], [o_returnedError code]);
        return nil;
    }

    int chroma = VLC_CODEC_BGRA;

    memset(&_fmt, 0, sizeof(es_format_t));
    es_format_Init(&_fmt, VIDEO_ES, chroma);

    _session = [[AVCaptureSession alloc] init];

    [_session addInput:input];

    _output = [[VLCAVDecompressedVideoOutput alloc] initWithDemux:_demux];

    [_session addOutput:_output];

    dispatch_queue_t queue = dispatch_queue_create("avCaptureQueue", NULL);
    [_output setSampleBufferDelegate:_output queue:queue];
    dispatch_release(queue);

    [_output setVideoSettings:[NSDictionary dictionaryWithObject:[NSNumber numberWithInt:kCVPixelFormatType_32BGRA] forKey:(id)kCVPixelBufferPixelFormatTypeKey]];
    [_session startRunning];

    input = nil;

    msg_Dbg(_demux, "AVCapture: Video device ready!");

    return self;
}

- (int)demux
{
    block_t     *p_block;

    @autoreleasepool {
        @synchronized(_output)
        {
            p_block = block_Alloc([_output width] * [_output bytesPerRow]);

            if ( !p_block )
            {
                msg_Err(_demux, "cannot get block");
                return 0;
            }

            p_block->i_pts = [_output copyCurrentFrameToBuffer: p_block->p_buffer];

            if ( !p_block->i_pts )
            {
                /* Nothing to display yet, just forget */
                block_Release(p_block);
                vlc_tick_sleep(VLC_HARD_MIN_SLEEP);
                return 1;
            }
            else if ( !_es_video )
            {
                _fmt.video.i_frame_rate = 1;
                _fmt.video.i_frame_rate_base = [_output timeScale];
                msg_Dbg(_demux, "using frame rate base: %i", _fmt.video.i_frame_rate_base);
                _width
                    = _fmt.video.i_width
                    = _fmt.video.i_visible_width
                    = [_output width];
                _height
                    = _fmt.video.i_height
                    = _fmt.video.i_visible_height
                    = [_output height];
                _fmt.video.i_chroma = _fmt.i_codec;

                _es_video = es_out_Add(_demux->out, &_fmt);
                video_format_Print(&_demux->obj, "added new video es", &_fmt.video);
            }
        }

        es_out_SetPCR(_demux->out, p_block->i_pts);
        es_out_Send(_demux->out, _es_video, p_block);
    }
    return 1;
}

- (vlc_tick_t)pts
{
    return [_output currentPts];
}

- (void)dealloc
{
    // Perform this on main thread, as the framework itself will sometimes try to synchronously
    // work on main thread. And this will create a dead lock.
    [_session performSelectorOnMainThread:@selector(stopRunning) withObject:nil waitUntilDone:NO];
}
@end
