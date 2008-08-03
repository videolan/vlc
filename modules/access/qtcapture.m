/*****************************************************************************
* qtcapture.m: qtkit (Mac OS X) based capture module
*****************************************************************************
* Copyright (C) 2008 the VideoLAN team
*
* Authors: Pierre d'Herbemont <pdherbemont@videolan.org>
*
*****************************************************************************
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation;
* version 2 of the License.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
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
#include <vlc_vout.h>
#include <vlc_demux.h>
#include <vlc_interface.h>

#import <QTKit/QTKit.h>
#import <CoreAudio/CoreAudio.h>

/*****************************************************************************
* Local prototypes
*****************************************************************************/
static int Open( vlc_object_t *p_this );
static void Close( vlc_object_t *p_this );
static int Demux( demux_t *p_demux );
static int Control( demux_t *, int, va_list );

/*****************************************************************************
* Module descriptor
*****************************************************************************/
vlc_module_begin();
   set_shortname( N_("Quicktime Capture") );
   set_description( N_("Quicktime Capture") );
   set_category( CAT_INPUT );
   set_subcategory( SUBCAT_INPUT_ACCESS );
   add_shortcut( "qtcapture" );
   set_capability( "access_demux", 10 );
   set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
* QTKit Bridge
*****************************************************************************/
@interface VLCDecompressedVideoOutput : QTCaptureDecompressedVideoOutput
{
    CVImageBufferRef currentImageBuffer;
    mtime_t currentPts;
    mtime_t previousPts;
}
- (id)init;
- (void)outputVideoFrame:(CVImageBufferRef)videoFrame withSampleBuffer:(QTSampleBuffer *)sampleBuffer fromConnection:(QTCaptureConnection *)connection;
- (mtime_t)copyCurrentFrameToBuffer:(void *)buffer;
@end

/* Apple sample code */
@implementation VLCDecompressedVideoOutput : QTCaptureDecompressedVideoOutput
- (id)init
{
    if( self = [super init] )
    {
        currentImageBuffer = nil;
        currentPts = 0;
        previousPts = 0;
    }
    return self;
}
- (void)dealloc
{
    @synchronized (self)
    {
        CVBufferRelease(currentImageBuffer);
        currentImageBuffer = nil;
    }
    [super dealloc];
}

- (void)outputVideoFrame:(CVImageBufferRef)videoFrame withSampleBuffer:(QTSampleBuffer *)sampleBuffer fromConnection:(QTCaptureConnection *)connection
{
    // Store the latest frame
    // This must be done in a @synchronized block because this delegate method is not called on the main thread
    CVImageBufferRef imageBufferToRelease;

    CVBufferRetain(videoFrame);

    @synchronized (self)
    {
        imageBufferToRelease = currentImageBuffer;
        currentImageBuffer = videoFrame;
        currentPts = (mtime_t)(1000000L / [sampleBuffer presentationTime].timeScale * [sampleBuffer presentationTime].timeValue);
        
        /* Try to use hosttime of the sample if available, because iSight Pts seems broken */
        NSNumber *hosttime = (NSNumber *)[sampleBuffer attributeForKey:QTSampleBufferHostTimeAttribute];
        if( hosttime ) currentPts = (mtime_t)AudioConvertHostTimeToNanos([hosttime unsignedLongLongValue])/1000;
    }
    CVBufferRelease(imageBufferToRelease);
}

- (mtime_t)copyCurrentFrameToBuffer:(void *)buffer
{
    CVImageBufferRef imageBuffer;
    mtime_t pts;

    if(!currentImageBuffer || currentPts == previousPts )
        return 0;

    @synchronized (self)
    {
        imageBuffer = CVBufferRetain(currentImageBuffer);
        pts = previousPts = currentPts;

        CVPixelBufferLockBaseAddress(imageBuffer, 0);
        void * pixels = CVPixelBufferGetBaseAddress(imageBuffer);
        memcpy( buffer, pixels, CVPixelBufferGetBytesPerRow(imageBuffer) * CVPixelBufferGetHeight(imageBuffer) );
        CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
    }

    CVBufferRelease(imageBuffer);

    return currentPts;
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
};


/*****************************************************************************
* qtchroma_to_fourcc
*****************************************************************************/
static int qtchroma_to_fourcc( int i_qt )
{
    static const struct
    {
        unsigned int i_qt;
        int i_fourcc;
    } qtchroma_to_fourcc[] =
    {
        /* Raw data types */
        { k422YpCbCr8CodecType,    VLC_FOURCC('U','Y','V','Y') },
        { 0, 0 }
    };
    int i;
    for( i = 0; qtchroma_to_fourcc[i].i_qt; i++ )
    {
        if( qtchroma_to_fourcc[i].i_qt == i_qt )
            return qtchroma_to_fourcc[i].i_fourcc;
    }
    return 0;
}

/*****************************************************************************
* Open:
*****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = NULL;
    es_format_t fmt;
    int i;
    int i_width;
    int i_height;
    int i_aspect;
    int result = 0;

    /* Only when selected */
    if( *p_demux->psz_access == '\0' )
        return VLC_EGENERIC;
    
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    /* Set up p_demux */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->info.i_update = 0;
    p_demux->info.i_title = 0;
    p_demux->info.i_seekpoint = 0;
    
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    if( !p_sys ) return VLC_ENOMEM;
    
    memset( p_sys, 0, sizeof( demux_sys_t ) );
    memset( &fmt, 0, sizeof( es_format_t ) );    
    
    msg_Dbg( p_demux, "QTCapture Probed" );

    QTCaptureDeviceInput * input = nil;
    NSError *o_returnedError;

    p_sys->device = [QTCaptureDevice defaultInputDeviceWithMediaType: QTMediaTypeVideo];
    if( !p_sys->device )
    {
        intf_UserFatal( p_demux, true, _("No Input device found"),
                        _("Your Mac does not seem to be equipped with a suitable input device. "
                          "Please check your connectors and drivers.") );
        msg_Err( p_demux, "Can't find any Video device" );
        
        goto error;
    }

    if( ![p_sys->device open: &o_returnedError] )
    {
        msg_Err( p_demux, "Unable to open the capture device (%i)", [o_returnedError code] );
        goto error;
    }

    if( [p_sys->device isInUseByAnotherApplication] == YES )
    {
        msg_Err( p_demux, "default capture device is exclusively in use by another application" );
        goto error;
    }

    input = [[QTCaptureDeviceInput alloc] initWithDevice: p_sys->device];
    if( !input )
    {
        msg_Err( p_demux, "can't create a valid capture input facility" );
        goto error;
    }

    p_sys->output = [[VLCDecompressedVideoOutput alloc] init];

    /* Hack - This will lower CPU consumption for some reason */
    [p_sys->output setPixelBufferAttributes: [NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithInt:480], kCVPixelBufferHeightKey,
        [NSNumber numberWithInt:640], kCVPixelBufferWidthKey, nil]];

    p_sys->session = [[QTCaptureSession alloc] init];

    bool ret = [p_sys->session addInput:input error: &o_returnedError];
    if( !ret )
    {
        msg_Err( p_demux, "default video capture device could not be added to capture session (%i)", [o_returnedError code] );
        goto error;
    }

    ret = [p_sys->session addOutput:p_sys->output error: &o_returnedError];
    if( !ret )
    {
        msg_Err( p_demux, "output could not be added to capture session (%i)", [o_returnedError code] );
        goto error;
    }

    [p_sys->session startRunning];


    int qtchroma = [[[p_sys->device formatDescriptions] objectAtIndex: 0] formatType]; /* FIXME */
    int chroma = qtchroma_to_fourcc( qtchroma );
    if( !chroma )
    {
        msg_Err( p_demux, "Unknown qt chroma %4.4s provided by camera", (char*)&qtchroma );
        goto error;
    }

    /* Now we can init */

    es_format_Init( &fmt, VIDEO_ES, chroma );

    NSSize size = [[p_sys->device attributeForKey:QTFormatDescriptionVideoEncodedPixelsSizeAttribute] sizeValue];
    p_sys->width = fmt.video.i_width = 640;/* size.width; FIXME */
    p_sys->height = fmt.video.i_height = 480;/* size.height; FIXME */

    msg_Dbg( p_demux, "added new video es %4.4s %dx%d",
            (char*)&fmt.i_codec, fmt.video.i_width, fmt.video.i_height );

    p_sys->p_es_video = es_out_Add( p_demux->out, &fmt );

    [input release];
    [pool release];

    msg_Dbg( p_demux, "QTCapture: We have a video device ready!" );

    return VLC_SUCCESS;
error:
    [input release];
    [pool release];

    free( p_sys );

    return VLC_EGENERIC;
}

/*****************************************************************************
* Close:
*****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    /* Hack: if libvlc was killed, main interface thread was,
     * and poor QTKit needs it, so don't tell him.
     * Else we dead lock. */
    if( vlc_object_alive(p_this->p_libvlc))
    {
        [p_sys->session stopRunning];
        [p_sys->output release];
        [p_sys->session release];
    }
    free( p_sys );

    [pool release];
}


/*****************************************************************************
* Demux:
*****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_block;

    p_block = block_New( p_demux, p_sys->width *
                            p_sys->height * 2 /* FIXME */ );
    if( !p_block )
    {
        msg_Err( p_demux, "cannot get block" );
        return 0;
    }

    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    @synchronized (p_sys->output)
    {
    p_block->i_pts = [p_sys->output copyCurrentFrameToBuffer: p_block->p_buffer];
    }

    if( !p_block->i_pts )
    {
        /* Nothing to display yet, just forget */
        block_Release( p_block );
        [pool release];
        msleep( 10000 );
        return 1;
    }

    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block->i_pts );
    es_out_Send( p_demux->out, p_sys->p_es_video, p_block );

    [pool release];
    return 1;
}

/*****************************************************************************
* Control:
*****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    bool *pb;
    int64_t    *pi64;

    switch( i_query )
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
           pb = (bool*)va_arg( args, bool * );
           *pb = false;
           return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
           pi64 = (int64_t*)va_arg( args, int64_t * );
           *pi64 = (int64_t)DEFAULT_PTS_DELAY;
           return VLC_SUCCESS;

        default:
           return VLC_EGENERIC;
    }
    return VLC_EGENERIC;
}
