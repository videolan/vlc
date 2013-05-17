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

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>

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
   set_capability("access_demux", 10)
   set_callbacks(Open, Close)
vlc_module_end ()


/*****************************************************************************
* AVFoundation Bridge
*****************************************************************************/
@interface VLCAVDecompressedVideoOutput : AVCaptureVideoDataOutput
{
    demux_t             *p_avcapture;

    CVImageBufferRef    currentImageBuffer;
    CMVideoDimensions   videoDimensions;

    mtime_t             currentPts;
    mtime_t             previousPts;
    size_t              bytesPerRow;

    long                timeScale;
    BOOL                videoDimensionsReady;
}

@property (readwrite) CMVideoDimensions videoDimensions;

- (id)initWithDemux:(demux_t *)p_demux;
- (int)width;
- (int)height;
- (void)getVideoDimensions:(CMSampleBufferRef)sampleBuffer;
- (mtime_t)currentPts;
- (void)captureOutput:(AVCaptureOutput *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection;
- (mtime_t)copyCurrentFrameToBuffer:(void *)buffer;
@end

@implementation VLCAVDecompressedVideoOutput : AVCaptureVideoDataOutput

@synthesize videoDimensions;

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
    [super dealloc];
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

-(mtime_t)currentPts
{
    mtime_t pts;

    if ( !currentImageBuffer || currentPts == previousPts )
        return 0;

    @synchronized (self)
    {
        pts = previousPts = currentPts;
    }

    return currentPts;
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    CVImageBufferRef imageBufferToRelease;
    CMTime presentationtimestamp = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
    CVImageBufferRef videoFrame = CMSampleBufferGetImageBuffer(sampleBuffer);
    CVBufferRetain(videoFrame);
    [self getVideoDimensions:sampleBuffer];

    @synchronized (self)
    {
        imageBufferToRelease = currentImageBuffer;
        currentImageBuffer = videoFrame;
        currentPts = (mtime_t)presentationtimestamp.value;
        timeScale = (long)presentationtimestamp.timescale;
    }

    CVBufferRelease(imageBufferToRelease);
    [pool release];
}

- (mtime_t)copyCurrentFrameToBuffer:(void *)buffer
{
    CVImageBufferRef imageBuffer;
    mtime_t pts;

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

struct demux_sys_t
{
    AVCaptureSession                *session;
    AVCaptureDevice                 *device;
    VLCAVDecompressedVideoOutput    *output;
    es_out_id_t                     *p_es_video;
    es_format_t                     fmt;
    int                             height, width;
    BOOL                            b_es_setup;
};

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

    NSAutoreleasePool       *pool = [[NSAutoreleasePool alloc] init];

    /* Only when selected */
    if ( *p_demux->psz_access == '\0' )
        return VLC_EGENERIC;

    if ( p_demux->psz_location && *p_demux->psz_location )
        psz_uid = strdup(p_demux->psz_location);

    msg_Dbg(p_demux, "avcapture uid = %s", psz_uid);
    avf_currdevice_uid = [[NSString alloc] initWithFormat:@"%s", psz_uid];

    /* Set up p_demux */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->info.i_update = 0;
    p_demux->info.i_title = 0;
    p_demux->info.i_seekpoint = 0;

    p_demux->p_sys = p_sys = calloc(1, sizeof(demux_sys_t));
    if ( !p_sys )
        return VLC_ENOMEM;

    myVideoDevices = [[[AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo] arrayByAddingObjectsFromArray:[AVCaptureDevice devicesWithMediaType:AVMediaTypeMuxed]] retain];
    if ( [myVideoDevices count] == 0 )
    {
        dialog_FatalWait(p_demux, _("No video devices found"),
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
        msg_Dbg(p_demux, "avcapture %lu/%lu %s %s", ivideo, deviceCount, [[avf_device modelID] UTF8String], [[avf_device uniqueID] UTF8String]);
        if ([[[avf_device uniqueID]stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] isEqualToString:avf_currdevice_uid]) {
            break;
        }
    }

    if ( ivideo < [myVideoDevices count] )
    {
       p_sys->device = [myVideoDevices objectAtIndex:ivideo];
    }
    else
    {
        msg_Dbg(p_demux, "Cannot find designated device as %s, falling back to default.", [avf_currdevice_uid UTF8String]);
        p_sys->device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    }
    if ( !p_sys->device )
    {
        dialog_FatalWait(p_demux, _("No video devices found"),
                        _("Your Mac does not seem to be equipped with a suitable input device. "
                          "Please check your connectors and drivers."));
        msg_Err(p_demux, "Can't find any suitable video device");
        goto error;
    }

    if ( [p_sys->device isInUseByAnotherApplication] == YES )
    {
        msg_Err(p_demux, "default capture device is exclusively in use by another application");
        goto error;
    }

    input = [AVCaptureDeviceInput deviceInputWithDevice:p_sys->device error:&o_returnedError];

    if ( !input )
    {
        msg_Err(p_demux, "can't create a valid capture input facility (%ld)", [o_returnedError code]);
        goto error;
    }

    int chroma = VLC_CODEC_RGB32;

    memset(&p_sys->fmt, 0, sizeof(es_format_t));
    es_format_Init(&p_sys->fmt, VIDEO_ES, chroma);

    p_sys->session = [[AVCaptureSession alloc] init];
    [p_sys->session addInput:input];

    p_sys->output = [[VLCAVDecompressedVideoOutput alloc] initWithDemux:p_demux];
    [p_sys->session addOutput:p_sys->output];

    dispatch_queue_t queue = dispatch_queue_create("avCaptureQueue", NULL);
    [p_sys->output setSampleBufferDelegate:(id)p_sys->output queue:queue];
    dispatch_release(queue);

    p_sys->output.videoSettings = [NSDictionary dictionaryWithObject:@(kCVPixelFormatType_32BGRA) forKey:(id)kCVPixelBufferPixelFormatTypeKey];
    [p_sys->session startRunning];

    [input release];

    msg_Dbg(p_demux, "AVCapture: Video device ready!");

    [pool release];
    return VLC_SUCCESS;
error:
    msg_Err(p_demux, "Error");
    [input release];

    free(p_sys);

    [pool release];
    return VLC_EGENERIC;
}

/*****************************************************************************
* Close:
*****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    NSAutoreleasePool   *pool = [[NSAutoreleasePool alloc] init];
    demux_t             *p_demux = (demux_t*)p_this;
    demux_sys_t         *p_sys = p_demux->p_sys;

    msg_Dbg(p_demux,"Close AVCapture");

    if ( vlc_object_alive(p_this->p_libvlc) )
    {
        // Perform this on main thread, as the framework itself will sometimes try to synchronously
        // work on main thread. And this will create a dead lock.
        [p_sys->session performSelectorOnMainThread:@selector(stopRunning) withObject:nil waitUntilDone:NO];
        [p_sys->output performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];
        [p_sys->session performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];
    }

    free(p_sys);

    [pool release];
}

/*****************************************************************************
* Demux:
*****************************************************************************/
static int Demux(demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_block;

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    @synchronized ( p_sys->output )
    {
        p_block = block_Alloc([p_sys->output width] * [p_sys->output bytesPerRow]);

        if ( !p_block )
        {
            msg_Err(p_demux, "cannot get block");
            return 0;
        }

        p_block->i_pts = [p_sys->output copyCurrentFrameToBuffer: p_block->p_buffer];

        if ( !p_block->i_pts )
        {
            /* Nothing to display yet, just forget */
            block_Release(p_block);
            [pool release];
            msleep(10000);
            return 1;
        }
        else if ( !p_sys->b_es_setup )
        {
            p_sys->fmt.video.i_frame_rate_base = [p_sys->output timeScale];
            msg_Dbg(p_demux, "using frame rate base: %i", p_sys->fmt.video.i_frame_rate_base);
            p_sys->width = p_sys->fmt.video.i_width = [p_sys->output width];
            p_sys->height = p_sys->fmt.video.i_height = [p_sys->output height];
            p_sys->p_es_video = es_out_Add(p_demux->out, &p_sys->fmt);
            msg_Dbg(p_demux, "added new video es %4.4s %dx%d", (char*)&p_sys->fmt.i_codec, p_sys->width, p_sys->height);
            p_sys->b_es_setup = YES;
        }
    }

    es_out_Control(p_demux->out, ES_OUT_SET_PCR, p_block->i_pts);
    es_out_Send(p_demux->out, p_sys->p_es_video, p_block);

    [pool release];
    return 1;
}

/*****************************************************************************
* Control:
*****************************************************************************/
static int Control(demux_t *p_demux, int i_query, va_list args)
{
    bool        *pb;
    int64_t     *pi64;

    switch( i_query )
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
           pb = (bool*)va_arg(args, bool *);
           *pb = false;
           return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
           pi64 = (int64_t*)va_arg(args, int64_t *);
           *pi64 = INT64_C(1000) * var_InheritInteger(p_demux, "live-caching");
           return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg(args, int64_t *);
            *pi64 = mdate();
            return VLC_SUCCESS;

        default:
           return VLC_EGENERIC;
    }
    return VLC_EGENERIC;
}
