/*****************************************************************************
 * aout_darwin.c : Darwin audio output plugin
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: aout_macosx.c,v 1.13 2002/02/19 00:50:19 sam Exp $
 *
 * Authors: Colin Delacroix <colin@zoy.org>
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

/*
 * 2001/03/21
 * Status of audio under Darwin
 * It currently works with 16 bits signed big endian mpeg 1 audio
 * (and probably mpeg 2). This is the most common case.
 * Note: ac3 decoder is currently broken under Darwin 
 *
 * TODO:
 * Find little endian files and adapt output
 * Find unsigned files and adapt output
 * Find 8 bits files and adapt output
 */
 
/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <string.h>

#include <videolan/vlc.h>

#include "audio_output.h"                                   /* aout_thread_t */

#include <Carbon/Carbon.h>
#include <CoreAudio/AudioHardware.h>

#include <sys/fcntl.h>

/*
 * Debug: to dump the output of the decoder directly to a file
 * May disappear when AC3 decoder will work on Darwin
 */
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
    int           fd;                         // debug: fd to dump audio
#endif
    AudioStreamBasicDescription	deviceFormat; // info about the default device
    AudioDeviceID device;			                // the default device
    Ptr		        p_Data;	                    // ptr to the 32 bit float data
    UInt32	      ui_deviceBufferSize;        // audio device buffer size
    vlc_mutex_t   mutex_lock;                 // pthread locks for sync of
    vlc_cond_t    cond_sync;                  // aout_Play and callback
} aout_sys_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int     aout_Open        ( aout_thread_t *p_aout );
static int     aout_SetFormat   ( aout_thread_t *p_aout );
static long    aout_GetBufInfo  ( aout_thread_t *p_aout, long l_buffer_info );
static void    aout_Play        ( aout_thread_t *p_aout,
                                  byte_t *buffer, int i_size );
static void    aout_Close       ( aout_thread_t *p_aout );

static OSStatus appIOProc( AudioDeviceID inDevice, const AudioTimeStamp* inNow, 
                    const void* inInputData, const AudioTimeStamp* inInputTime,
                    AudioBufferList* outOutputData, 
                    const AudioTimeStamp* inOutputTime, 
                    void* threadGlobals );
static void Convert16BitIntegerTo32Float( Ptr p_in16BitDataPtr, Ptr p_out32BitDataPtr, 
                                   UInt32 ui_totalBytes );
static void Convert16BitIntegerTo32FloatWithByteSwap( Ptr p_in16BitDataPtr, 
                                               Ptr p_out32BitDataPtr, 
                                               UInt32 p_totalBytes );
static void Convert8BitIntegerTo32Float( Ptr in8BitDataPtr, Ptr p_out32BitDataPtr, 
                                  UInt32 ui_totalBytes );

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
 * aout_Open: opens a HAL audio device
 *****************************************************************************/
static int aout_Open( aout_thread_t *p_aout )
{
    OSStatus        err = noErr;
    UInt32			    ui_paramSize, ui_bufferSize;
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
    p_aout->p_sys->device = kAudioDeviceUnknown;
    p_aout->p_sys->p_Data = nil;
    
    /*
     * get the default output device for the HAL
     * it is required to pass the size of the data to be returned
     */
    ui_paramSize = sizeof( p_aout->p_sys->device );	
    err = AudioHardwareGetProperty( kAudioHardwarePropertyDefaultOutputDevice,
                                    &ui_paramSize, (void *) &device );
    
    
    if( err == noErr) 
    {
        /* 
         * The values we get here are not used. We may find another method for
         * insuring us that the audio device is working !
         *
         * First get the buffersize that the default device uses for IO
         */
        ui_paramSize = sizeof( p_aout->p_sys->ui_deviceBufferSize );
        err = AudioDeviceGetProperty( device, 0, false, 
                                      kAudioDevicePropertyBufferSize, 
                                      &ui_paramSize, &ui_bufferSize);
        if( err == noErr )
        {
            /* get a description of the data format used by the default device */
            ui_paramSize = sizeof(p_aout->p_sys->deviceFormat); 
            err = AudioDeviceGetProperty( device, 0, false, 
                                          kAudioDevicePropertyStreamFormat, 
                                          &ui_paramSize, &format);
            if( err == noErr )
            {
                if( format.mFormatID != kAudioFormatLinearPCM ) return paramErr;
            
                /* everything is ok so fill in p_sys */
                p_aout->p_sys->device              = device;
                p_aout->p_sys->ui_deviceBufferSize = ui_bufferSize;
                p_aout->p_sys->deviceFormat        = format;
            }
        }
    }

    if (err != noErr) return err;

    /* 
     * Size calcul taken from audio_output.c we may change that file so we would
     * not be forced to compute the same value twice
     */
    p_aout->p_sys->ui_deviceBufferSize = 
      2 * 2 * sizeof(s16) * ((s64)p_aout->l_rate * AOUT_BUFFER_DURATION) / 1000000; 
 
    /* Allocate memory for audio */
    p_aout->p_sys->p_Data = NewPtrClear( p_aout->p_sys->ui_deviceBufferSize );
    if( p_aout->p_sys->p_Data == nil ) return paramErr;

#if WRITE_AUDIO_OUTPUT_TO_FILE
    p_aout->p_sys->fd = open( "audio-darwin.pcm", O_RDWR|O_CREAT );
    intf_WarnMsg( 3, "open(...) -> %d", p_aout->p_sys->fd );
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
    UInt32	 ui_paramSize, 
             ui_bufferSize = p_aout->p_sys->ui_deviceBufferSize;
    AudioStreamBasicDescription	format;

    /* set the buffersize that the default device uses for IO */
    ui_paramSize = sizeof( ui_bufferSize );	
    err = AudioDeviceSetProperty( p_aout->p_sys->device, 0, 0, false, 
                                  kAudioDevicePropertyBufferSize, 
                                  ui_paramSize, &ui_bufferSize);
    if( err != noErr )
    {
        /* We have to tell the decoder to use audio device's buffer size  */
        intf_ErrMsg( "aout : AudioDeviceSetProperty failed ( buffersize = %d ) -> %d",
                     ui_bufferSize, err );
        return( -1 );
    }
    else
    {
        p_aout->p_sys->ui_deviceBufferSize = ui_bufferSize;
    
        ui_paramSize = sizeof( format ); 
        err = AudioDeviceGetProperty( p_aout->p_sys->device, 0, false, 
                                      kAudioDevicePropertyStreamFormat, 
                                      &ui_paramSize, &format);

        if( err == noErr )
        {
            /*
             * setting format.mFormatFlags to anything but the default value 
             * doesn't seem to work. Can anybody explain that ??
             */

            switch( p_aout->i_format )
            {
                case AOUT_FMT_U8:
                    intf_ErrMsg( "Audio format (Unsigned 8) not supported now,"
                                 "please report stream" );
                    return( -1 );
                    
                case AOUT_FMT_S16_LE:           /* Little endian signed 16 */
                    // format.mFormatFlags &= ~kLinearPCMFormatFlagIsBigEndian;
                    intf_ErrMsg( "Audio format (LE Unsigned 16) not supported now,"
                                 "please report stream" );
                    return( -1 );
                    
                case AOUT_FMT_S16_BE:              /* Big endian signed 16 */
                    // format.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;
                    break;
                    
                case AOUT_FMT_S8:
                    intf_ErrMsg( "Audio format (Signed 8) not supported now,"
                                 "please report stream" );
                    return( -1 );
                    
                case AOUT_FMT_U16_LE:                 /* Little endian U16 */
                    // format.mFormatFlags &= ~kLinearPCMFormatFlagIsSignedInteger;
                    intf_ErrMsg( "Audio format (LE Unsigned 8) not supported now,"
                                 "please report stream" );
                    return( -1 );
                    
                case AOUT_FMT_U16_BE:                    /* Big endian U16 */
                    // format.mFormatFlags &= ~kLinearPCMFormatFlagIsSignedInteger;
                    intf_ErrMsg( "Audio format (BE Unsigned 8) not supported now,"
                                 "please report stream" );
                    return( -1 );
                    
                    break;
                default:
                    return( -1 );
            }

            /*
             * It would have been nice to have these work (no more buffer
             * convertion to float) but I couldn't manage to
             */
            // format.mFormatFlags &= ~kLinearPCMFormatFlagIsFloat;
            // format.mFormatFlags |= kLinearPCMFormatFlagIsFloat;
            // format.mFormatFlags |= kLinearPCMFormatFlagIsSignedInteger;

            format.mSampleRate       = p_aout->l_rate;
            format.mChannelsPerFrame = p_aout->i_channels;

            err = AudioDeviceSetProperty( p_aout->p_sys->device, 0, 0, false, 
                                          kAudioDevicePropertyStreamFormat, 
                                          ui_paramSize, &format);
            if( err != noErr )
            {
                intf_ErrMsg( "aout : AudioDeviceSetProperty( mFormatFlags = %x, " 
                             "mSampleRate = %f, mChannelsPerFrame = %d ) -> %d", 
                             format.mFormatFlags, format.mSampleRate, 
                             format.mChannelsPerFrame, err );
                return( -1 );
            }
        }
    }

    /* add callback */
    err = AudioDeviceAddIOProc( p_aout->p_sys->device, 
                                (AudioDeviceIOProc)appIOProc, 
                                (void *)p_aout->p_sys );

    /* open the output */
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
static OSStatus appIOProc( AudioDeviceID  inDevice, const AudioTimeStamp*  inNow, 
                    const void*  inInputData, const AudioTimeStamp*  inInputTime, 
                    AudioBufferList*  outOutputData, const AudioTimeStamp* inOutputTime, 
                    void* threadGlobals )
{
    aout_sys_t*	p_sys = threadGlobals;

    /* see aout_Play below */
    vlc_mutex_lock( &p_sys->mutex_lock );
    vlc_cond_signal( &p_sys->cond_sync );
    
    /* move data into output data buffer */
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
#else
    Convert16BitIntegerTo32Float( buffer, p_aout->p_sys->p_Data, i_size );
    
    /* 
     * wait for a callback to occur (to flush the buffer), so aout_Play
     * can't be called twice, losing the data we just wrote. 
     */
    vlc_cond_wait( &p_aout->p_sys->cond_sync, &p_aout->p_sys->mutex_lock );
#endif
}

/*****************************************************************************
 * aout_Close: closes the dummy audio device
 *****************************************************************************/
static void aout_Close( aout_thread_t *p_aout )
{
    OSStatus 	err = noErr;
    
    /* stop playing sound through the device */
    err = AudioDeviceStop( p_aout->p_sys->device, 
                           (AudioDeviceIOProc)appIOProc );			
    if( err == noErr )
    {
        /* remove the callback */
        err = AudioDeviceRemoveIOProc( p_aout->p_sys->device, 
                                      (AudioDeviceIOProc)appIOProc );		
    }

    DisposePtr( p_aout->p_sys->p_Data );
 
    return;
}

/*****************************************************************************
 * Convert16BitIntegerTo32Float
 *****************************************************************************/
static void Convert16BitIntegerTo32Float( Ptr p_in16BitDataPtr, Ptr p_out32BitDataPtr, 
                                   UInt32 ui_totalBytes )
{
    UInt32	i, ui_samples = ui_totalBytes / 2 /* each 16 bit sample is 2 bytes */;
    SInt16	*p_s_inDataPtr = (SInt16 *) p_in16BitDataPtr;
    Float32	*p_f_outDataPtr = (Float32 *) p_out32BitDataPtr;
    
    for( i = 0 ; i < ui_samples ; i++ )
    {
        *p_f_outDataPtr = (Float32)(*p_s_inDataPtr);
        if( *p_f_outDataPtr > 0 )
            *p_f_outDataPtr /= 32767.0;
        else
            *p_f_outDataPtr /= 32768.0;
        p_f_outDataPtr++;
        p_s_inDataPtr++;
    }
}
       
/*****************************************************************************
 * Convert16BitIntegerTo32FloatWithByteSwap
 *****************************************************************************/
static void Convert16BitIntegerTo32FloatWithByteSwap( Ptr p_in16BitDataPtr, 
                                               Ptr p_out32BitDataPtr, 
                                               UInt32 ui_totalBytes )
{
    UInt32	i, ui_samples = ui_totalBytes / 2 /* each 16 bit sample is 2 bytes */;
    SInt16	*p_s_inDataPtr = (SInt16 *) p_in16BitDataPtr;
    Float32	*p_f_outDataPtr = (Float32 *) p_out32BitDataPtr;
    
    for( i = 0 ; i < ui_samples ; i++ )
    {
        *p_f_outDataPtr = (Float32)CFSwapInt16LittleToHost(*p_s_inDataPtr);
        if( *p_f_outDataPtr > 0 )
            *p_f_outDataPtr /= 32767.0;
        else
            *p_f_outDataPtr /= 32768.0;

        p_f_outDataPtr++;
        p_s_inDataPtr++;
    }
}
       
/*****************************************************************************
 * Convert8BitIntegerTo32Float
 *****************************************************************************/
static void Convert8BitIntegerTo32Float( Ptr p_in8BitDataPtr, Ptr p_out32BitDataPtr, 
                                  UInt32 ui_totalBytes )
{
    UInt32	i, ui_samples = ui_totalBytes;
    SInt8	*p_c_inDataPtr = (SInt8 *)p_in8BitDataPtr;
    Float32	*p_f_outDataPtr = (Float32 *)p_out32BitDataPtr;
    
    for( i = 0 ; i < ui_samples ; i++ )
    {
        *p_f_outDataPtr = (Float32)(*p_c_inDataPtr);
        if( *p_f_outDataPtr > 0 )
            *p_f_outDataPtr /= 32767.0;
        else
            *p_f_outDataPtr /= 32768.0;
        
        p_f_outDataPtr++;
        p_c_inDataPtr++;
    }
}
