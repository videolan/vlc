/*****************************************************************************
 * aout.m: CoreAudio output plugin
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: aout.m,v 1.14 2002/11/14 22:38:48 massiot Exp $
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Heiko Panther <heiko.panther@web.de>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <string.h>
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include "aout_internal.h"
#include "asystm.h"

#include <Carbon/Carbon.h>
#include <CoreAudio/AudioHardware.h>
#include <CoreAudio/HostTime.h>
#include <AudioToolbox/AudioConverter.h>

#define A52_FRAME_NB 1536

/*****************************************************************************
 * aout_sys_t: private audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the CoreAudio specific properties of an output thread.
 *****************************************************************************/
struct aout_sys_t
{
    AudioDeviceID       device;         // the audio device

    AudioStreamBasicDescription stream_format;

    UInt32              i_buffer_size;  // audio device buffer size
    mtime_t             clock_diff;
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void     Play            ( aout_instance_t *p_aout );

static OSStatus IOCallback      ( AudioDeviceID inDevice,
                                  const AudioTimeStamp *inNow, 
                                  const void *inInputData, 
                                  const AudioTimeStamp *inInputTime,
                                  AudioBufferList *outOutputData, 
                                  const AudioTimeStamp *inOutputTime, 
                                  void *threadGlobals );

/*****************************************************************************
 * Open: open a CoreAudio HAL device
 *****************************************************************************/
extern MacOSXAudioSystem *gTheMacOSXAudioSystem; // Remove this global, access audio system froma aout some other way

int E_(OpenAudio)( vlc_object_t * p_this )
{
    OSStatus err;
    UInt32 i_param_size;
    aout_instance_t * p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys;
    
    /* Allocate instance */
    p_sys = p_aout->output.p_sys = malloc( sizeof( struct aout_sys_t ) );
    memset( p_sys, 0, sizeof( struct aout_sys_t ) );
    if( p_aout->output.p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return( 1 );
    }

    /* Get the default output device */
    // We now ask the GUI for the selected device
    p_sys->device=[gTheMacOSXAudioSystem getSelectedDeviceSetToRate:p_aout->output.output.i_rate];
    if(p_sys->device==0)
    {
        msg_Err( p_aout, "couldn't get output device");
        return( -1 );
    }
    msg_Dbg(p_aout, "device returned: %ld", p_sys->device);

    p_aout->output.pf_play = Play;
    aout_VolumeSoftInit( p_aout );

    /* Get a description of the data format used by the device */
    i_param_size = sizeof(AudioStreamBasicDescription); 
    err = AudioDeviceGetProperty(p_sys->device, 0, false, kAudioDevicePropertyStreamFormat, &i_param_size, &p_sys->stream_format );
    if( err != noErr )
    {
        msg_Err( p_aout, "failed to get stream format: %4.4s", &err );
        return -1 ;
    }

    msg_Dbg( p_aout, "mSampleRate %ld, mFormatID %4.4s, mFormatFlags %ld, mBytesPerPacket %ld, mFramesPerPacket %ld, mBytesPerFrame %ld, mChannelsPerFrame %ld, mBitsPerChannel %ld",
           (UInt32)p_sys->stream_format.mSampleRate, &p_sys->stream_format.mFormatID,
           p_sys->stream_format.mFormatFlags, p_sys->stream_format.mBytesPerPacket,
           p_sys->stream_format.mFramesPerPacket, p_sys->stream_format.mBytesPerFrame,
           p_sys->stream_format.mChannelsPerFrame, p_sys->stream_format.mBitsPerChannel );

    msg_Dbg( p_aout, "vlc format %4.4s, mac output format '%4.4s'",
             (char *)&p_aout->output.output.i_format, &p_sys->stream_format.mFormatID );

    switch(p_sys->stream_format.mFormatID)
    {
    case 0:
    case kAudioFormatLinearPCM:
        p_aout->output.output.i_format = VLC_FOURCC('f','l','3','2');
        if ( p_sys->stream_format.mChannelsPerFrame < 6 )
            p_aout->output.output.i_channels = AOUT_CHAN_STEREO;
        else
            p_aout->output.output.i_channels = AOUT_CHAN_3F2R | AOUT_CHAN_LFE;
        break;

    case kAudioFormat60958AC3:
    case 'IAC3':
        p_aout->output.output.i_format = VLC_FOURCC('s','p','d','i');
        //not necessary, use the input's format by default --Meuuh
        //p_aout->output.output.i_channels = AOUT_CHAN_DOLBY | AOUT_CHAN_LFE;
        p_aout->output.output.i_bytes_per_frame = AOUT_SPDIF_SIZE; //p_sys->stream_format.mBytesPerFrame;
        p_aout->output.output.i_frame_length = A52_FRAME_NB; //p_sys->stream_format.mFramesPerPacket;
        break;

    default:
        msg_Err( p_aout, "Unknown hardware format '%4.4s'. Go ask Heiko.", &p_sys->stream_format.mFormatID );
        return -1;
    }

    /* Set sample rate and channels per frame */
    p_aout->output.output.i_rate = p_sys->stream_format.mSampleRate;

    /* Get the buffer size that the device uses for IO */
    i_param_size = sizeof( p_sys->i_buffer_size );
#if 1	// i have a feeling we should use the buffer size imposed by the AC3 device (usually about 6144)
    err = AudioDeviceGetProperty( p_sys->device, 1, false, 
                                  kAudioDevicePropertyBufferSize, 
                                  &i_param_size, &p_sys->i_buffer_size );
    if(err) {
	msg_Err(p_aout, "failed to get buffer size - err %4.4s, device %ld", &err, p_sys->device);
	return -1;
    }
    else msg_Dbg( p_aout, "native buffer Size: %d", p_sys->i_buffer_size );
#else
    p_sys->i_buffer_size = p_aout->output.output.i_bytes_per_frame;
    err = AudioDeviceSetProperty( p_sys->device, 0, 1, false,
                                  kAudioDevicePropertyBufferSize,
                                  i_param_size, &p_sys->i_buffer_size );
    if( err != noErr )
    {
        msg_Err( p_aout, "failed to set device buffer size: %4.4s", err );
        return( -1 );
    }
    else msg_Dbg(p_aout, "bufferSize set to %d", p_sys->i_buffer_size);
#endif

    p_aout->output.i_nb_samples = p_sys->i_buffer_size / p_sys->stream_format.mBytesPerFrame;

    /* Add callback */
    err = AudioDeviceAddIOProc( p_sys->device,
                                (AudioDeviceIOProc)IOCallback,
                                (void *)p_aout );

    /* Open the output with callback IOCallback */
    err = AudioDeviceStart( p_sys->device,
                            (AudioDeviceIOProc)IOCallback );
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioDeviceStart failed: %d", err );
        return -1;
    }

    /* Let's pray for the following operation to be atomic... */
    p_sys->clock_diff = - (mtime_t)AudioConvertHostTimeToNanos(
                                 AudioGetCurrentHostTime()) / 1000;
    p_sys->clock_diff += mdate();

    return 0;
}

/*****************************************************************************
 * Close: close the CoreAudio HAL device
 *****************************************************************************/
void E_(CloseAudio)( aout_instance_t * p_aout )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    OSStatus err; 

    /* Stop playing sound through the device */
    err = AudioDeviceStop( p_sys->device,
                           (AudioDeviceIOProc)IOCallback ); 
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioDeviceStop failed: %d", err );
    }

    free( p_sys );
}

/*****************************************************************************
 * Play: nothing to do
 *****************************************************************************/
static void Play( aout_instance_t * p_aout )
{
}

/*****************************************************************************
 * IOCallback : callback for audio output
 *****************************************************************************/
static OSStatus IOCallback( AudioDeviceID inDevice,
                            const AudioTimeStamp *inNow, 
                            const void *inInputData,
                            const AudioTimeStamp *inInputTime, 
                            AudioBufferList *outOutputData,
                            const AudioTimeStamp *inOutputTime, 
                            void *threadGlobals )
{
    aout_instance_t * p_aout = (aout_instance_t *)threadGlobals;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    mtime_t         current_date;
    AudioTimeStamp  host_time;
    aout_buffer_t * p_buffer;

    host_time.mFlags = kAudioTimeStampHostTimeValid;
    AudioDeviceTranslateTime( inDevice, inOutputTime, &host_time );
    current_date = p_sys->clock_diff
                 + AudioConvertHostTimeToNanos(host_time.mHostTime) / 1000;

//    msg_Dbg(p_aout, "Now fetching audio data");
    p_buffer = aout_OutputNextBuffer( p_aout, current_date, (p_aout->output.output.i_format == VLC_FOURCC('s','p','d','i')) );

    /* move data into output data buffer */
    if ( p_buffer != NULL )
    {
	BlockMoveData( p_buffer->p_buffer,
                       outOutputData->mBuffers[ 0 ].mData, 
                       p_sys->i_buffer_size );

//	msg_Dbg(p_aout, "This buffer has %d bytes, i take %d", p_buffer->i_nb_bytes, p_sys->i_buffer_size);
    
	aout_BufferFree( p_buffer );
    }
    else
    {
        memset(outOutputData->mBuffers[ 0 ].mData, 0, p_sys->i_buffer_size);
    }

    return noErr;     
}

