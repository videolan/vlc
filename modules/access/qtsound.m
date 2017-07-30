/*****************************************************************************
* qtsound.m: qtkit (Mac OS X) based audio capture module
*****************************************************************************
* Copyright © 2011 VLC authors and VideoLAN
*
* Authors: Pierre d'Herbemont <pdherbemont@videolan.org>
*          Gustaf Neumann <neumann@wu.ac.at>
*          Michael S. Feurstein <michael.feurstein@wu.ac.at>
*
*****************************************************************************
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public License
* as published by the Free Software Foundation; either version 2.1
* of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110, USA
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
#include <vlc_aout.h>

#include <vlc_demux.h>
#include <vlc_dialog.h>

#define QTKIT_VERSION_MIN_REQUIRED 70603

#import <QTKit/QTKit.h>

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int Open(vlc_object_t *p_this);
static void Close(vlc_object_t *p_this);
static int Demux(demux_t *p_demux);
static int Control(demux_t *, int, va_list);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin()
set_shortname(N_("QTSound"))
set_description(N_("QuickTime Sound Capture"))
set_category(CAT_INPUT)
set_subcategory(SUBCAT_INPUT_ACCESS)
add_shortcut("qtsound")
set_capability("access_demux", 0)
set_callbacks(Open, Close)
vlc_module_end ()


/*****************************************************************************
 * QTKit Bridge
 *****************************************************************************/
@interface VLCDecompressedAudioOutput : QTCaptureDecompressedAudioOutput
{
    demux_t *p_qtsound;
    AudioBuffer *currentAudioBuffer;
    void *rawAudioData;
    UInt32 numberOfSamples;
    date_t date;
    mtime_t currentPts;
    mtime_t previousPts;
}
- (id)initWithDemux:(demux_t *)p_demux;
- (void)outputAudioSampleBuffer:(QTSampleBuffer *)sampleBuffer fromConnection:(QTCaptureConnection *)connection;
- (BOOL)checkCurrentAudioBuffer;
- (void)freeAudioMem;
- (mtime_t)getCurrentPts;
- (void *)getCurrentAudioBufferData;
- (UInt32)getCurrentTotalDataSize;
- (UInt32)getNumberOfSamples;

@end

@implementation VLCDecompressedAudioOutput : QTCaptureDecompressedAudioOutput
- (id)initWithDemux:(demux_t *)p_demux
{
    if (self = [super init]) {
        p_qtsound = p_demux;
        currentAudioBuffer = nil;
        date_Init(&date, 44100, 1);
        date_Set(&date,0);
        currentPts = 0;
        previousPts = 0;
    }
    return self;
}
- (void)dealloc
{
    [super dealloc];
}

- (void)outputAudioSampleBuffer:(QTSampleBuffer *)sampleBuffer fromConnection:(QTCaptureConnection *)connection
{
    AudioBufferList *tempAudioBufferList;
    UInt32 totalDataSize = 0;
    UInt32 count = 0;

    @synchronized (self) {
        numberOfSamples = [sampleBuffer numberOfSamples];
        date_Increment(&date,numberOfSamples);
        currentPts = date_Get(&date);

        tempAudioBufferList = [sampleBuffer audioBufferListWithOptions:0];
        if (tempAudioBufferList->mNumberBuffers == 2) {
            /*
             * Compute totalDataSize as sum of all data blocks in the
             * audio buffer list:
             */
            for (count = 0; count < tempAudioBufferList->mNumberBuffers; count++)
                totalDataSize += tempAudioBufferList->mBuffers[count].mDataByteSize;

            /*
             * Allocate storage for the interleaved audio data
             */
            rawAudioData = malloc(totalDataSize);
            if (NULL == rawAudioData) {
                msg_Err(p_qtsound, "Raw audiodata could not be allocated");
                return;
            }
        } else {
            msg_Err(p_qtsound, "Too many or only one channel found: %i.",
                               tempAudioBufferList->mNumberBuffers);
            return;
        }

        /*
         * Interleave raw data (provided in two separate channels as
         * F32L) with 2 samples per frame
         */
        if (totalDataSize) {
            unsigned short i;
            const float *b1Ptr, *b2Ptr;
            float *uPtr;

            for (i = 0,
                 uPtr = (float *)rawAudioData,
                 b1Ptr = (const float *) tempAudioBufferList->mBuffers[0].mData,
                 b2Ptr = (const float *) tempAudioBufferList->mBuffers[1].mData;
                 i < numberOfSamples; i++) {
                *uPtr++ = *b1Ptr++;
                *uPtr++ = *b2Ptr++;
            }

            if (currentAudioBuffer == nil) {
                currentAudioBuffer = (AudioBuffer *)malloc(sizeof(AudioBuffer));
                if (NULL == currentAudioBuffer) {
                    msg_Err(p_qtsound, "AudioBuffer could not be allocated.");
                    return;
                }
            }
            currentAudioBuffer->mNumberChannels = 2;
            currentAudioBuffer->mDataByteSize = totalDataSize;
            currentAudioBuffer->mData = rawAudioData;
        }
    }
}

- (BOOL)checkCurrentAudioBuffer
{
    return (currentAudioBuffer) ? 1 : 0;
}

- (void)freeAudioMem
{
    FREENULL(rawAudioData);
}

- (mtime_t)getCurrentPts
{
    /* FIXME: can this getter be minimized? */
    mtime_t pts;

    if(!currentAudioBuffer || currentPts == previousPts)
        return 0;

    @synchronized (self) {
        pts = previousPts = currentPts;
    }

    return (currentAudioBuffer->mData) ? currentPts : 0;
}

- (void *)getCurrentAudioBufferData
{
    return currentAudioBuffer->mData;
}

- (UInt32)getCurrentTotalDataSize
{
    return currentAudioBuffer->mDataByteSize;
}

- (UInt32)getNumberOfSamples
{
    return numberOfSamples;
}

@end

/*****************************************************************************
 * Struct
 *****************************************************************************/

struct demux_sys_t {
    QTCaptureSession * session;
    QTCaptureDevice * audiodevice;
    VLCDecompressedAudioOutput * audiooutput;
    es_out_id_t *p_es_audio;
};

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    es_format_t audiofmt;
    char *psz_uid = NULL;
    int audiocodec;
    bool success;
    NSString *qtk_curraudiodevice_uid;
    NSArray *myAudioDevices, *audioformat_array;
    QTFormatDescription *audio_format;
    QTCaptureDeviceInput *audioInput;
    NSError *o_returnedAudioError;

    @autoreleasepool {
        if(p_demux->psz_location && *p_demux->psz_location)
            psz_uid = p_demux->psz_location;

        msg_Dbg(p_demux, "qtsound uid = %s", psz_uid);
        qtk_curraudiodevice_uid = [[NSString alloc] initWithFormat:@"%s", psz_uid];

        p_demux->p_sys = p_sys = calloc(1, sizeof(demux_sys_t));
        if(!p_sys)
            return VLC_ENOMEM;

        msg_Dbg(p_demux, "qtsound : uid = %s", [qtk_curraudiodevice_uid UTF8String]);
        myAudioDevices = [[[QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeSound]
                           arrayByAddingObjectsFromArray:[QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeMuxed]] retain];
        if([myAudioDevices count] == 0) {
            vlc_dialog_display_error(p_demux, _("No Audio Input device found"),
                _("Your Mac does not seem to be equipped with a suitable audio input device."
                "Please check your connectors and drivers."));
            msg_Err(p_demux, "Can't find any Audio device");

            goto error;
        }
        unsigned iaudio;
        for (iaudio = 0; iaudio < [myAudioDevices count]; iaudio++) {
            QTCaptureDevice *qtk_audioDevice;
            qtk_audioDevice = [myAudioDevices objectAtIndex:iaudio];
            msg_Dbg(p_demux, "qtsound audio %u/%lu localizedDisplayName: %s uniqueID: %s",
                    iaudio, [myAudioDevices count],
                    [[qtk_audioDevice localizedDisplayName] UTF8String],
                    [[qtk_audioDevice uniqueID] UTF8String]);
            if ([[[qtk_audioDevice uniqueID] stringByTrimmingCharactersInSet:
                  [NSCharacterSet whitespaceCharacterSet]] isEqualToString:qtk_curraudiodevice_uid]) {
                msg_Dbg(p_demux, "Device found");
                break;
            }
        }

        audioInput = nil;
        if(iaudio < [myAudioDevices count])
            p_sys->audiodevice = [myAudioDevices objectAtIndex:iaudio];
        else {
            /* cannot find designated audio device, fall back to open default audio device */
            msg_Dbg(p_demux, "Cannot find designated uid audio device as %s. Fall back to open default audio device.", [qtk_curraudiodevice_uid UTF8String]);
            p_sys->audiodevice = [QTCaptureDevice defaultInputDeviceWithMediaType: QTMediaTypeSound];
        }
        if(!p_sys->audiodevice) {
            vlc_dialog_display_error(p_demux, _("No audio input device found"),
                _("Your Mac does not seem to be equipped with a suitable audio input device."
                "Please check your connectors and drivers."));
            msg_Err(p_demux, "Can't find any Audio device");

            goto error;
        }

        if(![p_sys->audiodevice open: &o_returnedAudioError]) {
            msg_Err(p_demux, "Unable to open the audio capture device (%ld)", [o_returnedAudioError code]);
            goto error;
        }

        if([p_sys->audiodevice isInUseByAnotherApplication] == YES) {
            msg_Err(p_demux, "default audio capture device is exclusively in use by another application");
            goto error;
        }
        audioInput = [[QTCaptureDeviceInput alloc] initWithDevice: p_sys->audiodevice];
        if(!audioInput) {
            msg_Err(p_demux, "can't create a valid audio capture input facility");
            goto error;
        } else
            msg_Dbg(p_demux, "created valid audio capture input facility");

        p_sys->audiooutput = [[VLCDecompressedAudioOutput alloc] initWithDemux:p_demux];
        msg_Dbg (p_demux, "initialized audio output");

        /* Get the formats */
        /*
         FIXME: the format description gathered here does not seem to be the same
         in comparison to the format description collected from the actual sampleBuffer.
         This information needs to be updated some other place. For the time being this shall suffice.

         The following verbose output is an example of what is read from the input device during the below block
         [0x3042138] qtsound demux debug: Audio localized format summary: Linear PCM, 24 bit little-endian signed integer, 2 channels, 44100 Hz
         [0x3042138] qtsound demux debug: Sample Rate: 44100; Format ID: lpcm; Format Flags: 00000004; Bytes per Packet: 8; Frames per Packet: 1; Bytes per Frame: 8; Channels per Frame: 2; Bits per Channel: 24
         [0x3042138] qtsound demux debug: Flag float 0 bigEndian 0 signedInt 1 packed 0 alignedHigh 0 non interleaved 0 non mixable 0
         canonical 0 nativeFloatPacked 0 nativeEndian 0

         However when reading this information from the sampleBuffer during the delegate call from
         - (void)outputAudioSampleBuffer:(QTSampleBuffer *)sampleBuffer fromConnection:(QTCaptureConnection *)connection;
         the following data shows up
         2011-09-23 22:06:03.077 VLC[23070:f103] Audio localized format summary: Linear PCM, 32 bit little-endian floating point, 2 channels, 44100 Hz
         2011-09-23 22:06:03.078 VLC[23070:f103] Sample Rate: 44100; Format ID: lpcm; Format Flags: 00000029; Bytes per Packet: 4; Frames per Packet: 1; Bytes per Frame: 4; Channels per Frame: 2; Bits per Channel: 32
         2011-09-23 22:06:03.078 VLC[23070:f103] Flag float 1 bigEndian 0 signedInt 0 packed 1 alignedHigh 0 non interleaved 1 non mixable 0
         canonical 1 nativeFloatPacked 1 nativeEndian 0

         Note the differences
         24bit vs. 32bit
         little-endian signed integer vs. little-endian floating point
         format flag 00000004 vs. 00000029
         bytes per packet 8 vs. 4
         packed 0 vs. 1
         non interleaved 0 vs. 1 -> this makes a major difference when filling our own buffer
         canonical 0 vs. 1
         nativeFloatPacked 0 vs. 1

         One would assume we'd need to feed the (es_format_t)audiofmt with the data collected here.
         This is not the case. Audio will be transmitted in artefacts, due to wrong information.

         At the moment this data is set manually, however one should consider trying to set this data dynamically
         */
        audioformat_array = [p_sys->audiodevice formatDescriptions];
        audio_format = NULL;
        for(int k = 0; k < [audioformat_array count]; k++) {
            audio_format = (QTFormatDescription *)[audioformat_array objectAtIndex:k];

            msg_Dbg(p_demux, "Audio localized format summary: %s", [[audio_format localizedFormatSummary] UTF8String]);
            msg_Dbg(p_demux, "Audio format description attributes: %s",[[[audio_format formatDescriptionAttributes] description] UTF8String]);

            AudioStreamBasicDescription asbd = {0};
            NSValue *asbdValue =  [audio_format attributeForKey:QTFormatDescriptionAudioStreamBasicDescriptionAttribute];
            [asbdValue getValue:&asbd];

            char formatIDString[5];
            UInt32 formatID = CFSwapInt32HostToBig (asbd.mFormatID);
            bcopy (&formatID, formatIDString, 4);
            formatIDString[4] = '\0';

            /* kept for development purposes */
#if 0
            msg_Dbg(p_demux, "Sample Rate: %.0lf; Format ID: %s; Format Flags: %.8x; Bytes per Packet: %d; Frames per Packet: %d; Bytes per Frame: %d; Channels per Frame: %d; Bits per Channel: %d",
                    asbd.mSampleRate,
                    formatIDString,
                    asbd.mFormatFlags,
                    asbd.mBytesPerPacket,
                    asbd.mFramesPerPacket,
                    asbd.mBytesPerFrame,
                    asbd.mChannelsPerFrame,
                    asbd.mBitsPerChannel);

            msg_Dbg(p_demux, "Flag float %d bigEndian %d signedInt %d packed %d alignedHigh %d non interleaved %d non mixable %d\ncanonical %d nativeFloatPacked %d nativeEndian %d",
                    (asbd.mFormatFlags & kAudioFormatFlagIsFloat) != 0,
                    (asbd.mFormatFlags & kAudioFormatFlagIsBigEndian) != 0,
                    (asbd.mFormatFlags & kAudioFormatFlagIsSignedInteger) != 0,
                    (asbd.mFormatFlags & kAudioFormatFlagIsPacked) != 0,
                    (asbd.mFormatFlags & kAudioFormatFlagIsAlignedHigh) != 0,
                    (asbd.mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0,
                    (asbd.mFormatFlags & kAudioFormatFlagIsNonMixable) != 0,

                    (asbd.mFormatFlags & kAudioFormatFlagsCanonical) != 0,
                    (asbd.mFormatFlags & kAudioFormatFlagsNativeFloatPacked) != 0,
                    (asbd.mFormatFlags & kAudioFormatFlagsNativeEndian) != 0
                    );
#endif
        }

        if([audioformat_array count])
            audio_format = [audioformat_array objectAtIndex:0];
        else
            goto error;

        /* Now we can init */
        audiocodec = VLC_CODEC_FL32;
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

        p_sys->session = [[QTCaptureSession alloc] init];

        success = [p_sys->session addInput:audioInput error: &o_returnedAudioError];
        if(!success) {
            msg_Err(p_demux, "the audio capture device could not be added to capture session (%ld)", [o_returnedAudioError code]);
            goto error;
        }
        
        success = [p_sys->session addOutput:p_sys->audiooutput error: &o_returnedAudioError];
        if(!success) {
            msg_Err(p_demux, "audio output could not be added to capture session (%ld)", [o_returnedAudioError code]);
            goto error;
        }
        
        [p_sys->session startRunning];
        
        /* Set up p_demux */
        p_demux->pf_demux = Demux;
        p_demux->pf_control = Control;
        p_demux->info.i_update = 0;
        p_demux->info.i_title = 0;
        p_demux->info.i_seekpoint = 0;
        
        msg_Dbg(p_demux, "New audio es %d channels %dHz",
                audiofmt.audio.i_channels, audiofmt.audio.i_rate);
        
        p_sys->p_es_audio = es_out_Add(p_demux->out, &audiofmt);
        
        [audioInput release];
        
        msg_Dbg(p_demux, "QTSound: We have an audio device ready!");
        
        return VLC_SUCCESS;
    error:
        [audioInput release];
        
        free(p_sys);
        
        return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    @autoreleasepool {
        demux_t *p_demux = (demux_t*)p_this;
        demux_sys_t *p_sys = p_demux->p_sys;

        [p_sys->session performSelectorOnMainThread:@selector(stopRunning) withObject:nil waitUntilDone:NO];
        [p_sys->audiooutput performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];
        [p_sys->session performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];

        free(p_sys);
    }
}

/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux(demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_blocka = nil;

    @autoreleasepool {
        @synchronized (p_sys->audiooutput) {
            if ([p_sys->audiooutput checkCurrentAudioBuffer]) {
                unsigned i_buffer_size = [p_sys->audiooutput getCurrentTotalDataSize];
                p_blocka = block_Alloc(i_buffer_size);

                if(!p_blocka) {
                    msg_Err(p_demux, "cannot get audio block");
                    return 0;
                }

                memcpy(p_blocka->p_buffer, [p_sys->audiooutput getCurrentAudioBufferData], i_buffer_size);
                p_blocka->i_nb_samples = [p_sys->audiooutput getNumberOfSamples];
                p_blocka->i_pts = [p_sys->audiooutput getCurrentPts];
                
                [p_sys->audiooutput freeAudioMem];
            }
        }
    }

    if (p_blocka) {
        if (!p_blocka->i_pts) {
            block_Release(p_blocka);

            // Nothing to transfer yet, just forget
            msleep(10000);
            return 1;
        }

        es_out_SetPCR(p_demux->out, p_blocka->i_pts);
        es_out_Send(p_demux->out, p_sys->p_es_audio, p_blocka);
    }

    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control(demux_t *p_demux, int i_query, va_list args)
{
    bool *pb;
    int64_t *pi64;

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
            pi64 = (int64_t*)va_arg(args, int64_t *);
            *pi64 = INT64_C(1000) * var_InheritInteger(p_demux, "live-caching");
            return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
    }
    return VLC_EGENERIC;
}
