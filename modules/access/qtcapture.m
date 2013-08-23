/*****************************************************************************
 * qtcapture.m: qtkit (Mac OS X) based capture module
 *****************************************************************************
 * Copyright © 2008-2011 VLC authors and VideoLAN
 *
 * Authors: Pierre d'Herbemont <pdherbemont@videolan.org>
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

#define QTKIT_VERSION_MIN_REQUIRED 70603

#import <QTKit/QTKit.h>
#import <CoreAudio/CoreAudio.h>

#define QTKIT_WIDTH_TEXT N_("Video Capture width")
#define QTKIT_WIDTH_LONGTEXT N_("Video Capture width in pixel")
#define QTKIT_HEIGHT_TEXT N_("Video Capture height")
#define QTKIT_HEIGHT_LONGTEXT N_("Video Capture height in pixel")

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
   set_shortname(N_("Quicktime Capture"))
   set_description(N_("Quicktime Capture"))
   set_category(CAT_INPUT)
   set_subcategory(SUBCAT_INPUT_ACCESS)
   add_shortcut("qtcapture")
   set_capability("access_demux", 10)
   set_callbacks(Open, Close)
   add_integer("qtcapture-width", 640, QTKIT_WIDTH_TEXT, QTKIT_WIDTH_LONGTEXT, true)
      change_integer_range (80, 1280)
   add_integer("qtcapture-height", 480, QTKIT_HEIGHT_TEXT, QTKIT_HEIGHT_LONGTEXT, true)
      change_integer_range (60, 480)
vlc_module_end ()


/*****************************************************************************
* QTKit Bridge
*****************************************************************************/
@interface VLCDecompressedVideoOutput : QTCaptureDecompressedVideoOutput
{
    CVImageBufferRef currentImageBuffer;
    mtime_t currentPts;
    mtime_t previousPts;
    long timeScale;
}
- (id)init;
- (void)outputVideoFrame:(CVImageBufferRef)videoFrame withSampleBuffer:(QTSampleBuffer *)sampleBuffer fromConnection:(QTCaptureConnection *)connection;
- (mtime_t)copyCurrentFrameToBuffer:(void *)buffer;
@end

/* Apple sample code */
@implementation VLCDecompressedVideoOutput : QTCaptureDecompressedVideoOutput

- (id)init
{
    if (self = [super init]) {
        currentImageBuffer = nil;
        currentPts = 0;
        previousPts = 0;
        timeScale = 0;
    }
    return self;
}

- (void)dealloc
{
    @synchronized (self) {
        CVBufferRelease(currentImageBuffer);
        currentImageBuffer = nil;
    }
    [super dealloc];
}

- (long)timeScale
{
    return timeScale;
}

- (void)outputVideoFrame:(CVImageBufferRef)videoFrame withSampleBuffer:(QTSampleBuffer *)sampleBuffer fromConnection:(QTCaptureConnection *)connection
{
    // Store the latest frame
    // This must be done in a @synchronized block because this delegate method is not called on the main thread
    CVImageBufferRef imageBufferToRelease;

    CVBufferRetain(videoFrame);

    @synchronized (self) {
        imageBufferToRelease = currentImageBuffer;
        currentImageBuffer = videoFrame;
        QTTime timeStamp = [sampleBuffer presentationTime];
        timeScale = timeStamp.timeScale;
        currentPts = (mtime_t)(1000000L / timeScale * timeStamp.timeValue);

        /* Try to use hosttime of the sample if available, because iSight Pts seems broken */
        NSNumber *hosttime = (NSNumber *)[sampleBuffer attributeForKey:QTSampleBufferHostTimeAttribute];
        if (hosttime) currentPts = (mtime_t)AudioConvertHostTimeToNanos([hosttime unsignedLongLongValue])/1000;
    }
    CVBufferRelease(imageBufferToRelease);
}

- (mtime_t)copyCurrentFrameToBuffer:(void *)buffer
{
    CVImageBufferRef imageBuffer;
    mtime_t pts;

void * pixels;

    if (!currentImageBuffer || currentPts == previousPts)
        return 0;

    @synchronized (self) {
        imageBuffer = CVBufferRetain(currentImageBuffer);
        if (imageBuffer) {
            pts = previousPts = currentPts;
            CVPixelBufferLockBaseAddress(imageBuffer, 0);
            pixels = CVPixelBufferGetBaseAddress(imageBuffer);
            if (pixels)
                memcpy(buffer, pixels, CVPixelBufferGetBytesPerRow(imageBuffer) * CVPixelBufferGetHeight(imageBuffer));
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

struct demux_sys_t {
    QTCaptureSession * session;
    QTCaptureDevice * device;
    VLCDecompressedVideoOutput * output;
    int height, width;
    es_out_id_t * p_es_video;
    BOOL b_es_setup;
    es_format_t fmt;
};


/*****************************************************************************
* qtchroma_to_fourcc
*****************************************************************************/
static int qtchroma_to_fourcc(int i_qt)
{
    static const struct
    {
        unsigned int i_qt;
        int i_fourcc;
    } qtchroma_to_fourcc[] =
    {
        /* Raw data types */
        { '2vuy',    VLC_CODEC_UYVY },
        { 'yuv2',VLC_CODEC_YUYV },
        { 'yuvs', VLC_CODEC_YUYV },
        { 0, 0 }
    };

    for (int i = 0; qtchroma_to_fourcc[i].i_qt; i++) {
        if (qtchroma_to_fourcc[i].i_qt == i_qt)
            return qtchroma_to_fourcc[i].i_fourcc;
    }
    return 0;
}

/*****************************************************************************
* Open:
*****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = NULL;
    int i;
    int i_width;
    int i_height;
    int result = 0;
    char *psz_uid = NULL;

    /* Only when selected */
    if (*p_demux->psz_access == '\0')
        return VLC_EGENERIC;

    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    if (p_demux->psz_location && *p_demux->psz_location)
        psz_uid = strdup(p_demux->psz_location);
    msg_Dbg(p_demux, "qtcapture uid = %s", psz_uid);
    NSString *qtk_currdevice_uid = [[NSString alloc] initWithFormat:@"%s", psz_uid];

    /* Set up p_demux */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->info.i_update = 0;
    p_demux->info.i_title = 0;
    p_demux->info.i_seekpoint = 0;

    p_demux->p_sys = p_sys = calloc(1, sizeof(demux_sys_t));
    if (!p_sys)
        return VLC_ENOMEM;

    NSArray *myVideoDevices = [[[QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeVideo] arrayByAddingObjectsFromArray:[QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeMuxed]] retain];
    if ([myVideoDevices count] == 0) {
        dialog_FatalWait(p_demux, _("No Input device found"),
                         _("Your Mac does not seem to be equipped with a suitable input device. "
                           "Please check your connectors and drivers."));
        msg_Err(p_demux, "Can't find any Video device");

        goto error;
    }
    NSUInteger ivideo;
    NSUInteger deviceCount = [myVideoDevices count];
    for (ivideo = 0; ivideo < deviceCount; ivideo++) {
        QTCaptureDevice *qtk_device;
        qtk_device = [myVideoDevices objectAtIndex:ivideo];
        msg_Dbg(p_demux, "qtcapture %lu/%lu %s %s", ivideo, deviceCount, [[qtk_device localizedDisplayName] UTF8String], [[qtk_device uniqueID] UTF8String]);
        if ([[[qtk_device uniqueID]stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] isEqualToString:qtk_currdevice_uid]) {
            break;
        }
    }

    memset(&p_sys->fmt, 0, sizeof(es_format_t));

    QTCaptureDeviceInput * input = nil;
    NSError *o_returnedError;
    if (ivideo < [myVideoDevices count])
        p_sys->device = [myVideoDevices objectAtIndex:ivideo];
    else {
        /* cannot found designated device, fall back to open default device */
        msg_Dbg(p_demux, "Cannot find designated uid device as %s, falling back to default.", [qtk_currdevice_uid UTF8String]);
        p_sys->device = [QTCaptureDevice defaultInputDeviceWithMediaType: QTMediaTypeVideo];
    }
    if (!p_sys->device) {
        dialog_FatalWait(p_demux, _("No Input device found"),
                        _("Your Mac does not seem to be equipped with a suitable input device. "
                          "Please check your connectors and drivers."));
        msg_Err(p_demux, "Can't find any Video device");

        goto error;
    }

    if (![p_sys->device open: &o_returnedError]) {
        msg_Err(p_demux, "Unable to open the capture device (%ld)", [o_returnedError code]);
        goto error;
    }

    if ([p_sys->device isInUseByAnotherApplication] == YES) {
        msg_Err(p_demux, "default capture device is exclusively in use by another application");
        goto error;
    }

    input = [[QTCaptureDeviceInput alloc] initWithDevice: p_sys->device];
    if (!input) {
        msg_Err(p_demux, "can't create a valid capture input facility");
        goto error;
    }

    p_sys->output = [[VLCDecompressedVideoOutput alloc] init];

    /* Get the formats */
    NSArray *format_array = [p_sys->device formatDescriptions];
    QTFormatDescription* camera_format = NULL;
    NSUInteger formatCount = [format_array count];
    for (NSUInteger k = 0; k < formatCount; k++) {
        camera_format = [format_array objectAtIndex:k];

        msg_Dbg(p_demux, "localized Format: %s", [[camera_format localizedFormatSummary] UTF8String]);
        msg_Dbg(p_demux, "format description: %s", [[[camera_format formatDescriptionAttributes] description] UTF8String]);
    }
    if ([format_array count])
        camera_format = [format_array objectAtIndex:0];
    else
        goto error;

    int qtchroma = [camera_format formatType];
    int chroma = VLC_CODEC_UYVY;

    /* Now we can init */
    es_format_Init(&p_sys->fmt, VIDEO_ES, chroma);

    NSSize encoded_size = [[camera_format attributeForKey:QTFormatDescriptionVideoEncodedPixelsSizeAttribute] sizeValue];
    NSSize display_size = [[camera_format attributeForKey:QTFormatDescriptionVideoCleanApertureDisplaySizeAttribute] sizeValue];
    NSSize par_size = [[camera_format attributeForKey:QTFormatDescriptionVideoProductionApertureDisplaySizeAttribute] sizeValue];

    par_size.width = display_size.width = encoded_size.width
        = var_InheritInteger (p_this, "qtcapture-width");
    par_size.height = display_size.height = encoded_size.height
        = var_InheritInteger (p_this, "qtcapture-height");

    p_sys->fmt.video.i_width = p_sys->width = encoded_size.width;
    p_sys->fmt.video.i_height = p_sys->height = encoded_size.height;
    p_sys->fmt.video.i_frame_rate = 25.0; // cave: check with setMinimumVideoFrameInterval (see below)
    if (par_size.width != encoded_size.width) {
        p_sys->fmt.video.i_sar_num = (int64_t)encoded_size.height * par_size.width / encoded_size.width;
        p_sys->fmt.video.i_sar_den = encoded_size.width;
    }

    msg_Dbg(p_demux, "encoded_size %i %i", (int)encoded_size.width, (int)encoded_size.height);
    msg_Dbg(p_demux, "display_size %i %i", (int)display_size.width, (int)display_size.height);
    msg_Dbg(p_demux, "PAR size %i %i", (int)par_size.width, (int)par_size.height);

    [p_sys->output setPixelBufferAttributes: [NSDictionary dictionaryWithObjectsAndKeys:
                                              [NSNumber numberWithInt:kCVPixelFormatType_422YpCbCr8], (id)kCVPixelBufferPixelFormatTypeKey,
                                              [NSNumber numberWithInt:p_sys->height], kCVPixelBufferHeightKey,
                                              [NSNumber numberWithInt:p_sys->width], kCVPixelBufferWidthKey,
                                              [NSNumber numberWithBool:YES], (id)kCVPixelBufferOpenGLCompatibilityKey,
                                              nil]];
    [p_sys->output setAutomaticallyDropsLateVideoFrames:YES];
    [p_sys->output setMinimumVideoFrameInterval: (1/25)]; // 25 fps

    p_sys->session = [[QTCaptureSession alloc] init];

    bool ret = [p_sys->session addInput:input error: &o_returnedError];
    if (!ret) {
        msg_Err(p_demux, "default video capture device could not be added to capture session (%ld)", [o_returnedError code]);
        goto error;
    }

    ret = [p_sys->session addOutput:p_sys->output error: &o_returnedError];
    if (!ret) {
        msg_Err(p_demux, "output could not be added to capture session (%ld)", [o_returnedError code]);
        goto error;
    }

    [p_sys->session startRunning];

    [input release];
    [pool release];

    msg_Dbg(p_demux, "QTCapture: We have a video device ready!");

    return VLC_SUCCESS;
error:
    [input release];
    [pool release];

    free(p_sys);

    return VLC_EGENERIC;
}

/*****************************************************************************
* Close:
*****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    /* Hack: if libvlc was killed, main interface thread was,
     * and poor QTKit needs it, so don't tell him.
     * Else we dead lock. */
    if (vlc_object_alive(p_this->p_libvlc)) {
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
    block_t *p_block;

    p_block = block_Alloc(p_sys->width * p_sys->height * 2 /* FIXME */);
    if (!p_block) {
        msg_Err(p_demux, "cannot get block");
        return 0;
    }

    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    @synchronized (p_sys->output) {
        p_block->i_pts = [p_sys->output copyCurrentFrameToBuffer: p_block->p_buffer];
    }

    if (!p_block->i_pts) {
        /* Nothing to display yet, just forget */
        block_Release(p_block);
        [pool release];
        msleep(10000);
        return 1;
    } else if (!p_sys->b_es_setup) {
        p_sys->fmt.video.i_frame_rate_base = [p_sys->output timeScale];
        msg_Dbg(p_demux, "using frame rate base: %i", p_sys->fmt.video.i_frame_rate_base);
        p_sys->p_es_video = es_out_Add(p_demux->out, &p_sys->fmt);
        msg_Dbg(p_demux, "added new video es %4.4s %dx%d", (char*)&p_sys->fmt.i_codec, p_sys->fmt.video.i_width, p_sys->fmt.video.i_height);
        p_sys->b_es_setup = YES;
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
    bool *pb;
    int64_t    *pi64;

    switch(i_query)
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
