/*****************************************************************************
 * aout_macosx.c : CoreAudio output plugin
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: aout_macosx.c,v 1.16 2002/03/22 00:47:47 jlj Exp $
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
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

#include <videolan/vlc.h>

#include "audio_output.h"                                   /* aout_thread_t */

#include <Carbon/Carbon.h>
#include <CoreAudio/AudioHardware.h>
#include <AudioToolbox/AudioConverter.h>

/*****************************************************************************
 * aout_sys_t: private audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the CoreAudio specific properties of an output thread.
 *****************************************************************************/
typedef struct aout_sys_s
{
    AudioDeviceID       device;         // the audio device
    AudioConverterRef   s_converter;    // the AudioConverter
    int                 b_format;       // format begun 

    AudioStreamBasicDescription s_src_stream_format;
    AudioStreamBasicDescription s_dst_stream_format;

    Ptr                 p_buffer;       // ptr to the 32 bit float data
    UInt32              ui_buffer_size; // audio device buffer size
    boolean_t           b_buffer_data;  // available buffer data?
    vlc_mutex_t         mutex_lock;     // pthread locks for sync of
    vlc_cond_t          cond_sync;      // aout_Play and callback
} aout_sys_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int      aout_Open       ( aout_thread_t *p_aout );
static int      aout_SetFormat  ( aout_thread_t *p_aout );
static int      aout_GetBufInfo ( aout_thread_t *p_aout, int i_buffer_info );
static void     aout_Play       ( aout_thread_t *p_aout,
                                  byte_t *buffer, int i_size );
static void     aout_Close      ( aout_thread_t *p_aout );

static int      CABeginFormat   ( aout_thread_t *p_aout );
static int      CAEndFormat     ( aout_thread_t *p_aout );

static OSStatus CAIOCallback    ( AudioDeviceID inDevice,
                                  const AudioTimeStamp *inNow, 
                                  const void *inInputData, 
                                  const AudioTimeStamp *inInputTime,
                                  AudioBufferList *outOutputData, 
                                  const AudioTimeStamp *inOutputTime, 
                                  void *threadGlobals );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( aout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->functions.aout.pf_open = aout_Open;
    p_function_list->functions.aout.pf_setformat = aout_SetFormat;
    p_function_list->functions.aout.pf_getbufinfo = aout_GetBufInfo;
    p_function_list->functions.aout.pf_play = aout_Play;
    p_function_list->functions.aout.pf_close = aout_Close;
}

/*****************************************************************************
 * aout_Open: opens a CoreAudio HAL device
 *****************************************************************************/
static int aout_Open( aout_thread_t *p_aout )
{
    OSStatus err;
    UInt32 ui_param_size;

    /* allocate instance */
    p_aout->p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->p_sys == NULL )
    {
        intf_ErrMsg( "aout error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    /* initialize members */
    memset( p_aout->p_sys, 0, sizeof( aout_sys_t ) );

    /* get the default output device */
    ui_param_size = sizeof( p_aout->p_sys->device );
    err = AudioHardwareGetProperty( kAudioHardwarePropertyDefaultOutputDevice,
                                    &ui_param_size, 
                                    (void *)&p_aout->p_sys->device );

    if( err != noErr ) 
    {
        intf_ErrMsg( "aout error: failed to get the device: %d", err );
        return( -1 );
    }

    /* get the buffer size that the device uses for IO */
    ui_param_size = sizeof( p_aout->p_sys->ui_buffer_size );
    err = AudioDeviceGetProperty( p_aout->p_sys->device, 0, false, 
                                  kAudioDevicePropertyBufferSize, 
                                  &ui_param_size,
                                  &p_aout->p_sys->ui_buffer_size );

    if( err != noErr )
    {
        intf_ErrMsg( "aout error: failed to get device buffer size: %d", err );
        return( -1 );
    }

    /* get a description of the data format used by the device */
    ui_param_size = sizeof( p_aout->p_sys->s_dst_stream_format ); 
    err = AudioDeviceGetProperty( p_aout->p_sys->device, 0, false, 
                                  kAudioDevicePropertyStreamFormat, 
                                  &ui_param_size,
                                  &p_aout->p_sys->s_dst_stream_format );

    if( err != noErr )
    {
        intf_ErrMsg( "aout error: failed to get dst stream format: %d", err );
        return( -1 );
    }

    if( p_aout->p_sys->s_dst_stream_format.mFormatID != kAudioFormatLinearPCM )
    {
        intf_ErrMsg( "aout error: kAudioFormatLinearPCM required" );
        return( -1 );
    }

    /* initialize mutex and cond */
    vlc_mutex_init( &p_aout->p_sys->mutex_lock );
    vlc_cond_init( &p_aout->p_sys->cond_sync );

    /* initialize source stream format */
    memcpy( &p_aout->p_sys->s_src_stream_format,
            &p_aout->p_sys->s_dst_stream_format,
            sizeof( p_aout->p_sys->s_src_stream_format ) );

    if( CABeginFormat( p_aout ) )
    {
        intf_ErrMsg( "aout error: CABeginFormat failed" );
        return( -1 );
    }

    return( 0 );
}

/*****************************************************************************
 * aout_SetFormat: pretends to set the dsp output format
 *****************************************************************************/
static int aout_SetFormat( aout_thread_t *p_aout )
{
    if( CAEndFormat( p_aout ) )
    {
        intf_ErrMsg( "aout error: CAEndFormat failed" );
        return( -1 );
    }

    switch( p_aout->i_format )
    {
        case AOUT_FMT_S8:
            intf_ErrMsg( "Audio format (Signed 8) not supported now,"
                         "please report stream" );
            return( -1 );
                    
        case AOUT_FMT_U8:
            intf_ErrMsg( "Audio format (Unsigned 8) not supported now,"
                         "please report stream" );
            return( -1 );

        case AOUT_FMT_S16_LE:
            p_aout->p_sys->s_src_stream_format.mFormatFlags &=
                ~kLinearPCMFormatFlagIsBigEndian;
            p_aout->p_sys->s_src_stream_format.mFormatFlags |=
                kLinearPCMFormatFlagIsSignedInteger;
            break;

        case AOUT_FMT_S16_BE:
            p_aout->p_sys->s_src_stream_format.mFormatFlags |=
                kLinearPCMFormatFlagIsBigEndian;
            p_aout->p_sys->s_src_stream_format.mFormatFlags |=
                kLinearPCMFormatFlagIsSignedInteger;
            break;

        case AOUT_FMT_U16_LE:
            p_aout->p_sys->s_src_stream_format.mFormatFlags &=
                ~kLinearPCMFormatFlagIsBigEndian;
            p_aout->p_sys->s_src_stream_format.mFormatFlags &=
                ~kLinearPCMFormatFlagIsSignedInteger;
            break;
                    
        case AOUT_FMT_U16_BE:
            p_aout->p_sys->s_src_stream_format.mFormatFlags |=
                kLinearPCMFormatFlagIsBigEndian;
            p_aout->p_sys->s_src_stream_format.mFormatFlags &=
                ~kLinearPCMFormatFlagIsSignedInteger;
            break;
                    
        default:
            intf_ErrMsg( "Audio format (0x%08x) not supported now,"
                         "please report stream", p_aout->i_format );
            return( -1 );
    }

    /* source format is not float */
    p_aout->p_sys->s_src_stream_format.mFormatFlags &=
        ~kLinearPCMFormatFlagIsFloat;

    /* if destination format is float, take size diff into account */
    if( p_aout->p_sys->s_dst_stream_format.mFormatFlags & 
        kLinearPCMFormatFlagIsFloat )
    {
        p_aout->p_sys->s_src_stream_format.mBytesPerPacket =
            p_aout->p_sys->s_dst_stream_format.mBytesPerPacket / 2;
        p_aout->p_sys->s_src_stream_format.mBytesPerFrame =
            p_aout->p_sys->s_src_stream_format.mBytesPerFrame / 2;
        p_aout->p_sys->s_src_stream_format.mBitsPerChannel =
            p_aout->p_sys->s_src_stream_format.mBitsPerChannel / 2;
    }

    /* set sample rate and channels per frame */
    p_aout->p_sys->s_src_stream_format.mSampleRate = p_aout->i_rate; 
    p_aout->p_sys->s_src_stream_format.mChannelsPerFrame = p_aout->i_channels;

    if( CABeginFormat( p_aout ) )
    {
        intf_ErrMsg( "aout error: CABeginFormat failed" );
        return( -1 );
    }

    return( 0 );
}

/*****************************************************************************
 * aout_GetBufInfo: returns available bytes in buffer
 *****************************************************************************/
static int aout_GetBufInfo( aout_thread_t *p_aout, int i_buffer_limit )
{
    return( 0 ); /* send data as soon as possible */
}

/*****************************************************************************
 * CAIOCallback : callback for audio output
 *****************************************************************************/
static OSStatus CAIOCallback( AudioDeviceID inDevice,
                              const AudioTimeStamp *inNow, 
                              const void *inInputData,
                              const AudioTimeStamp *inInputTime, 
                              AudioBufferList *outOutputData,
                              const AudioTimeStamp *inOutputTime, 
                              void *threadGlobals )
{
    aout_sys_t *p_sys = (aout_sys_t *)threadGlobals;

    /* see aout_Play below */
    vlc_mutex_lock( &p_sys->mutex_lock );
    vlc_cond_signal( &p_sys->cond_sync );
    
    /* move data into output data buffer */
    if( p_sys->b_buffer_data )
    {
        BlockMoveData( p_sys->p_buffer,
                       outOutputData->mBuffers[ 0 ].mData, 
                       p_sys->ui_buffer_size );

        p_sys->b_buffer_data = 0;
    }

    vlc_mutex_unlock( &p_sys->mutex_lock );

    return( noErr );     
}

/*****************************************************************************
 * aout_Play: plays a sound
 *****************************************************************************/
static void aout_Play( aout_thread_t *p_aout, byte_t *buffer, int i_size )
{
    OSStatus err;
    UInt32 ui_buffer_size = p_aout->p_sys->ui_buffer_size;

    err = AudioConverterConvertBuffer( p_aout->p_sys->s_converter,
                                       i_size, buffer,
                                       &ui_buffer_size,
                                       p_aout->p_sys->p_buffer );

    if( err != noErr )
    {
        intf_ErrMsg( "aout error: ConvertBuffer failed: %d", err );
    }
    else
    {
        p_aout->p_sys->b_buffer_data = 1;
    }

    /* 
     * wait for a callback to occur (to flush the buffer), so aout_Play
     * can't be called twice, losing the data we just wrote. 
     */
    vlc_mutex_lock( &p_aout->p_sys->mutex_lock );
    vlc_cond_wait( &p_aout->p_sys->cond_sync, &p_aout->p_sys->mutex_lock );
    vlc_mutex_unlock( &p_aout->p_sys->mutex_lock );
}

/*****************************************************************************
 * aout_Close: closes the CoreAudio HAL device
 *****************************************************************************/
static void aout_Close( aout_thread_t *p_aout )
{
    if( CAEndFormat( p_aout ) )
    {
        intf_ErrMsg( "aout error: CAEndFormat failed" );
    }

    /* destroy lock and cond */
    vlc_mutex_destroy( &p_aout->p_sys->mutex_lock );
    vlc_cond_destroy( &p_aout->p_sys->cond_sync );

    free( p_aout->p_sys );
}

/*****************************************************************************
 * CABeginFormat: creates an AudioConverter 
 *****************************************************************************/
static int CABeginFormat( aout_thread_t *p_aout )
{
    OSStatus err;
    UInt32 ui_param_size;

    if( p_aout->p_sys->b_format )
    {
        intf_ErrMsg( "aout error: CABeginFormat (b_format)" );
        return( 1 );
    }

    p_aout->p_sys->ui_buffer_size = 2 * 2 * sizeof(s16) * 
        ((s64)p_aout->i_rate * AOUT_BUFFER_DURATION) / 1000000; 

    /* set the buffer size that the device uses for IO */
    ui_param_size = sizeof( p_aout->p_sys->ui_buffer_size );
    err = AudioDeviceSetProperty( p_aout->p_sys->device, 0, 0, false, 
                                  kAudioDevicePropertyBufferSize, 
                                  ui_param_size,
                                  &p_aout->p_sys->ui_buffer_size );

    if( err != noErr )
    {
        intf_ErrMsg( "aout error: AudioDeviceSetProperty failed: %d", err );
        return( 1 );
    }

    /* allocate audio buffer */ 
    p_aout->p_sys->p_buffer = NewPtrClear( p_aout->p_sys->ui_buffer_size );

    if( p_aout->p_sys->p_buffer == nil )
    {
        intf_ErrMsg( "aout error: failed to allocate audio buffer" );
        return( 1 );
    }

    /* create a new AudioConverter */
    err = AudioConverterNew( &p_aout->p_sys->s_src_stream_format,
                             &p_aout->p_sys->s_dst_stream_format,
                             &p_aout->p_sys->s_converter );

    if( err != noErr )
    {
        intf_ErrMsg( "aout error: AudioConverterNew failed: %d", err );
        DisposePtr( p_aout->p_sys->p_buffer );
        return( 1 );
    }

    /* add callback */
    err = AudioDeviceAddIOProc( p_aout->p_sys->device, 
                                (AudioDeviceIOProc)CAIOCallback, 
                                (void *)p_aout->p_sys );

    if( err != noErr )
    {
        intf_ErrMsg( "aout error: AudioDeviceAddIOProc failed: %d", err );
        AudioConverterDispose( p_aout->p_sys->s_converter );
        DisposePtr( p_aout->p_sys->p_buffer );
        return( 1 );
    } 

    /* open the output */
    err = AudioDeviceStart( p_aout->p_sys->device,
                            (AudioDeviceIOProc)CAIOCallback );

    if( err != noErr )
    {
        intf_ErrMsg( "aout error: AudioDeviceStart failed: %d", err );
        AudioConverterDispose( p_aout->p_sys->s_converter );
        DisposePtr( p_aout->p_sys->p_buffer );
        return( 1 );
    }

    p_aout->p_sys->b_format = 1;

    return( 0 );
}

/*****************************************************************************
 * CAEndFormat: destroys the AudioConverter 
 *****************************************************************************/
static int CAEndFormat( aout_thread_t *p_aout )
{
    OSStatus err; 

    if( !p_aout->p_sys->b_format )
    {
        intf_ErrMsg( "aout error: CAEndFormat (!b_format)" );
        return( 1 );
    }

    /* stop playing sound through the device */
    err = AudioDeviceStop( p_aout->p_sys->device,
                           (AudioDeviceIOProc)CAIOCallback ); 

    if( err != noErr )
    {
        intf_ErrMsg( "aout error: AudioDeviceStop failed: %d", err );
        return( 1 );
    }

    /* remove the callback */
    err = AudioDeviceRemoveIOProc( p_aout->p_sys->device,
                                   (AudioDeviceIOProc)CAIOCallback ); 

    if( err != noErr )
    {
        intf_ErrMsg( "aout error: AudioDeviceRemoveIOProc failed: %d", err );
        return( 1 );
    }

    /* destroy the AudioConverter */
    err = AudioConverterDispose( p_aout->p_sys->s_converter );

    if( err != noErr )
    {
        intf_ErrMsg( "aout error: AudioConverterDispose failed: %d", err );
        return( 1 );
    }

    /* release audio buffer */
    DisposePtr( p_aout->p_sys->p_buffer );

    p_aout->p_sys->b_format = 0;

    return( 0 );
}
