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
   set_category(CAT_INPUT)
   set_subcategory(SUBCAT_INPUT_ACCESS)
   add_shortcut("avcapture")
   set_capability("access", 0)
   set_callbacks(Open, Close)
vlc_module_end ()


/*****************************************************************************
* AVFoundation Bridge
*****************************************************************************/
@interface VLCAVDecompressedVideoOutput : AVCaptureVideoDataOutput
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
        msg_Dbg(p_avcapture, "Dimensionns obtained height:%i width:%i bytesPerRow:%lu", [self height], [self width], bytesPerRow);
    }
}

-(vlc_tick_t)currentPts
{
    vlc_tick_t pts;

    if ( !currentImageBuffer || currentPts == previousPts )
        return 0;

    @synchronized (self)
    {
        pts = previousPts = currentPts;
    }

    return currentPts;
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
            currentPts = (vlc_tick_t)presentationtimestamp.value;
            timeScale = (long)presentationtimestamp.timescale;
        }
        
        CVBufferRelease(imageBufferToRelease);
    }
}

- (vlc_tick_t)copyCurrentFrameToBuffer:(void *)buffer
{
    CVImageBufferRef imageBuffer;
    vlc_tick_t pts;

    void *pixels;

    if ( !currentImageBuffer || currentPts == previousPts )
        return 0;

    @synchronized (self)
    {
        imageBuffer = CVBufferRetain(currentImageBuffer);
        if (imageBuffer)
        {
            pts = previousPts = currentPts;
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

typedef struct demux_sys_t
{
    CFTypeRef _Nullable             session;       // AVCaptureSession
    CFTypeRef _Nullable             device;        // AVCaptureDevice
    CFTypeRef _Nullable             output;        // VLCAVDecompressedVideoOutput
    es_out_id_t                     *p_es_video;
    es_format_t                     fmt;
    int                             height, width;
    BOOL                            b_es_setup;
} demux_sys_t;

/*****************************************************************************
* Open:
*****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    demux_t                 *p_demux = (demux_t*)p_this;
    demux_sys_t             *p_sys = NULL;

    NSString                *avf_currdevice_uid;
    NSArray                 *myVideoDevices;
    NSError                 *o_returnedError;

    AVCaptureDeviceInput    *input = nil;

    int                     i, i_width, i_height, deviceCount, ivideo;

    char                    *psz_uid = NULL;

    if (p_demux->out == NULL)
        return VLC_EGENERIC;

    @autoreleasepool {
        if (p_demux->psz_location && *p_demux->psz_location)
            psz_uid = strdup(p_demux->psz_location);

        msg_Dbg(p_demux, "avcapture uid = %s", psz_uid);
        avf_currdevice_uid = [[NSString alloc] initWithFormat:@"%s", psz_uid];

        /* Set up p_demux */
        p_demux->pf_demux = Demux;
        p_demux->pf_control = Control;

        p_demux->p_sys = p_sys = calloc(1, sizeof(demux_sys_t));
        if ( !p_sys )
            return VLC_ENOMEM;

        myVideoDevices = [[AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo]
                           arrayByAddingObjectsFromArray:[AVCaptureDevice devicesWithMediaType:AVMediaTypeMuxed]];
        if ( [myVideoDevices count] == 0 )
        {
            vlc_dialog_display_error(p_demux, _("No video devices found"),
                _("Your Mac does not seem to be equipped with a suitable video input device. "
                "Please check your connectors and drivers."));
            msg_Err(p_demux, "Can't find any suitable video device");
            goto error;
        }

        deviceCount = [myVideoDevices count];
        for ( ivideo = 0; ivideo < deviceCount; ivideo++ )
        {
            AVCaptureDevice *avf_device;
            avf_device = [myVideoDevices objectAtIndex:ivideo];
            msg_Dbg(p_demux, "avcapture %i/%i %s %s", ivideo, deviceCount, [[avf_device modelID] UTF8String], [[avf_device uniqueID] UTF8String]);
            if ([[[avf_device uniqueID]stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] isEqualToString:avf_currdevice_uid]) {
                break;
            }
        }

        if ( ivideo < [myVideoDevices count] )
        {
            p_sys->device = CFBridgingRetain([myVideoDevices objectAtIndex:ivideo]);
        }
        else
        {
            msg_Dbg(p_demux, "Cannot find designated device as %s, falling back to default.", [avf_currdevice_uid UTF8String]);
            p_sys->device = CFBridgingRetain([AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo]);
        }
        if ( !p_sys->device )
        {
            vlc_dialog_display_error(p_demux, _("No video devices found"),
                _("Your Mac does not seem to be equipped with a suitable input device. "
                "Please check your connectors and drivers."));
            msg_Err(p_demux, "Can't find any suitable video device");
            goto error;
        }

        if ( [(__bridge AVCaptureDevice *)p_sys->device isInUseByAnotherApplication] == YES )
        {
            msg_Err(p_demux, "default capture device is exclusively in use by another application");
            goto error;
        }

        if (@available(macOS 10.14, *)) {
            msg_Dbg(p_demux, "Check user consent for access to the video device");

            dispatch_semaphore_t sema = dispatch_semaphore_create(0);
            __block bool accessGranted = NO;
            [AVCaptureDevice requestAccessForMediaType: AVMediaTypeVideo completionHandler:^(BOOL granted) {
                accessGranted = granted;
                dispatch_semaphore_signal(sema);
            } ];
            dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
            dispatch_release(sema);
            if (!accessGranted) {
                msg_Err(p_demux, "Can't use the video device as access has not been granted by the user");
                vlc_dialog_display_error(p_demux, _("Problem accessing a system resource"),
                    _("Please open \"System Preferences\" -> \"Security & Privacy\" "
                      "and allow VLC to access your camera."));

                goto error;
            }
        }

        input = [AVCaptureDeviceInput deviceInputWithDevice:(__bridge AVCaptureDevice *)p_sys->device error:&o_returnedError];

        if ( !input )
        {
            msg_Err(p_demux, "can't create a valid capture input facility: %s (%ld)",[[o_returnedError localizedDescription] UTF8String], [o_returnedError code]);
            goto error;
        }


        int chroma = VLC_CODEC_RGB32;

        memset(&p_sys->fmt, 0, sizeof(es_format_t));
        es_format_Init(&p_sys->fmt, VIDEO_ES, chroma);

        p_sys->session = CFBridgingRetain([[AVCaptureSession alloc] init]);
        [(__bridge AVCaptureSession *)p_sys->session addInput:input];

        p_sys->output = CFBridgingRetain([[VLCAVDecompressedVideoOutput alloc] initWithDemux:p_demux]);
        [(__bridge AVCaptureSession *)p_sys->session addOutput:(__bridge VLCAVDecompressedVideoOutput *)p_sys->output];

        dispatch_queue_t queue = dispatch_queue_create("avCaptureQueue", NULL);
        [(__bridge VLCAVDecompressedVideoOutput *)p_sys->output setSampleBufferDelegate:(__bridge id)p_sys->output queue:queue];
        dispatch_release(queue);

        [(__bridge VLCAVDecompressedVideoOutput *)p_sys->output setVideoSettings:[NSDictionary dictionaryWithObject:[NSNumber numberWithInt:kCVPixelFormatType_32BGRA] forKey:(id)kCVPixelBufferPixelFormatTypeKey]];
        [(__bridge AVCaptureSession *)p_sys->session startRunning];

        input = nil;

        msg_Dbg(p_demux, "AVCapture: Video device ready!");

        return VLC_SUCCESS;
    error:
        msg_Err(p_demux, "Error");
        input = nil;

        free(p_sys);

        return VLC_EGENERIC;
    }
}

/*****************************************************************************
* Close:
*****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    demux_t             *p_demux = (demux_t*)p_this;
    demux_sys_t         *p_sys = p_demux->p_sys;

    @autoreleasepool {
        msg_Dbg(p_demux,"Close AVCapture");

        // Perform this on main thread, as the framework itself will sometimes try to synchronously
        // work on main thread. And this will create a dead lock.
        [(__bridge AVCaptureSession *)p_sys->session performSelectorOnMainThread:@selector(stopRunning) withObject:nil waitUntilDone:NO];
        CFBridgingRelease(p_sys->output);
        CFBridgingRelease(p_sys->session);

        free(p_sys);
    }
}

/*****************************************************************************
* Demux:
*****************************************************************************/
static int Demux(demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_block;

    @autoreleasepool {
        @synchronized ( p_sys->output )
        {
            p_block = block_Alloc([(__bridge VLCAVDecompressedVideoOutput *)p_sys->output width] * [(__bridge VLCAVDecompressedVideoOutput *)p_sys->output bytesPerRow]);

            if ( !p_block )
            {
                msg_Err(p_demux, "cannot get block");
                return 0;
            }

            p_block->i_pts = [(__bridge VLCAVDecompressedVideoOutput *)p_sys->output copyCurrentFrameToBuffer: p_block->p_buffer];

            if ( !p_block->i_pts )
            {
                /* Nothing to display yet, just forget */
                block_Release(p_block);
                vlc_tick_sleep(VLC_HARD_MIN_SLEEP);
                return 1;
            }
            else if ( !p_sys->b_es_setup )
            {
                p_sys->fmt.video.i_frame_rate_base = [(__bridge VLCAVDecompressedVideoOutput *)p_sys->output timeScale];
                msg_Dbg(p_demux, "using frame rate base: %i", p_sys->fmt.video.i_frame_rate_base);
                p_sys->width = p_sys->fmt.video.i_width = [(__bridge VLCAVDecompressedVideoOutput *)p_sys->output width];
                p_sys->height = p_sys->fmt.video.i_height = [(__bridge VLCAVDecompressedVideoOutput *)p_sys->output height];
                p_sys->p_es_video = es_out_Add(p_demux->out, &p_sys->fmt);
                msg_Dbg(p_demux, "added new video es %4.4s %dx%d", (char*)&p_sys->fmt.i_codec, p_sys->width, p_sys->height);
                p_sys->b_es_setup = YES;
            }
        }
        
        es_out_SetPCR(p_demux->out, p_block->i_pts);
        es_out_Send(p_demux->out, p_sys->p_es_video, p_block);
        
    }
    return 1;
}

/*****************************************************************************
* Control:
*****************************************************************************/
static int Control(demux_t *p_demux, int i_query, va_list args)
{
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
            *va_arg(args, vlc_tick_t *) = vlc_tick_now();
            return VLC_SUCCESS;

        default:
           return VLC_EGENERIC;
    }
    return VLC_EGENERIC;
}
