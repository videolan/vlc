/*****************************************************************************
 * avaudiocapture.m: AVFoundation based audio capture module
 *****************************************************************************
 * Copyright © 2018 VLC authors and VideoLAN
 *
 * Authors: Pierre d'Herbemont <pdherbemont@videolan.org>
 *          Gustaf Neumann <neumann@wu.ac.at>
 *          Michael S. Feurstein <michael.feurstein@wu.ac.at>
 *          David Fuhrmann <dfuhrmann at videolan dot org>
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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_demux.h>
#include <vlc_dialog.h>

#import <AvailabilityMacros.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>


#ifndef MAC_OS_X_VERSION_10_14
@interface AVCaptureDevice (AVCaptureDeviceAuthorizationSince10_14)

+ (void)requestAccessForMediaType:(AVMediaType)mediaType completionHandler:(void (^)(BOOL granted))handler API_AVAILABLE(macos(10.14), ios(7.0));

@end
#endif

/*****************************************************************************
 * Struct
 *****************************************************************************/

typedef struct demux_sys_t
{
    CFTypeRef _Nullable             session;       // AVCaptureSession
    es_out_id_t                     *p_es_audio;

} demux_sys_t;


/*****************************************************************************
* AVFoundation Bridge
*****************************************************************************/
@interface VLCAVDecompressedAudioOutput : AVCaptureAudioDataOutput<AVCaptureAudioDataOutputSampleBufferDelegate>
{
    demux_t *p_avcapture;
    date_t date;
}

@end

@implementation VLCAVDecompressedAudioOutput : AVCaptureAudioDataOutput

- (id)initWithDemux:(demux_t *)p_demux
{
    if (self = [super init])
    {
        p_avcapture = p_demux;

        date_Init(&date, 44100, 1);
        date_Set(&date, VLC_TICK_0);
    }
    return self;
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection
{
    @autoreleasepool {
        CMItemCount numSamplesInBuffer = CMSampleBufferGetNumSamples(sampleBuffer);

        size_t neededBufferListSize = 0;
        // first get needed size for buffer
        OSStatus retValue = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(sampleBuffer, &neededBufferListSize, nil, 0, nil, nil, kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, nil);

        if (retValue != noErr) {
            msg_Err(p_avcapture, "Error getting sample list buffer size: %d", retValue);
            return;
        }

        CMBlockBufferRef blockBuffer;
        AudioBufferList *audioBufferList = calloc(1, neededBufferListSize);
        retValue = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(sampleBuffer, nil, audioBufferList, neededBufferListSize, nil, nil, kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, &blockBuffer);

        if (retValue != noErr) {
            msg_Err(p_avcapture, "Cannot get samples from buffer: %d", retValue);
            return;
        }

        if (audioBufferList->mNumberBuffers != 1) {
            msg_Warn(p_avcapture, "This module expects 1 buffer only, got %d", audioBufferList->mNumberBuffers);
            return;
        }

        int64_t totalDataSize = audioBufferList->mBuffers[0].mDataByteSize;
        block_t *outBlock = block_Alloc(totalDataSize);
        if (!outBlock)
            return;

        date_Increment(&date, (uint32_t)numSamplesInBuffer);

        memcpy(outBlock->p_buffer, audioBufferList->mBuffers[0].mData, totalDataSize);
        outBlock->i_pts = date_Get(&date);
        outBlock->i_nb_samples = (unsigned)numSamplesInBuffer;

        CFRelease(blockBuffer);

        demux_sys_t *p_sys = p_avcapture->p_sys;
        es_out_SetPCR(p_avcapture->out, outBlock->i_pts);
        es_out_Send(p_avcapture->out, p_sys->p_es_audio, outBlock);
    }
}

@end


/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control(demux_t *p_demux, int i_query, va_list args)
{
    bool *pb;

    switch(i_query) {
            /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
            pb = (bool*)va_arg(args, bool *);
            *pb = false;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            *va_arg(args, vlc_tick_t *) =
            VLC_TICK_FROM_MS(var_InheritInteger(p_demux, "live-caching"));
            return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
    }
    return VLC_EGENERIC;
}

/*****************************************************************************
* Open:
*****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    demux_t *p_demux = (demux_t*)p_this;

    if (p_demux->out == NULL)
        return VLC_EGENERIC;

    @autoreleasepool {
        NSString *currentDeviceId = @"";
        if (p_demux->psz_location && *p_demux->psz_location)
            currentDeviceId = [NSString stringWithUTF8String:p_demux->psz_location];

        msg_Dbg(p_demux, "avcapture uid = %s", currentDeviceId.UTF8String);

        NSArray *knownAudioDevices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeAudio];
        NSInteger numberOfKnownAudioDevices = [knownAudioDevices count];

        int selectedDevice = 0;
        for (;selectedDevice < numberOfKnownAudioDevices; selectedDevice++ )
        {
            AVCaptureDevice *avf_device = [knownAudioDevices objectAtIndex:selectedDevice];
            msg_Dbg(p_demux, "avcapture %i/%ld %s %s", selectedDevice, (long)numberOfKnownAudioDevices, [[avf_device modelID] UTF8String], [[avf_device uniqueID] UTF8String]);
            if ([[[avf_device uniqueID] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] isEqualToString:currentDeviceId]) {
                break;
            }
        }

        AVCaptureDevice *device = nil;
        if (selectedDevice < numberOfKnownAudioDevices) {
            device = [knownAudioDevices objectAtIndex:selectedDevice];
        } else {
            msg_Dbg(p_demux, "Cannot find designated device as %s, falling back to default.", currentDeviceId.UTF8String);
            device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeAudio];
        }

        if (!device) {
            vlc_dialog_display_error(p_demux, _("No Audio Input device found"),
                                     _("Your Mac does not seem to be equipped with a suitable audio input device."
                                       "Please check your connectors and drivers."));
            msg_Err(p_demux, "Can't find any Audio device");
            return VLC_EGENERIC;
        }

        if ([device isInUseByAnotherApplication]) {
            msg_Err(p_demux, "Capture device is exclusively in use by another application");
            return VLC_EGENERIC;
        }

        if (@available(macOS 10.14, *)) {
            msg_Dbg(p_demux, "Check user consent for access to the audio device");

            dispatch_semaphore_t sema = dispatch_semaphore_create(0);
            __block bool accessGranted = NO;
            [AVCaptureDevice requestAccessForMediaType: AVMediaTypeAudio completionHandler:^(BOOL granted) {
                accessGranted = granted;
                dispatch_semaphore_signal(sema);
            } ];
            dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
            if (!accessGranted) {
                msg_Err(p_demux, "Can't use the audio device as access has not been granted by the user");
                vlc_dialog_display_error(p_demux, _("Problem accessing a system resource"),
                    _("Please open \"System Preferences\" -> \"Security & Privacy\" "
                      "and allow VLC to access your microphone."));

                return VLC_EGENERIC;
            }
        }

        NSError *error = nil;
        AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
        if (!input) {
            msg_Err(p_demux, "can't create a valid capture input facility (%ld)", [error code]);
            return VLC_EGENERIC;
        }

        /* Now we can init */
        int audiocodec = VLC_CODEC_FL32;
        es_format_t audiofmt;
        es_format_Init(&audiofmt, AUDIO_ES, audiocodec);

        audiofmt.audio.i_format = audiocodec;
        audiofmt.audio.i_rate = 44100;
        /*
         * i_physical_channels Describes the channels configuration of the
         * samples (ie. number of channels which are available in the
         * buffer, and positions).
         */
        audiofmt.audio.i_physical_channels = AOUT_CHAN_RIGHT | AOUT_CHAN_LEFT;
        /*
         * Please note that it may be completely arbitrary - buffers are not
         * obliged to contain a integral number of so-called "frames". It's
         * just here for the division:
         * buffer_size = i_nb_samples * i_bytes_per_frame / i_frame_length
         */
        audiofmt.audio.i_bitspersample = 32;
        audiofmt.audio.i_channels = 2;
        audiofmt.audio.i_blockalign = audiofmt.audio.i_channels * (audiofmt.audio.i_bitspersample / 8);
        audiofmt.i_bitrate = audiofmt.audio.i_channels * audiofmt.audio.i_rate * audiofmt.audio.i_bitspersample;


        AVCaptureSession *session = [[AVCaptureSession alloc] init];
        [session addInput:input];

        VLCAVDecompressedAudioOutput *output = [[VLCAVDecompressedAudioOutput alloc] initWithDemux:p_demux];
        [session addOutput:output];

        dispatch_queue_t queue = dispatch_queue_create("avCaptureQueue", NULL);
        [output setSampleBufferDelegate:output queue:queue];

        [output setAudioSettings:
            @{
              AVFormatIDKey : @(kAudioFormatLinearPCM),
              AVLinearPCMBitDepthKey : @(32),
              AVLinearPCMIsFloatKey : @YES,
              AVLinearPCMIsBigEndianKey : @NO,
              AVNumberOfChannelsKey : @(2),
              AVLinearPCMIsNonInterleaved : @NO,
              AVSampleRateKey : @(44100.0)
            }];

        /* Set up p_demux */
        p_demux->pf_demux = NULL;
        p_demux->pf_control = Control;

        demux_sys_t *p_sys = NULL;
        p_demux->p_sys = p_sys = calloc(1, sizeof(demux_sys_t));
        if (!p_sys)
            return VLC_ENOMEM;

        p_sys->session = CFBridgingRetain(session);
        p_sys->p_es_audio = es_out_Add(p_demux->out, &audiofmt);

        [session startRunning];
        msg_Dbg(p_demux, "AVCapture: Audio device ready!");
        return VLC_SUCCESS;
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

        ///@todo Investigate why this should be needed
        // Perform this on main thread, as the framework itself will sometimes try to synchronously
        // work on main thread. And this will create a dead lock.
//        [(__bridge AVCaptureSession *)p_sys->session performSelectorOnMainThread:@selector(stopRunning) withObject:nil waitUntilDone:YES];

        [(__bridge AVCaptureSession *)p_sys->session stopRunning];
        CFBridgingRelease(p_sys->session);

        free(p_sys);
    }
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
set_shortname(N_("AVFoundation Audio Capture"))
set_description(N_("AVFoundation audio capture module."))
set_category(CAT_INPUT)
set_subcategory(SUBCAT_INPUT_ACCESS)
add_shortcut("qtsound")
set_capability("access", 0)
set_callbacks(Open, Close)
vlc_module_end ()
