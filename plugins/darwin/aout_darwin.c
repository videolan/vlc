/*****************************************************************************
 * aout_darwin.c : Darwin audio output plugin
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 *
 * Authors: 
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

#define MODULE_NAME darwin
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include "config.h"
#include "common.h"                                     /* boolean_t, byte_t */
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "audio_output.h"                                   /* aout_thread_t */

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */
#include "main.h"

#include "modules.h"

#include <Carbon/Carbon.h>
#include <CoreAudio/AudioHardware.h>

#include <sys/fcntl.h>

#define WRITE_AUDIO_OUTPUT_TO_FILE 0

/*****************************************************************************
 * aout_sys_t: private audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the Darwin specific properties of an output thread.
 *****************************************************************************/
typedef struct aout_sys_s
{
#if WRITE_AUDIO_OUTPUT_TO_FILE
    int           fd;                 // debug
#endif
    // unsigned long sizeOfDataInMemory; // size in bytes of the 32 bit float data stored in memory
    Ptr		        p_Data;	    // Ptr to the 32 bit float data stored in memory
    // Ptr		        currentDataLocationPtr;	// location of the next chunk of data to send to the HAL
    AudioDeviceID device;			            // the default device
    UInt32	      ui_deviceBufferSize;   // bufferSize returned by kAudioDevicePropertyBufferSize
    AudioStreamBasicDescription	deviceFormat; // info about the default device
    vlc_mutex_t   mutex_lock;
    vlc_cond_t    cond_sync;
} aout_sys_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int     aout_Probe       ( probedata_t *p_data );
static int     aout_Open        ( aout_thread_t *p_aout );
static int     aout_SetFormat   ( aout_thread_t *p_aout );
static long    aout_GetBufInfo  ( aout_thread_t *p_aout, long l_buffer_info );
static void    aout_Play        ( aout_thread_t *p_aout,
                                  byte_t *buffer, int i_size );
static void    aout_Close       ( aout_thread_t *p_aout );

OSStatus appIOProc( AudioDeviceID  inDevice, const AudioTimeStamp*  inNow, 
                    const void*  inInputData, const AudioTimeStamp*  inInputTime, 
                    AudioBufferList*  outOutputData, const AudioTimeStamp* inOutputTime, 
                    void* appGlobals );
void Convert16BitIntegerTo32Float( Ptr in16BitDataPtr, Ptr out32BitDataPtr, UInt32 totalBytes );
void Convert16BitIntegerTo32FloatWithByteSwap( Ptr in16BitDataPtr, Ptr out32BitDataPtr, UInt32 totalBytes );
void Convert8BitIntegerTo32Float( Ptr in8BitDataPtr, Ptr out32BitDataPtr, UInt32 totalBytes );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( aout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = aout_Probe;
    p_function_list->functions.aout.pf_open = aout_Open;
    p_function_list->functions.aout.pf_setformat = aout_SetFormat;
    p_function_list->functions.aout.pf_getbufinfo = aout_GetBufInfo;
    p_function_list->functions.aout.pf_play = aout_Play;
    p_function_list->functions.aout.pf_close = aout_Close;
}

/*****************************************************************************
 * aout_Probe: probe the audio device and return a score
 *****************************************************************************/
static int aout_Probe( probedata_t *p_data )
{
    if( TestMethod( AOUT_METHOD_VAR, "darwin" ) )
    {
        return( 999 );
    }

    /* The Darwin plugin always works under Darwin or MacOS X */
    return( 100 );
}

/*****************************************************************************
 * aout_Open: opens a HAL audio device
 *****************************************************************************/
static int aout_Open( aout_thread_t *p_aout )
{
    OSStatus        err = noErr;
    UInt32			    count, bufferSize;
    AudioDeviceID		device = kAudioDeviceUnknown;
    AudioStreamBasicDescription	format;

    /* Allocate structure */
    p_aout->p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->p_sys == NULL )
    {
        intf_ErrMsg("aout error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    /* Initialize some variables */
    p_aout->i_format   = AOUT_FORMAT_DEFAULT;
    p_aout->i_channels = 1 + main_GetIntVariable( AOUT_STEREO_VAR,
                                                  AOUT_STEREO_DEFAULT );
    p_aout->l_rate     =     main_GetIntVariable( AOUT_RATE_VAR,
                                                  AOUT_RATE_DEFAULT );
    p_aout->p_sys->device                 = kAudioDeviceUnknown;
    p_aout->p_sys->p_Data         = nil;
    // p_aout->p_sys->currentDataLocationPtr = nil;
    
    // get the default output device for the HAL
    // it is required to pass the size of the data to be returned
    count = sizeof( p_aout->p_sys->device );	
    err = AudioHardwareGetProperty( kAudioHardwarePropertyDefaultOutputDevice,  
                                    &count, (void *) &device);
    
    if( err == noErr) 
    {
        // get the buffersize that the default device uses for IO
        // it is required to pass the size of the data to be returned
        count = sizeof(p_aout->p_sys->ui_deviceBufferSize);	
        err = AudioDeviceGetProperty( device, 0, false, 
                                      kAudioDevicePropertyBufferSize, 
                                      &count, &bufferSize);
        if( err == noErr )
        {
            // get a description of the data format used by the default device
            // it is required to pass the size of the data to be returned
            count = sizeof(p_aout->p_sys->deviceFormat); 
            err = AudioDeviceGetProperty( device, 0, false, 
                                          kAudioDevicePropertyStreamFormat, 
                                          &count, &format);
            if( err == noErr )
            {
                if( format.mFormatID != kAudioFormatLinearPCM ) return paramErr;
            
                // everything is ok so fill in p_sys
                p_aout->p_sys->device           = device;
                p_aout->p_sys->ui_deviceBufferSize = bufferSize;
                p_aout->p_sys->deviceFormat     = format;
            }
        }
    }

    if (err != noErr) return err;

    p_aout->p_sys->ui_deviceBufferSize = 2 * 2 * sizeof(s16) 
                                        * ((s64)p_aout->l_rate * AOUT_BUFFER_DURATION) / 1000000; 
    // p_aout->p_sys->sizeOfDataInMemory = p_aout->p_sys->ui_deviceBufferSize; 

    p_aout->p_sys->p_Data = NewPtrClear( p_aout->p_sys->ui_deviceBufferSize );
    if( p_aout->p_sys->p_Data == nil ) return paramErr;

#if WRITE_AUDIO_OUTPUT_TO_FILE
    p_aout->p_sys->fd = open( "/Users/bofh/audio-darwin.pcm", O_RDWR|O_CREAT );
    intf_ErrMsg( "open(...) -> %d", p_aout->p_sys->fd );
#endif

    vlc_cond_init( &p_aout->p_sys->cond_sync );
    vlc_mutex_init( &p_aout->p_sys->mutex_lock );
    
    return( 0 );
}

/*****************************************************************************
 * aout_SetFormat: pretends to set the dsp output format
 *****************************************************************************/
static int aout_SetFormat( aout_thread_t *p_aout )
{
    OSStatus err = noErr;
    UInt32	 count, 
             bufferSize = p_aout->p_sys->ui_deviceBufferSize;
    AudioStreamBasicDescription	format;

    // get the buffersize that the default device uses for IO
    // it is required to pass the size of the data to be returned
    count = sizeof( bufferSize );	
    err = AudioDeviceSetProperty( p_aout->p_sys->device, 0, 0, false, 
                                  kAudioDevicePropertyBufferSize, 
                                  count, &bufferSize);
    intf_ErrMsg( "AudioDeviceSetProperty( buffersize = %d ) -> %d", bufferSize, err );

    if( err == noErr )
    {
        p_aout->p_sys->ui_deviceBufferSize = bufferSize;
    
        // get a description of the data format used by the default device
        // it is required to pass the size of the data to be returned
        count = sizeof( format ); 
        /*
        err = AudioDeviceGetProperty( p_aout->p_sys->device, 0, false, 
                                      kAudioDevicePropertyStreamFormat, 
                                      &count, &format);
        */
        if( err == noErr )
        {
            // intf_ErrMsg( "audio output format is %i", p_aout->i_format );
            if( format.mFormatID != kAudioFormatLinearPCM ) return paramErr;

            switch( p_aout->i_format )
            {
                case AOUT_FMT_U8:
                    break;
                case AOUT_FMT_S16_LE:           /* Little endian signed 16 */
                    // intf_ErrMsg( "This means Little endian signed 16" );
                    break; 
                case AOUT_FMT_S16_BE:              /* Big endian signed 16 */
                    // intf_ErrMsg( "This means Big endian signed 16" );
                    // format.mFormatFlags &= ~kLinearPCMFormatFlagIsFloat;
                    // format.mFormatFlags |= kLinearPCMFormatFlagIsFloat;
                    // format.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;
                    // format.mFormatFlags &= ~kLinearPCMFormatFlagIsBigEndian;
                    // format.mFormatFlags |= kLinearPCMFormatFlagIsSignedInteger;
                    break; 
                case AOUT_FMT_S8:
                    break; 
                case AOUT_FMT_U16_LE:                 /* Little endian U16 */
                    // intf_ErrMsg( "This means Little endian U16" );
                    break; 
                case AOUT_FMT_U16_BE:                    /* Big endian U16 */
                    // intf_ErrMsg( "This means Big endian U16" );
                    break;
                default:
                    ; // intf_ErrMsg( "This means Unknown aout format" );
            }

            format.mSampleRate = p_aout->l_rate;
            format.mChannelsPerFrame = p_aout->i_channels;
            err = AudioDeviceSetProperty( p_aout->p_sys->device, 0, 0, false, 
                                          kAudioDevicePropertyStreamFormat, 
                                          count, &format);
            /*
            intf_ErrMsg( "AudioDeviceSetProperty( mFormatFlags = %x, " 
                                                 "mSampleRate = %f, "
                                                 "mChannelsPerFrame = %d ) " 
                                                 "-> %d", 
                                                  format.mFormatFlags, 
                                                  format.mSampleRate, 
                                                  format.mChannelsPerFrame, 
                                                  err );
            */
        }
    }

    err = AudioDeviceAddIOProc( p_aout->p_sys->device, 
                                (AudioDeviceIOProc)appIOProc, 
                                (void *)p_aout->p_sys );

    if( err == noErr )
        err = AudioDeviceStart( p_aout->p_sys->device, (AudioDeviceIOProc)appIOProc );			
    
    return( err );
}

/*****************************************************************************
 * aout_GetBufInfo: returns available bytes in buffer
 *****************************************************************************/
static long aout_GetBufInfo( aout_thread_t *p_aout, long l_buffer_limit )
{
    return( 0 ); // Send data as soon as possible

/*
 * Tune me ?
 *
    return (   p_aout->p_sys->p_Data
             + p_aout->p_sys->sizeOfDataInMemory 
             - p_aout->p_sys->currentDataLocationPtr 
             - p_aout->p_sys->ui_deviceBufferSize );
*/
}

/*****************************************************************************
 * appIOProc : callback for audio output
 *****************************************************************************/
OSStatus appIOProc( AudioDeviceID  inDevice, const AudioTimeStamp*  inNow, 
                    const void*  inInputData, const AudioTimeStamp*  inInputTime, 
                    AudioBufferList*  outOutputData, const AudioTimeStamp* inOutputTime, 
                    void* appGlobals )
{
    aout_sys_t*	p_sys    = appGlobals;

    vlc_mutex_lock( &p_sys->mutex_lock );
    vlc_cond_signal( &p_sys->cond_sync );
    
    // move data into output data buffer
    BlockMoveData( p_sys->p_Data,
                   outOutputData->mBuffers[ 0 ].mData, 
                   p_sys->ui_deviceBufferSize );

    vlc_mutex_unlock( &p_sys->mutex_lock );

    return( noErr );     
}

/*****************************************************************************
 * aout_Play: plays a sound
 *****************************************************************************/
static void aout_Play( aout_thread_t *p_aout, byte_t *buffer, int i_size )
{
#if WRITE_AUDIO_OUTPUT_TO_FILE
    write( p_aout->p_sys->fd, buffer, i_size );
    // intf_ErrMsg( "write() -> %d", write( p_aout->p_sys->fd, buffer, i_size ) );
#else
    Convert16BitIntegerTo32Float( buffer, p_aout->p_sys->p_Data, i_size );
    vlc_cond_wait( &p_aout->p_sys->cond_sync, &p_aout->p_sys->mutex_lock );
#endif
}

/*****************************************************************************
 * aout_Close: closes the dummy audio device
 *****************************************************************************/
static void aout_Close( aout_thread_t *p_aout )
{
    OSStatus 	err = noErr;
    
    // stop playing sound through the device
    err = AudioDeviceStop( p_aout->p_sys->device, (AudioDeviceIOProc)appIOProc );			
    if (err != noErr) return;

    // remove the IO proc from the device
    err = AudioDeviceRemoveIOProc( p_aout->p_sys->device, (AudioDeviceIOProc)appIOProc );		
    if (err != noErr) return;

    // vlc_cond_signal( &p_aout->p_sys->cond_sync );
    DisposePtr( p_aout->p_sys->p_Data );
 
    return;
}

/*****************************************************************************
 * Convert16BitIntegerTo32Float
 *****************************************************************************/
void Convert16BitIntegerTo32Float( Ptr in16BitDataPtr, Ptr out32BitDataPtr, 
                                   UInt32 totalBytes )
{
    UInt32	i, samples = totalBytes / 2 /* each 16 bit sample is 2 bytes */;
    SInt16	*inDataPtr = (SInt16 *) in16BitDataPtr;
    Float32	*outDataPtr = (Float32 *) out32BitDataPtr;
    
    for( i = 0 ; i < samples ; i++ )
    {
        *outDataPtr = (Float32)(*inDataPtr);
        if( *outDataPtr > 0 )
            *outDataPtr /= 32767.0;
        else
            *outDataPtr /= 32768.0;
        outDataPtr++;
        inDataPtr++;
    }
}
       
/*****************************************************************************
 * Convert16BitIntegerTo32FloatWithByteSwap
 *****************************************************************************/
void Convert16BitIntegerTo32FloatWithByteSwap( Ptr in16BitDataPtr, 
                                               Ptr out32BitDataPtr, 
                                               UInt32 totalBytes )
{
    UInt32	i, samples = totalBytes / 2 /* each 16 bit sample is 2 bytes */;
    SInt16	*inDataPtr = (SInt16 *) in16BitDataPtr;
    Float32	*outDataPtr = (Float32 *) out32BitDataPtr;
    
    for( i = 0 ; i < samples ; i++ )
    {
        *outDataPtr = (Float32)CFSwapInt16LittleToHost(*inDataPtr);
        if( *outDataPtr > 0 )
            *outDataPtr /= 32767.0;
        else
            *outDataPtr /= 32768.0;

        outDataPtr++;
        inDataPtr++;
    }
}
       
/*****************************************************************************
 * Convert8BitIntegerTo32Float
 *****************************************************************************/
void Convert8BitIntegerTo32Float( Ptr in8BitDataPtr, Ptr out32BitDataPtr, 
                                  UInt32 totalBytes )
{
    UInt32	i, samples = totalBytes;
    SInt8	*inDataPtr = (SInt8 *) in8BitDataPtr;
    Float32	*outDataPtr = (Float32 *) out32BitDataPtr;
    
    for( i = 0 ; i < samples ; i++ )
    {
        *outDataPtr = (Float32)(*inDataPtr);
        if( *outDataPtr > 0 )
            *outDataPtr /= 32767.0;
        else
            *outDataPtr /= 32768.0;
        
        outDataPtr++;
        inDataPtr++;
    }
}
