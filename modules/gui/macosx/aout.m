/*****************************************************************************
 * aout.m: CoreAudio output plugin
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: aout.m,v 1.4 2002/08/14 00:43:52 massiot Exp $
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#include <Carbon/Carbon.h>
#include <CoreAudio/AudioHardware.h>
#include <CoreAudio/HostTime.h>
#include <AudioToolbox/AudioConverter.h>

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
static int      SetFormat       ( aout_instance_t *p_aout );
static void     Play            ( aout_instance_t *p_aout,
                                  aout_buffer_t *p_buffer );

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
    /* FIXME : be more clever in choosing from several devices */
    i_param_size = sizeof( p_sys->device );
    err = AudioHardwareGetProperty( kAudioHardwarePropertyDefaultOutputDevice,
                                    &i_param_size, 
                                    (void *)&p_sys->device );
    if( err != noErr ) 
    {
        msg_Err( p_aout, "failed to get the device: %d", err );
        return( -1 );
    }

    p_aout->output.pf_setformat = SetFormat;
    p_aout->output.pf_play = Play;

    return 0;
}

/*****************************************************************************
 * SetFormat: find the closest available format from p_format
 *****************************************************************************/
static int SetFormat( aout_instance_t * p_aout )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    OSErr err;

    /* Get a description of the data format used by the device */
    UInt32 i_param_size = sizeof( p_sys->stream_format ); 
    err = AudioDeviceGetProperty( p_sys->device, 0, false, 
                                  kAudioDevicePropertyStreamFormat, 
                                  &i_param_size,
                                  &p_sys->stream_format );
    if( err != noErr )
    {
        msg_Err( p_aout, "failed to get stream format: %d", err );
        return -1 ;
    }

    if( p_sys->stream_format.mFormatID != kAudioFormatLinearPCM )
    {
        msg_Err( p_aout, "kAudioFormatLinearPCM required" );
        return -1 ;
    }

    /* We only deal with floats */
    if ( p_aout->output.output.i_format != AOUT_FMT_FLOAT32 )
    {
        msg_Err( p_aout, "cannot set format 0x%x",
                 p_aout->output.output.i_format );
        return -1;
    }
    p_sys->stream_format.mFormatFlags |=
        kLinearPCMFormatFlagIsFloat;

    /* Set sample rate and channels per frame */
    p_sys->stream_format.mSampleRate
                 = p_aout->output.output.i_rate; 
    p_sys->stream_format.mChannelsPerFrame
                 = p_aout->output.output.i_channels;

    /* Get the buffer size that the device uses for IO */
    i_param_size = sizeof( p_sys->i_buffer_size );
#if 0
    err = AudioDeviceGetProperty( p_sys->device, 0, false, 
                                  kAudioDevicePropertyBufferSize, 
                                  &i_param_size, &p_sys->i_buffer_size );
msg_Dbg( p_aout, "toto : %d", p_sys->i_buffer_size );
#else
    p_sys->i_buffer_size = sizeof(float) * p_aout->output.output.i_channels
                            * 4096;
    err = AudioDeviceSetProperty( p_sys->device, 0, 0, false,
                                  kAudioDevicePropertyBufferSize,
                                  i_param_size, &p_sys->i_buffer_size );
#endif
    if( err != noErr )
    {
        msg_Err( p_aout, "failed to set device buffer size: %d", err );
        return( -1 );
    }

    p_aout->output.i_nb_samples = p_sys->i_buffer_size / sizeof(float)
                                   / p_aout->output.output.i_channels;

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
    p_sys->clock_diff = mdate()
         - AudioConvertHostTimeToNanos(AudioGetCurrentHostTime()) / 1000;

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
 * Play: queue a buffer for playing by IOCallback
 *****************************************************************************/
static void Play( aout_instance_t * p_aout, aout_buffer_t * p_buffer )
{
    aout_FifoPush( p_aout, &p_aout->output.fifo, p_buffer );
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

    p_buffer = aout_OutputNextBuffer( p_aout, current_date, 0 );

    /* move data into output data buffer */
    if ( p_buffer != NULL )
    {
        BlockMoveData( p_buffer->p_buffer,
                       outOutputData->mBuffers[ 0 ].mData, 
                       p_sys->i_buffer_size );
        aout_BufferFree( p_buffer );
    }
    else
    {
        memset(outOutputData->mBuffers[ 0 ].mData, 0, p_sys->i_buffer_size);
    }

    return noErr;     
}

