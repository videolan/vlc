/*****************************************************************************
 * aout.m: CoreAudio output plugin
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: aout.m,v 1.16 2002/11/28 23:24:15 massiot Exp $
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
    err = AudioDeviceGetProperty(p_sys->device, 0, false, kAudioDevicePropertyStreamFormat,
				 &i_param_size, &p_sys->stream_format );
    if( err != noErr )
    {
        msg_Err( p_aout, "failed to get stream format: %4.4s", &err );
        return -1 ;
    }

    /* Now we know the sample rate of the device */
    p_aout->output.output.i_rate = p_sys->stream_format.mSampleRate;

    msg_Dbg( p_aout, "mSampleRate %ld, mFormatID %4.4s, mFormatFlags %ld, mBytesPerPacket %ld, mFramesPerPacket %ld, mBytesPerFrame %ld, mChannelsPerFrame %ld, mBitsPerChannel %ld",
           (UInt32)p_sys->stream_format.mSampleRate, &p_sys->stream_format.mFormatID,
           p_sys->stream_format.mFormatFlags, p_sys->stream_format.mBytesPerPacket,
           p_sys->stream_format.mFramesPerPacket, p_sys->stream_format.mBytesPerFrame,
           p_sys->stream_format.mChannelsPerFrame, p_sys->stream_format.mBitsPerChannel );

    msg_Dbg( p_aout, "vlc format %4.4s, mac output format '%4.4s'",
             (char *)&p_aout->output.output.i_format, &p_sys->stream_format.mFormatID );
    
    /* Get the buffer size that the device uses for IO */
    // If we do PCM, use the device's given buffer size
    // If we do raw AC3, we could use the devices given size too
    // If we do AC3 over SPDIF, force the size of one AC3 frame
    // (I think we need to do that because of the packetizer)
    i_param_size = sizeof( p_sys->i_buffer_size );
    err = AudioDeviceGetProperty( p_sys->device, 0, false, 
                                  kAudioDevicePropertyBufferSize, 
                                  &i_param_size, &p_sys->i_buffer_size );
    if(err) {
	msg_Err(p_aout, "failed to get buffer size - err %4.4s, device %ld", &err, p_sys->device);
	return -1;
    }
    else msg_Dbg( p_aout, "native buffer Size: %d", p_sys->i_buffer_size );
    
    if((p_sys->stream_format.mFormatID==kAudioFormat60958AC3
       || p_sys->stream_format.mFormatID=='IAC3')
       && p_sys->i_buffer_size != AOUT_SPDIF_SIZE)
    {
	p_sys->i_buffer_size = AOUT_SPDIF_SIZE;
	i_param_size = sizeof( p_sys->i_buffer_size );
	err = AudioDeviceSetProperty( p_sys->device, 0, 0, false,
			       kAudioDevicePropertyBufferSize,
			       i_param_size, &p_sys->i_buffer_size );
	if( err != noErr )
	{
	    msg_Err( p_aout, "failed to set device buffer size: %4.4s", &err );
	    return -1;
	}
	else msg_Dbg(p_aout, "bufferSize set to %d", p_sys->i_buffer_size);
    };


    // We now know the buffer size in bytes. Set the values for the vlc converters.
    switch(p_sys->stream_format.mFormatID)
    {
	case 0:
	case kAudioFormatLinearPCM:
	    p_aout->output.output.i_format = VLC_FOURCC('f','l','3','2');
	    if ( p_sys->stream_format.mChannelsPerFrame < 6 )
		p_aout->output.output.i_physical_channels
		    = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
	    else
		p_aout->output.output.i_physical_channels
		    = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
		    | AOUT_CHAN_CENTER | AOUT_CHAN_REARRIGHT
		    | AOUT_CHAN_REARLEFT | AOUT_CHAN_LFE;

	    p_aout->output.i_nb_samples = p_sys->i_buffer_size / p_sys->stream_format.mBytesPerFrame;
	    break;

	case kAudioFormat60958AC3:
	case 'IAC3':
	    p_aout->output.output.i_format = VLC_FOURCC('s','p','d','i');
	    msg_Dbg(p_aout, "phychan %d, ochan %d, bytes/fr %d, frlen %d",
	     p_aout->output.output.i_physical_channels,
	     p_aout->output.output.i_original_channels,
	     p_aout->output.output.i_bytes_per_frame,
	     p_aout->output.output.i_frame_length);

	    p_aout->output.output.i_bytes_per_frame = AOUT_SPDIF_SIZE; //p_sys->stream_format.mBytesPerFrame;
	    p_aout->output.output.i_frame_length = A52_FRAME_NB; //p_sys->stream_format.mFramesPerPacket;
	    p_aout->output.i_nb_samples = p_aout->output.output.i_frame_length;
	    
	    
	     //Probably not needed after all
	    
	    // Some more settings to make the SPDIF device work... Other SPDIF Devices might need additional
	    // values here. But don't change these, in order to not break existing devices. Either add values
	    // which are not being set here, or check if the SetProperty was successful, and try another version
	    // if not.
//	    p_sys->stream_format.mBytesPerFrame=4;	// usually set to 0 for AC3 by the system
//	    p_sys->stream_format.mFormatFlags|=kAudioFormatFlagIsBigEndian;
//		+kAudioFormatFlagIsPacked
//		+kAudioFormatFlagIsNonInterleaved;
//	    p_sys->stream_format.mBytesPerPacket=6144;
//	    p_sys->stream_format.mFramesPerPacket=1536;
	    
	    break;

	default:
	    msg_Err( p_aout, "Unknown hardware format '%4.4s'. Go ask Heiko.", &p_sys->stream_format.mFormatID );
	    return -1;
    }


    // Now tell the device how many sample frames to expect in each buffer
    i_param_size=sizeof(p_aout->output.i_nb_samples);
#if 0
    err = AudioDeviceGetProperty( p_sys->device, 0, false,
                                  kAudioDevicePropertyBufferFrameSize,
                                  &i_param_size, &p_aout->output.i_nb_samples);
    if(err) {
	msg_Err(p_aout, "failed to get BufferFrameSize - err %4.4s, device %ld", &err, p_sys->device);
	return -1;
    }
    else msg_Dbg( p_aout, "native BufferFrameSize: %d", p_aout->output.i_nb_samples);
#else
    err = AudioDeviceSetProperty( p_sys->device, 0, 0, false,
                                  kAudioDevicePropertyBufferFrameSize,
                                  i_param_size, &p_aout->output.i_nb_samples);
    if( err != noErr )
    {
        msg_Err( p_aout, "failed to set BufferFrameSize: %4.4s", &err );
        return -1;
    }
    else msg_Dbg(p_aout, "bufferFrameSize set to %d", p_aout->output.i_nb_samples);
#endif

/*    
    // And set the device format, since we might have changed some of it above
    i_param_size = sizeof(AudioStreamBasicDescription);
    err = AudioDeviceSetProperty(p_sys->device, 0, 0, false, kAudioDevicePropertyStreamFormat,
				 i_param_size, &p_sys->stream_format );
    if( err != noErr )
    {
        msg_Err( p_aout, "failed to set stream format: %4.4s", &err );
        return -1 ;
    }
    else
	msg_Dbg( p_aout, "set: mSampleRate %ld, mFormatID %4.4s, mFormatFlags %ld, mBytesPerPacket %ld, mFramesPerPacket %ld, mBytesPerFrame %ld, mChannelsPerFrame %ld, mBitsPerChannel %ld",
	  (UInt32)p_sys->stream_format.mSampleRate, &p_sys->stream_format.mFormatID,
	  p_sys->stream_format.mFormatFlags, p_sys->stream_format.mBytesPerPacket,
	  p_sys->stream_format.mFramesPerPacket, p_sys->stream_format.mBytesPerFrame,
	  p_sys->stream_format.mChannelsPerFrame, p_sys->stream_format.mBitsPerChannel );
  */  
    
    
    /* Add callback */
    err = AudioDeviceAddIOProc( p_sys->device,
                                (AudioDeviceIOProc)IOCallback,
                                (void *)p_aout );
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioDeviceAddIOProc failed: %4.4s", &err );
	return -1;
    }
    
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
        msg_Err( p_aout, "AudioDeviceStop failed: %4.4s", &err );
    }

    err = AudioDeviceRemoveIOProc( p_sys->device,
                                (AudioDeviceIOProc)IOCallback );
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioDeviceRemoveIOProc failed: %4.4s", &err );
    }

    free( p_sys );
}

/*****************************************************************************
 * Play: nothing to do
 *****************************************************************************/
static void Play( aout_instance_t * p_aout )
{
}

#include <syslog.h>
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

    p_buffer = aout_OutputNextBuffer( p_aout, current_date, (p_aout->output.output.i_format == VLC_FOURCC('s','p','d','i')) );

    /* move data into output data buffer */
    if ( p_buffer != NULL )
    {
	BlockMoveData( p_buffer->p_buffer,
                       outOutputData->mBuffers[ 0 ].mData, 
                       p_sys->i_buffer_size );
//	syslog(LOG_INFO, "convert: %08lX %08lX %08lX", ((long*)p_buffer->p_buffer)[0], ((long*)p_buffer->p_buffer)[1], ((long*)p_buffer->p_buffer)[2]);
	aout_BufferFree( p_buffer );
    }
    else
    {
        memset(outOutputData->mBuffers[ 0 ].mData, 0, p_sys->i_buffer_size);
    }


//    outOutputData->mBuffers[0].mDataByteSize=p_sys->i_buffer_size;
    
    return noErr;     
}

