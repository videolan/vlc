/*****************************************************************************
 * coreaudio.c: CoreAudio output plugin
 *****************************************************************************
 * Copyright (C) 2002-2004 the VideoLAN team
 * $Id$
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

#include <CoreAudio/CoreAudio.h>

#define STREAM_FORMAT_MSG( pre, sfm ) \
    pre ": [%ld][%4.4s][%ld][%ld][%ld][%ld][%ld][%ld]", \
    (UInt32)sfm.mSampleRate, (char *)&sfm.mFormatID, \
    sfm.mFormatFlags, sfm.mBytesPerPacket, \
    sfm.mFramesPerPacket, sfm.mBytesPerFrame, \
    sfm.mChannelsPerFrame, sfm.mBitsPerChannel

/*****************************************************************************
 * aout_class_t 
 ****************************************************************************/
enum AudioDeviceClass
{
    AudioDeviceClassA52     = 1 << 0,
    AudioDeviceClassPCM     = 1 << 1
};

static struct aout_class_t
{
    UInt32 mFormatID;
    UInt32 mChannelsPerFrame;
    enum AudioDeviceClass class;
    const char * psz_class;
}
aout_classes[] =
{
    { /* old A/52 format type */
        'IAC3', 
        2, 
        AudioDeviceClassA52, 
        "Digital A/52" 
    },

    { /* new A/52 format type */
        kAudioFormat60958AC3, 
        2, 
        AudioDeviceClassA52, 
        "Digital A/52"
    },

    {
        kAudioFormatLinearPCM, 
        2, 
        AudioDeviceClassPCM, 
        "Stereo PCM"
    },

    {
        kAudioFormatLinearPCM, 
        1, 
        AudioDeviceClassPCM, 
        "Mono PCM"
    },

    {
        kAudioFormatLinearPCM,
        4,
        AudioDeviceClassPCM,
        "4 Channel PCM"
    },

    {
        kAudioFormatLinearPCM, 
        6, 
        AudioDeviceClassPCM, 
        "6 Channel PCM"
    },

    {
        kAudioFormatLinearPCM,
        8,
        AudioDeviceClassPCM,
        "8 Channel PCM"
    }
}; 

#define N_AOUT_CLASSES (sizeof(aout_classes)/sizeof(aout_classes[0]))

/*****************************************************************************
 * aout_option_t
 ****************************************************************************/
struct aout_option_t
{
    char sz_option[64];
    UInt32 i_dev, i_idx;
    UInt32 i_sdx, i_cdx;
    AudioStreamID i_sid;
};

/*****************************************************************************
 * aout_dev_t
 ****************************************************************************/
struct aout_dev_t
{
    AudioDeviceID devid;
    char * psz_device_name;
    UInt32 i_streams;
    UInt32 * pi_streams;
    AudioStreamBasicDescription ** pp_streams;
};

/*****************************************************************************
 * aout_sys_t: private audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the CoreAudio specific properties of an output thread.
 *****************************************************************************/
struct aout_sys_t
{
    vlc_mutex_t                 lock;
    vlc_bool_t                  b_hwinfo;
    UInt32                      i_def_dev;
    UInt32                      i_devices;
    struct aout_dev_t *         p_devices;
    UInt32                      i_sel_opt;
    UInt32                      i_options;
    struct aout_option_t *      p_options;

    AudioDeviceID               devid;
    UInt32                      i_stream_index;
    AudioStreamBasicDescription stream_format;
    UInt32                      b_dev_alive;
    pid_t                       i_hog_pid;

    vlc_bool_t                  b_revert_sfmt;
    AudioStreamBasicDescription sfmt_revert;

    UInt32                      i_bufframe_size;
    mtime_t                     clock_diff;
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int      InitHardwareInfo ( aout_instance_t * p_aout );
static int      InitDeviceInfo   ( UInt32 i_dev, aout_instance_t * p_aout ); 
static void     FreeDeviceInfo   ( UInt32 i_dev, aout_instance_t * p_aout ); 
static void     FreeHardwareInfo ( aout_instance_t * p_aout );
static int      InitDevice       ( aout_instance_t * p_aout );
static void     FreeDevice       ( aout_instance_t * p_aout );
static int      GetStreamID      ( AudioDeviceID devid, UInt32 i_idx,
                                   AudioStreamID * p_sid );
static int      InitStreamInfo   ( UInt32 i_dev, aout_instance_t * p_aout,
                                   UInt32 i_idx );
static void     FreeStreamInfo   ( UInt32 i_dev, aout_instance_t * p_aout,
                                   UInt32 i_idx );
static void     InitDeviceVar    ( aout_instance_t * p_aout, int i_option,
                                   vlc_bool_t b_change );

static int      Open             ( vlc_object_t * );
static void     Close            ( vlc_object_t * );

static void     Play             ( aout_instance_t * p_aout );

static OSStatus IOCallback       ( AudioDeviceID inDevice,
                                   const AudioTimeStamp * inNow, 
                                   const void * inInputData, 
                                   const AudioTimeStamp * inInputTime,
                                   AudioBufferList * outOutputData, 
                                   const AudioTimeStamp * inOutputTime, 
                                   void * threadGlobals );

static OSStatus HardwareListener ( AudioHardwarePropertyID inPropertyID,
                                   void * inClientData );

static OSStatus DeviceListener   ( AudioDeviceID inDevice,
                                   UInt32 inChannel,
                                   Boolean isInput,
                                   AudioDevicePropertyID inPropertyID,
                                   void * inClientData );

static OSStatus StreamListener   ( AudioStreamID inStream,
                                   UInt32 inChannel,
                                   AudioDevicePropertyID inPropertyID,
                                   void * inClientData );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ADEV_TEXT N_("Audio Device")
#define ADEV_LONGTEXT N_("Choose a number corresponding to the number of an " \
    "audio device, as listed in your 'Audio Device' menu. This device will " \
    "then be used by default for audio playback.")

vlc_module_begin();
    set_shortname( "CoreAudio" );
    set_description( _("CoreAudio output") );
    set_capability( "audio output", 100 );
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_AOUT );
    set_callbacks( Open, Close );
    add_integer( "coreaudio-dev", -1, NULL, ADEV_TEXT, ADEV_LONGTEXT, VLC_FALSE ); 
vlc_module_end();

/*****************************************************************************
 * Open: open a CoreAudio HAL device
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    OSStatus err;
    UInt32 i_param_size;
    struct aout_sys_t * p_sys;
    aout_instance_t * p_aout = (aout_instance_t *)p_this;
    struct aout_option_t * p_option;
    UInt32 i_startingChannel;

    /* Allocate structure */
    p_sys = (struct aout_sys_t *)malloc( sizeof( struct aout_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return( VLC_ENOMEM );
    }

    memset( p_sys, 0, sizeof( struct aout_sys_t ) );

    p_aout->output.p_sys = p_sys;
    p_aout->output.pf_play = Play;
    p_sys->i_hog_pid = -1;

    vlc_mutex_init( p_aout, &p_sys->lock );

    if( InitHardwareInfo( p_aout ) )
    {
        msg_Err( p_aout, "InitHardwareInfo failed" );
        vlc_mutex_destroy( &p_sys->lock );
        free( (void *)p_sys );
        return( VLC_EGENERIC );
    } 

    if( var_Type( p_aout, "audio-device" ) == 0 )
    {
        InitDeviceVar( p_aout, config_GetInt( p_aout, "coreaudio-dev" ),
                       VLC_FALSE );
    }

    if( InitDevice( p_aout ) )
    {
        msg_Err( p_aout, "InitDevice failed" );
        FreeHardwareInfo( p_aout );
        vlc_mutex_destroy( &p_sys->lock );
        free( (void *)p_sys );
        return( VLC_EGENERIC );
    } 

    /* get starting channel for the selected stream */
    p_option = &p_sys->p_options[p_sys->i_sel_opt];
    i_param_size = sizeof( UInt32 ); 
    err = AudioStreamGetProperty( p_option->i_sid, 0, 
                                  kAudioStreamPropertyStartingChannel,
                                  &i_param_size, &i_startingChannel );
    if( err != noErr )
    {
        msg_Err( p_aout, "failed to get channel number: [%4.4s]", 
                 (char *)&err );
        FreeDevice( p_aout );
        FreeHardwareInfo( p_aout );
        vlc_mutex_destroy( &p_sys->lock );
        free( (void *)p_sys );
        return( VLC_EGENERIC );
    }
    
    msg_Dbg( p_aout, "starting channel: [%ld]", i_startingChannel );

    /* Get a description of the stream format */
    i_param_size = sizeof( AudioStreamBasicDescription ); 
    err = AudioDeviceGetProperty( p_sys->devid, i_startingChannel, FALSE, 
                                  kAudioStreamPropertyPhysicalFormat,
                                  &i_param_size, &p_sys->stream_format );
    if( err != noErr )
    {
        msg_Err( p_aout, "failed to get stream format: [%4.4s]", 
                 (char *)&err );
        FreeDevice( p_aout );
        FreeHardwareInfo( p_aout );
        vlc_mutex_destroy( &p_sys->lock );
        free( (void *)p_sys );
        return( VLC_EGENERIC );
    }

    /* Set the output sample rate */
    p_aout->output.output.i_rate = 
        (unsigned int)p_sys->stream_format.mSampleRate;

    msg_Dbg( p_aout, STREAM_FORMAT_MSG( "using format",
                                        p_sys->stream_format ) );

    /* Get the bufframe size */
    i_param_size = sizeof( p_sys->i_bufframe_size );
    err = AudioDeviceGetProperty( p_sys->devid, i_startingChannel, FALSE, 
                                  kAudioDevicePropertyBufferFrameSize, 
                                  &i_param_size, &p_sys->i_bufframe_size );
    if( err != noErr )
    {
        msg_Err( p_aout, "failed to get bufframe size: [%4.4s]", 
                 (char *)&err );
        FreeDevice( p_aout );
        FreeHardwareInfo( p_aout );
        vlc_mutex_destroy( &p_sys->lock );
        free( (void *)p_sys );
        return( VLC_EGENERIC );
    }
    
    msg_Dbg( p_aout, "device bufframe size: [%ld]", p_sys->i_bufframe_size );
    msg_Dbg( p_aout, "device buffer index: [%ld]", p_sys->i_stream_index );

	/* If we do AC3 over SPDIF, set buffer size to one AC3 frame */
	if( ( p_sys->stream_format.mFormatID == kAudioFormat60958AC3 ||
			p_sys->stream_format.mFormatID == 'IAC3' ) &&
		p_sys->i_bufframe_size != A52_FRAME_NB )
	{   
		p_sys->i_bufframe_size = A52_FRAME_NB;
		i_param_size = sizeof( p_sys->i_bufframe_size );
		err = AudioDeviceSetProperty( p_sys->devid, 0, 0, FALSE,
									  kAudioDevicePropertyBufferFrameSize,
									  i_param_size, &p_sys->i_bufframe_size );
		if( err != noErr )
		{
			msg_Err( p_aout, "failed to set bufframe size (%ld): [%4.4s]", 
					 p_sys->i_bufframe_size, (char *)&err );
			FreeDevice( p_aout );
			FreeHardwareInfo( p_aout );
			vlc_mutex_destroy( &p_sys->lock );
			free( (void *)p_sys );
			return( VLC_EGENERIC );
		}

		msg_Dbg( p_aout, "device bufframe size set to: [%ld]", 
			 p_sys->i_bufframe_size );
	}

    switch( p_sys->stream_format.mFormatID )
    {
    case kAudioFormatLinearPCM:
        p_aout->output.output.i_format = VLC_FOURCC('f','l','3','2');

        switch( p_sys->stream_format.mChannelsPerFrame )
        {
        case 1:
            p_aout->output.output.i_physical_channels = AOUT_CHAN_CENTER;
            break;

        case 2:
            p_aout->output.output.i_physical_channels =
                AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
            break;

        case 4:
            p_aout->output.output.i_physical_channels =
                AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
            break;

        case 6:
            p_aout->output.output.i_physical_channels =
                AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT |
                AOUT_CHAN_CENTER | AOUT_CHAN_LFE;
            break;

        case 8:
            p_aout->output.output.i_physical_channels =
                AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT |
                AOUT_CHAN_CENTER | AOUT_CHAN_LFE |
                AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT;
            break;

        default:
            msg_Err( p_aout, "unknown channel count: [%ld]",
                     p_sys->stream_format.mChannelsPerFrame ); 
            FreeDevice( p_aout );
            FreeHardwareInfo( p_aout );
            vlc_mutex_destroy( &p_sys->lock );
            free( (void *)p_sys );
            return( VLC_EGENERIC );
        }

        p_aout->output.i_nb_samples = p_sys->i_bufframe_size;

        aout_VolumeSoftInit( p_aout );
        break;

    case 'IAC3':
    case kAudioFormat60958AC3:
        if( p_sys->stream_format.mFormatFlags & kAudioFormatFlagIsBigEndian )
            p_aout->output.output.i_format = VLC_FOURCC('s','p','d','b');
        else
            p_aout->output.output.i_format = VLC_FOURCC('s','p','d','i');
        p_aout->output.output.i_bytes_per_frame = AOUT_SPDIF_SIZE;
        p_aout->output.output.i_frame_length = A52_FRAME_NB;
        p_aout->output.i_nb_samples = p_aout->output.output.i_frame_length;

        aout_VolumeNoneInit( p_aout );
        break;

    default:
        msg_Err( p_aout, "unknown hardware format: [%4.4s]", 
                 (char *)&p_sys->stream_format.mFormatID );
        FreeDevice( p_aout );
        FreeHardwareInfo( p_aout );
        vlc_mutex_destroy( &p_sys->lock );
        free( (void *)p_sys );
        return( VLC_EGENERIC );
    }

    /* Add callback */
    err = AudioDeviceAddIOProc( p_sys->devid,
                                (AudioDeviceIOProc)IOCallback,
                                (void *)p_aout );
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioDeviceAddIOProc failed: [%4.4s]",
                 (char *)&err );
        FreeDevice( p_aout );
        FreeHardwareInfo( p_aout );
        vlc_mutex_destroy( &p_sys->lock );
        free( (void *)p_sys );
        return( VLC_EGENERIC );
    }
 
    /* Start device */
    err = AudioDeviceStart( p_sys->devid, (AudioDeviceIOProc)IOCallback ); 
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioDeviceStart failed: [%4.4s]",
                 (char *)&err );

        err = AudioDeviceRemoveIOProc( p_sys->devid, 
                                       (AudioDeviceIOProc)IOCallback );
        if( err != noErr )
        {
            msg_Err( p_aout, "AudioDeviceRemoveIOProc failed: [%4.4s]",
                     (char *)&err );
        }

        FreeDevice( p_aout );
        FreeHardwareInfo( p_aout );
        vlc_mutex_destroy( &p_sys->lock );
        free( (void *)p_sys );

        return( VLC_EGENERIC );
    }

    err = AudioHardwareAddPropertyListener( kAudioHardwarePropertyDevices,
                                            HardwareListener, 
                                            (void *)p_aout ); 
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioHardwareAddPropertyListener failed: %4.4s",
                 (char *)&err );

        /* Stop device */
        err = AudioDeviceStop( p_sys->devid, 
                               (AudioDeviceIOProc)IOCallback ); 
        if( err != noErr )
        {
            msg_Err( p_aout, "AudioDeviceStop failed: [%4.4s]",
                     (char *)&err );
        }

        /* Remove callback */
        err = AudioDeviceRemoveIOProc( p_sys->devid,
                                       (AudioDeviceIOProc)IOCallback );
        if( err != noErr )
        {
            msg_Err( p_aout, "AudioDeviceRemoveIOProc failed: [%4.4s]",
                     (char *)&err );
        }

        FreeDevice( p_aout );
        FreeHardwareInfo( p_aout );
        vlc_mutex_destroy( &p_sys->lock );
        free( (void *)p_sys );

        return( VLC_EGENERIC );
    }

    /* Let's pray for the following operation to be atomic... */
    p_sys->clock_diff = - (mtime_t)
        AudioConvertHostTimeToNanos( AudioGetCurrentHostTime() ) / 1000; 
    p_sys->clock_diff += mdate();

    return( VLC_SUCCESS );
}

/*****************************************************************************
 * Close: close the CoreAudio HAL device
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    OSStatus err; 
    aout_instance_t * p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    if( p_sys->b_dev_alive )
    {
        /* Stop device */
        err = AudioDeviceStop( p_sys->devid, 
                               (AudioDeviceIOProc)IOCallback ); 
        if( err != noErr )
        {
            msg_Err( p_aout, "AudioDeviceStop failed: [%4.4s]",
                     (char *)&err );
        }

        /* Remove callback */
        err = AudioDeviceRemoveIOProc( p_sys->devid,
                                       (AudioDeviceIOProc)IOCallback );
        if( err != noErr )
        {
            msg_Err( p_aout, "AudioDeviceRemoveIOProc failed: [%4.4s]",
                     (char *)&err );
        }

        FreeDevice( p_aout );
    }

    err = AudioHardwareRemovePropertyListener( kAudioHardwarePropertyDevices,
                                               HardwareListener );
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioHardwareRemovePropertyListener failed: [%4.4s]",
                 (char *)&err );
    }

    FreeHardwareInfo( p_aout );

    vlc_mutex_destroy( &p_sys->lock );

    free( p_sys );
}

/*****************************************************************************
 * Play: nothing to do
 *****************************************************************************/
static void Play( aout_instance_t * p_aout )
{
}

/*****************************************************************************
 * IOCallback: callback for audio output
 *****************************************************************************/
static OSStatus IOCallback( AudioDeviceID inDevice,
                            const AudioTimeStamp * inNow, 
                            const void * inInputData,
                            const AudioTimeStamp * inInputTime, 
                            AudioBufferList * outOutputData,
                            const AudioTimeStamp * inOutputTime, 
                            void * threadGlobals )
{
    aout_buffer_t * p_buffer;
    mtime_t         current_date;

    aout_instance_t * p_aout = (aout_instance_t *)threadGlobals;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    p_sys->clock_diff = - (mtime_t)
        AudioConvertHostTimeToNanos( inOutputTime->mHostTime ) / 1000; 
    p_sys->clock_diff += mdate();

    current_date = p_sys->clock_diff +
                   AudioConvertHostTimeToNanos( inOutputTime->mHostTime ) / 1000;

#define B_SPDI (p_aout->output.output.i_format == VLC_FOURCC('s','p','d','i') || p_aout->output.output.i_format == VLC_FOURCC('s','p','d','b') )
    p_buffer = aout_OutputNextBuffer( p_aout, current_date, B_SPDI );
#undef B_SPDI

#define BUFFER outOutputData->mBuffers[p_sys->i_stream_index]
    if( p_buffer != NULL )
    {
        /* move data into output data buffer */
        p_aout->p_vlc->pf_memcpy( BUFFER.mData,
                                  p_buffer->p_buffer, p_buffer->i_nb_bytes );
        aout_BufferFree( p_buffer );
    }
    else
    {
        if( p_aout->output.output.i_format == VLC_FOURCC('f','l','3','2') )
        {
            UInt32 i, i_size = p_sys->i_bufframe_size * 
                               p_sys->stream_format.mChannelsPerFrame;
            float * p = (float *)BUFFER.mData;

            for( i = 0; i < i_size; i++ )
            {
                *p++ = 0.0;
            }
        }
        else
        {
            p_aout->p_vlc->pf_memset( BUFFER.mData, 0, BUFFER.mDataByteSize );
        }
    }
#undef BUFFER

    return( noErr );     
}

/*****************************************************************************
 * InitHardwareInfo
 *****************************************************************************/
static int InitHardwareInfo( aout_instance_t * p_aout )
{
    OSStatus err;
    UInt32 i, i_param_size;
    AudioDeviceID devid_def; 
    AudioDeviceID * p_devices;

    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    vlc_mutex_lock( &p_sys->lock );

    /* Get number of devices */
    err = AudioHardwareGetPropertyInfo( kAudioHardwarePropertyDevices,
                                        &i_param_size, NULL );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get number of devices: [%4.4s]",
                 (char *)&err );
        vlc_mutex_unlock( &p_sys->lock );
        return( VLC_EGENERIC );
    }

    p_sys->i_devices = i_param_size / sizeof( AudioDeviceID );

    if( p_sys->i_devices < 1 )
    {
        msg_Err( p_aout, "no devices found" );
        vlc_mutex_unlock( &p_sys->lock );
        return( VLC_EGENERIC );
    }

    msg_Dbg( p_aout, "system has [%ld] device(s)", p_sys->i_devices );

    /* Allocate DeviceID array */
    p_devices = (AudioDeviceID *)malloc( i_param_size );
    if( p_devices == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        vlc_mutex_unlock( &p_sys->lock );
        return( VLC_ENOMEM );
    }

    /* Populate DeviceID array */
    err = AudioHardwareGetProperty( kAudioHardwarePropertyDevices,
                                    &i_param_size, (void *)p_devices );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get the device ID's: [%4.4s]",
                 (char *)&err );
        free( (void *)p_devices );
        vlc_mutex_unlock( &p_sys->lock );
        return( VLC_EGENERIC );
    }

    i_param_size = sizeof( AudioDeviceID );
    err = AudioHardwareGetProperty( kAudioHardwarePropertyDefaultOutputDevice,
                                    &i_param_size, (void *)&devid_def );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get default audio device: [%4.4s]",
                 (char *)&err );
        free( (void *)p_devices );
        vlc_mutex_unlock( &p_sys->lock );
        return( VLC_EGENERIC );
    }

    p_sys->p_devices = (struct aout_dev_t *)
        malloc( sizeof( struct aout_dev_t ) * p_sys->i_devices ); 
    if( p_sys->p_devices == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        free( (void *)p_devices );
        vlc_mutex_unlock( &p_sys->lock );
        return( VLC_ENOMEM );
    }    

    p_sys->i_options = 0;
    p_sys->p_options = NULL;

    for( i = 0; i < p_sys->i_devices; i++ )
    {
        p_sys->p_devices[i].devid = p_devices[i];

        if( p_devices[i] == devid_def )
        {
            p_sys->i_def_dev = i;
        }

        if( InitDeviceInfo( i, p_aout ) )
        {
            UInt32 j;

            msg_Err( p_aout, "InitDeviceInfo(%ld) failed", i );

            for( j = 0; j < i; j++ )
            {
                FreeDeviceInfo( j, p_aout );
            }
    
            free( (void *)p_sys->p_devices );
            free( (void *)p_devices );

            vlc_mutex_unlock( &p_sys->lock );

            return( VLC_EGENERIC );
        }
    }

    free( (void *)p_devices );

    p_sys->b_hwinfo = VLC_TRUE;

    vlc_mutex_unlock( &p_sys->lock );

    return( VLC_SUCCESS );
}

/*****************************************************************************
 * InitDeviceInfo
 *****************************************************************************/
static int InitDeviceInfo( UInt32 i_dev, aout_instance_t * p_aout ) 
{
    OSStatus err;
    UInt32 i, i_param_size;
    AudioBufferList * p_buffer_list;

    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    struct aout_dev_t * p_dev = &p_sys->p_devices[i_dev];

    /* Get length of device name */
    err = AudioDeviceGetPropertyInfo( p_dev->devid, 0, FALSE, 
                                      kAudioDevicePropertyDeviceName,
                                      &i_param_size, NULL ); 
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get size of devicename: [%4.4s]",
                 (char *)&err ); 
        return( VLC_EGENERIC );
    }

    /* Allocate memory for device name */
    p_dev->psz_device_name = (char *)malloc( i_param_size );
    if( p_dev->psz_device_name == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return( VLC_ENOMEM );
    }

    /* Get device name */
    err = AudioDeviceGetProperty( p_dev->devid, 0, FALSE,
                                  kAudioDevicePropertyDeviceName,
                                  &i_param_size, p_dev->psz_device_name ); 
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get devicename: [%4.4s]",
                 (char *)&err );
        free( (void *)p_dev->psz_device_name );
        return( VLC_EGENERIC );
    }

    msg_Dbg( p_aout, "device [%ld] has name [%s]",
             i_dev, p_dev->psz_device_name );

    err = AudioDeviceGetPropertyInfo( p_dev->devid, 0, FALSE,
                                      kAudioDevicePropertyStreamConfiguration,
                                      &i_param_size, NULL );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get size of stream configuration: [%4.4s]",
                 (char *)&err );
        free( (void *)p_dev->psz_device_name );
        return( VLC_EGENERIC );
    }

    p_buffer_list = (AudioBufferList *)malloc( i_param_size );
    if( p_buffer_list == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        free( (void *)p_dev->psz_device_name );
        return( VLC_ENOMEM );
    }

    err = AudioDeviceGetProperty( p_dev->devid, 0, FALSE,
                                  kAudioDevicePropertyStreamConfiguration,
                                  &i_param_size, p_buffer_list );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get stream configuration: [%4.4s]",
                 (char *)&err );
        free( (void *)p_dev->psz_device_name );
        free( (void *)p_buffer_list );
        return( VLC_EGENERIC );
    }

    p_dev->i_streams = p_buffer_list->mNumberBuffers;
    free( (void *)p_buffer_list );

    msg_Dbg( p_aout, "device [%ld] has [%ld] streams", 
             i_dev, p_dev->i_streams ); 

    p_dev->pi_streams = (UInt32 *)malloc( p_dev->i_streams *
                                          sizeof( *p_dev->pi_streams ) );
    if( p_dev->pi_streams == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        free( (void *)p_dev->psz_device_name );
        return( VLC_ENOMEM );
    }

    p_dev->pp_streams = (AudioStreamBasicDescription **) 
                        malloc( p_dev->i_streams * 
                                sizeof( *p_dev->pp_streams ) );
    if( p_dev->pp_streams == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        free( (void *)p_dev->psz_device_name );
        free( (void *)p_dev->pi_streams );
        return( VLC_ENOMEM );
    } 

    for( i = 0; i < p_dev->i_streams; i++ )
    {
        if( InitStreamInfo( i_dev, p_aout, i ) )
        {
            UInt32 j;

            msg_Err( p_aout, "InitStreamInfo(%ld, %ld) failed", i_dev, i );

            for( j = 0; j < i; j++ )
            {
                FreeStreamInfo( i_dev, p_aout, j );
            }

            free( (void *)p_dev->psz_device_name );
            free( (void *)p_dev->pi_streams );
            free( (void *)p_dev->pp_streams );

            return( VLC_EGENERIC );
        }
    }

    return( VLC_SUCCESS );
}

/*****************************************************************************
 * FreeDeviceInfo
 *****************************************************************************/
static void FreeDeviceInfo( UInt32 i_dev, aout_instance_t * p_aout )
{
    UInt32 i;

    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    struct aout_dev_t * p_dev = &p_sys->p_devices[i_dev];

    for( i = 0; i < p_dev->i_streams; i++ )
    {
        FreeStreamInfo( i_dev, p_aout, i );
    }

    free( (void *)p_dev->pp_streams );
    free( (void *)p_dev->pi_streams );
    free( (void *)p_dev->psz_device_name );
}

/*****************************************************************************
 * FreeHardwareInfo
 *****************************************************************************/
static void FreeHardwareInfo( aout_instance_t * p_aout )
{
    UInt32 i;

    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    vlc_mutex_lock( &p_sys->lock );

    if( !p_sys->b_hwinfo )
    {
        vlc_mutex_unlock( &p_sys->lock );
        return;
    }

    for( i = 0; i < p_sys->i_devices; i++ )
    {
        FreeDeviceInfo( i, p_aout );
    }

    free( (void *)p_sys->p_options );
    free( (void *)p_sys->p_devices );

    p_sys->b_hwinfo = VLC_FALSE;

    vlc_mutex_unlock( &p_sys->lock );
}

/*****************************************************************************
 * GetStreamID 
 *****************************************************************************/
static int GetStreamID( AudioDeviceID devid, UInt32 i_idx,
                        AudioStreamID * p_sid )
{
    OSStatus err;
    UInt32 i_param_size;
    AudioStreamID * p_stream_list;

    err = AudioDeviceGetPropertyInfo( devid, 0, FALSE,
                                      kAudioDevicePropertyStreams,
                                      &i_param_size, NULL );
    if( err != noErr )
    {
        return( VLC_EGENERIC );
    }

    p_stream_list = (AudioStreamID *)malloc( i_param_size );
    if( p_stream_list == NULL )
    {
        return( VLC_ENOMEM );
    }

    err = AudioDeviceGetProperty( devid, 0, FALSE,
                                  kAudioDevicePropertyStreams,
                                  &i_param_size, p_stream_list );
    if( err != noErr )
    {
        free( (void *)p_stream_list );
        return( VLC_EGENERIC );
    }

    *p_sid = p_stream_list[i_idx - 1];

    free( (void *)p_stream_list );

    return( VLC_SUCCESS );
}

/*****************************************************************************
 * InitStreamInfo
 *****************************************************************************/
static int InitStreamInfo( UInt32 i_dev, aout_instance_t * p_aout,
                           UInt32 i_idx )
{
    OSStatus err;
    AudioStreamID i_sid;
    UInt32 i, j, i_param_size;

    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    struct aout_dev_t * p_dev = &p_sys->p_devices[i_dev];

    if( GetStreamID( p_dev->devid, i_idx + 1, &i_sid ) )
    {
        msg_Err( p_aout, "GetStreamID(%ld, %ld) failed", i_dev, i_idx );
        return( VLC_EGENERIC );
    }
    err = AudioStreamGetPropertyInfo( i_sid, 0,
                                      kAudioStreamPropertyPhysicalFormats,
                                      &i_param_size, NULL );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not retrieve the number of streams: [%4.4s]",
                 (char *)&err );
        return( VLC_EGENERIC );
    }

#define P_STREAMS p_dev->pp_streams[i_idx]
#define I_STREAMS p_dev->pi_streams[i_idx]

    I_STREAMS = i_param_size / sizeof( AudioStreamBasicDescription );

    P_STREAMS = (AudioStreamBasicDescription *)malloc( i_param_size );
    if( P_STREAMS == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return( VLC_ENOMEM );
    }

    memset( P_STREAMS, 0, i_param_size );

    err = AudioStreamGetProperty( i_sid, 0,
                                  kAudioStreamPropertyPhysicalFormats,
                                  &i_param_size, P_STREAMS );
    if( err != noErr )
    {
        msg_Err( p_aout, "could no get the streams: [%4.4s]",
                 (char *)&err );
        free( (void *)P_STREAMS );
        return( VLC_EGENERIC );
    }

    for( j = 0; j < N_AOUT_CLASSES; j++ )
    {
        vlc_bool_t b_found = 0;

        for( i = 0; i < I_STREAMS; i++ )
        {
            if( j == 0 )
            {
                msg_Dbg( p_aout, STREAM_FORMAT_MSG( "supported format",
                                                    P_STREAMS[i] ) );
            }

            if( ( P_STREAMS[i].mFormatID == 'IAC3' ||
                  P_STREAMS[i].mFormatID == kAudioFormat60958AC3 ) &&
                !AOUT_FMT_NON_LINEAR( &p_aout->output.output ) ) 
            {
                continue;
            }

            if( ( P_STREAMS[i].mFormatID != aout_classes[j].mFormatID ) ||
                ( P_STREAMS[i].mChannelsPerFrame != 
                  aout_classes[j].mChannelsPerFrame ) )
            {
                continue;
            }

            b_found = 1;
            break;
        }

        if( b_found )
        {
            p_sys->p_options = (struct aout_option_t *)
                               realloc( p_sys->p_options, 
                                        ( p_sys->i_options + 1 ) *
                                        sizeof( struct aout_option_t ) ); 
            if( p_sys->p_options == NULL )
            {
                msg_Err( p_aout, "out of memory" );
                free( (void *)P_STREAMS );
                return( VLC_ENOMEM );
            }

#define AOUT_OPTION p_sys->p_options[p_sys->i_options]

            snprintf( AOUT_OPTION.sz_option,
                      sizeof( AOUT_OPTION.sz_option ) / 
                      sizeof( AOUT_OPTION.sz_option[0] ) - 1,
                      "%ld: %s (%s)", 
                      p_sys->i_options,
                      p_dev->psz_device_name, 
                      aout_classes[j].psz_class );

            AOUT_OPTION.i_sid = i_sid;
            AOUT_OPTION.i_dev = i_dev; 
            AOUT_OPTION.i_idx = i_idx;
            AOUT_OPTION.i_sdx = i;
            AOUT_OPTION.i_cdx = j;

#undef AOUT_OPTION

            p_sys->i_options++;
        } 
    }

#undef I_STREAMS
#undef P_STREAMS

    return( VLC_SUCCESS );
}

/*****************************************************************************
 * FreeStreamInfo
 *****************************************************************************/
static void FreeStreamInfo( UInt32 i_dev, aout_instance_t * p_aout,
                            UInt32 i_idx )
{
    struct aout_sys_t * p_sys = p_aout->output.p_sys;
    struct aout_dev_t * p_dev = &p_sys->p_devices[i_dev];

    free( (void *)p_dev->pp_streams[i_idx] );
}

/*****************************************************************************
 * InitDevice 
 *****************************************************************************/
static int InitDevice( aout_instance_t * p_aout ) 
{
    OSStatus err;
    vlc_value_t val;
    unsigned int i_option;
    vlc_bool_t b_found = VLC_FALSE;
    UInt32 i, i_stream, i_param_size, i_firstChannelNum;

    struct aout_dev_t * p_dev;
    struct aout_option_t * p_option;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    if( var_Get( p_aout, "audio-device", &val ) < 0 )
    {
        msg_Err( p_aout, "audio-device var does not exist" );
        return( VLC_ENOVAR );
    }

    i_option = val.i_int;
    p_option = &p_sys->p_options[i_option];
    p_dev = &p_sys->p_devices[p_option->i_dev];

    msg_Dbg( p_aout, "getting device [%ld]", p_option->i_dev );

    i_param_size = sizeof( p_sys->b_dev_alive );
    err = AudioDeviceGetProperty( p_dev->devid, 0, FALSE,
                                  kAudioDevicePropertyDeviceIsAlive,
                                  &i_param_size, &p_sys->b_dev_alive );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not check whether device is alive: %4.4s",
                 (char *)&err );
        return( VLC_EGENERIC );
    }

#define P_STREAMS p_dev->pp_streams[p_option->i_idx]
#define I_STREAMS p_dev->pi_streams[p_option->i_idx]

    for( i = 0; i < I_STREAMS; i++ )
    {
        if( P_STREAMS[i].mFormatID ==
            aout_classes[p_option->i_cdx].mFormatID &&
            P_STREAMS[i].mChannelsPerFrame ==
            aout_classes[p_option->i_cdx].mChannelsPerFrame &&
            P_STREAMS[i].mSampleRate == p_aout->output.output.i_rate )  
        {
            b_found = VLC_TRUE;
            break;
        }
    } 

    i_stream = b_found ? i : p_option->i_sdx;

    i_firstChannelNum = 0;
    for( i = 0; i < i_stream; i++ )
    {
        i_firstChannelNum += P_STREAMS[i].mChannelsPerFrame;
    } 


    i_param_size = sizeof( p_sys->sfmt_revert );
    err = AudioStreamGetProperty( p_option->i_sid, 0,
                                  kAudioStreamPropertyPhysicalFormat,
                                  &i_param_size, 
                                  (void *)&p_sys->sfmt_revert );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not retrieve the original streamformat: [%4.4s]",
                 (char *)&err );
        return( VLC_EGENERIC ); 
    }

    if( memcmp( &P_STREAMS[i_stream], &p_sys->sfmt_revert, 
                sizeof( p_sys->sfmt_revert ) ) != 0 ) 
    {
        struct timeval now;
        struct timespec timeout;
        struct { vlc_mutex_t lock; vlc_cond_t cond; } w;

        vlc_cond_init( p_aout, &w.cond );
        vlc_mutex_init( p_aout, &w.lock );

        msg_Dbg( p_aout, STREAM_FORMAT_MSG( "stream format",
                                            p_sys->sfmt_revert ) );

        err = AudioStreamAddPropertyListener( p_option->i_sid, 0,
                                          kAudioStreamPropertyPhysicalFormat,
                                          StreamListener, (void *)&w );
        if( err != noErr )
        {
            msg_Err( p_aout, "AudioStreamAddPropertyListener failed: [%4.4s]",
                     (char *)&err );
            vlc_mutex_destroy( &w.lock );
            vlc_cond_destroy( &w.cond );
            return( VLC_EGENERIC ); 
        }

        vlc_mutex_lock( &w.lock );

		/* If we do AC3 over SPDIF, take hog mode and non mixable mode */
		if( P_STREAMS[i_stream].mFormatID == kAudioFormat60958AC3 ||
			P_STREAMS[i_stream].mFormatID == 'IAC3' )
		{
			UInt32 b_mix;

			i_param_size = sizeof( p_sys->i_hog_pid );
			err = AudioDeviceGetProperty( p_dev->devid, 0, FALSE, kAudioDevicePropertyHogMode,
											&i_param_size, &p_sys->i_hog_pid );
			if( !err )
			{
				msg_Dbg( p_aout, "Current status of hog mode: pid=%d vlc pid=%d\n", (int)p_sys->i_hog_pid, (int)getpid() );
				if( p_sys->i_hog_pid != getpid() ) {
					p_sys->i_hog_pid = getpid() ;
					err = AudioDeviceSetProperty( p_dev->devid, 0, 0, FALSE,
												 kAudioDevicePropertyHogMode, i_param_size, &p_sys->i_hog_pid );
					if( !err )
					{
						msg_Dbg( p_aout, "Successfully set hog mode - new pid=%d!\n", (int)p_sys->i_hog_pid );

						err = AudioDeviceGetProperty( p_dev->devid, 0, FALSE,
													kAudioDevicePropertyHogMode, &i_param_size, &p_sys->i_hog_pid );
						if( !err )
						{
							msg_Dbg( p_aout, "Checking new status of hog mode: pid=%d vlc pid=%d\n",
								(int)p_sys->i_hog_pid, (int)getpid() );
						}
					}
				}
			}
			
			if( err != noErr )
			{
				msg_Err( p_aout, "failed to set hogmode: : [%4.4s]", (char *)&err );
				FreeDevice( p_aout );
				FreeHardwareInfo( p_aout );
				vlc_mutex_destroy( &p_sys->lock );
				free( (void *)p_sys );
				return( VLC_EGENERIC );
			}
			/*
			i_param_size = sizeof( b_mix );
			err = AudioDeviceGetProperty( p_dev->devid, 0, FALSE, kAudioDevicePropertySupportsMixing,
											&i_param_size, &b_mix );
											
			if( !err )
			{
				msg_Dbg( p_aout, "Current status of mix mode: %i\n", (int)b_mix );
				b_mix = 0;
				err = AudioDeviceSetProperty( p_dev->devid, 0, 0, FALSE,
									kAudioDevicePropertySupportsMixing, i_param_size, &b_mix );
				if( !err )
				{
					msg_Dbg( p_aout, "Successfully set mix mode - new mix=%d!\n", (int)b_mix );

					err = AudioDeviceGetProperty( p_dev->devid, 0, FALSE,
												kAudioDevicePropertySupportsMixing, &i_param_size, &b_mix );
					if( !err )
					{
						msg_Dbg( p_aout, "Checking new status of mix mode: %d\n", (int)b_mix );
					}
				}
			}
			
			if( err != noErr )
			{
				msg_Err( p_aout, "failed to set mixmode: : [%4.4s]", (char *)&err );
				FreeDevice( p_aout );
				FreeHardwareInfo( p_aout );
				vlc_mutex_destroy( &p_sys->lock );
				free( (void *)p_sys );
				return( VLC_EGENERIC );
			}*/
		}

        msg_Dbg( p_aout, STREAM_FORMAT_MSG( "setting format",
                                            P_STREAMS[i_stream] ) );

        err = AudioStreamSetProperty( p_option->i_sid, 0, 0,
                                      kAudioStreamPropertyPhysicalFormat,
                                      sizeof( P_STREAMS[i_stream] ),
                                      &P_STREAMS[i_stream] ); 
        if( err != noErr )
        {
            msg_Err( p_aout, "could not set the stream format: [%4.4s]",
                     (char *)&err );
            vlc_mutex_unlock( &w.lock );
            vlc_mutex_destroy( &w.lock );
            vlc_cond_destroy( &w.cond );
            return( VLC_EGENERIC );
        }

        gettimeofday( &now, NULL );
        timeout.tv_sec = now.tv_sec;
        timeout.tv_nsec = (now.tv_usec + 100000) * 1000;

        pthread_cond_timedwait( &w.cond.cond, &w.lock.mutex, &timeout );
        vlc_mutex_unlock( &w.lock );

        if( GetStreamID( p_dev->devid, p_option->i_idx + 1, 
                         &p_option->i_sid ) )
        {
            msg_Err( p_aout, "GetStreamID(%ld, %ld) failed", 
                     p_option->i_dev, p_option->i_idx );
            vlc_mutex_destroy( &w.lock );
            vlc_cond_destroy( &w.cond );
            return( VLC_EGENERIC );
        }

        err = AudioStreamRemovePropertyListener( p_option->i_sid, 0,
                kAudioStreamPropertyPhysicalFormat, StreamListener );
        if( err != noErr )
        {
            msg_Err( p_aout, 
                    "AudioStreamRemovePropertyListener failed: [%4.4s]",
                    (char *)&err );
            vlc_mutex_destroy( &w.lock );
            vlc_cond_destroy( &w.cond );
            return( VLC_EGENERIC );
        }

        vlc_mutex_destroy( &w.lock );
        vlc_cond_destroy( &w.cond );

        p_sys->b_revert_sfmt = VLC_TRUE;
    }


    err = AudioDeviceAddPropertyListener( p_dev->devid, 0, FALSE,
                                          kAudioDevicePropertyDeviceIsAlive,
                                          DeviceListener, (void *)p_aout );
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioDeviceAddPropertyListener failed: [%4.4s]",
                 (char *)&err );
        return( VLC_EGENERIC );
    } 

    config_PutInt( p_aout, "coreaudio-dev", i_option );

    p_sys->i_sel_opt = i_option;
    p_sys->devid = p_dev->devid;
    p_sys->i_stream_index = p_option->i_idx;
#undef I_STREAMS
#undef P_STREAMS

    return( VLC_SUCCESS );
} 

/*****************************************************************************
 * FreeDevice 
 *****************************************************************************/
static void FreeDevice( aout_instance_t * p_aout ) 
{
    OSStatus err;

    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    if( p_sys->b_revert_sfmt )
    {
        struct aout_dev_t * p_dev;
        struct aout_option_t * p_option;

        p_option = &p_sys->p_options[p_sys->i_sel_opt];
        p_dev = &p_sys->p_devices[p_option->i_dev];

        msg_Dbg( p_aout, STREAM_FORMAT_MSG( "reverting to format",
                                            p_sys->sfmt_revert ) );

        if( GetStreamID( p_dev->devid, p_option->i_idx + 1,
                         &p_option->i_sid ) )
        {
            msg_Err( p_aout, "GetStreamID(%ld, %ld) failed", 
                     p_option->i_dev, p_option->i_idx );
        }
        else
        {
            err = AudioStreamSetProperty( p_option->i_sid, 0, 0,
                                          kAudioStreamPropertyPhysicalFormat,
                                          sizeof( p_sys->sfmt_revert ),
                                          &p_sys->sfmt_revert ); 
            if( err != noErr )
            {
                msg_Err( p_aout, "AudioStreamSetProperty revert format failed: [%4.4s]",
                         (char *)&err );
            }
        }
		
		if( (int)p_sys->i_hog_pid != -1 )
        {
			/*UInt32 b_mix = 1;
			err = AudioDeviceSetProperty( p_sys->devid, 0, 0, FALSE,
                                             kAudioDevicePropertySupportsMixing, sizeof(b_mix), &b_mix );
			
			if( err != noErr )
            {
                msg_Err( p_aout, "Resetting mix mode failed: [%4.4s]",
                         (char *)&err );
            }
			*/
            p_sys->i_hog_pid = (pid_t) -1;
            err = AudioDeviceSetProperty( p_sys->devid, 0, 0, FALSE,
                                             kAudioDevicePropertyHogMode, sizeof(p_sys->i_hog_pid), &p_sys->i_hog_pid );
            
            if( err != noErr )
            {
                msg_Err( p_aout, "Releasing hog mode failed: [%4.4s]",
                         (char *)&err );
            } 
        }
    }

    err = AudioDeviceRemovePropertyListener( p_sys->devid, 0, FALSE,
                                             kAudioDevicePropertyDeviceIsAlive,
                                             DeviceListener );
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioDeviceRemovePropertyListener failed: [%4.4s]",
                 (char *)&err );
    } 
}

/*****************************************************************************
 * HardwareListener 
 *****************************************************************************/
static OSStatus HardwareListener( AudioHardwarePropertyID inPropertyID,
                                  void * inClientData )
{
    OSStatus err = noErr;

    aout_instance_t * p_aout = (aout_instance_t *)inClientData;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    switch( inPropertyID )
    {
        case kAudioHardwarePropertyDevices:
        {
            UInt32 i_idx = 0;
            UInt32 i_sdx = 0;
            int i_option = -1;

            if( p_sys->b_dev_alive )
            {
                i_idx = p_sys->p_options[p_sys->i_sel_opt].i_idx;
                i_sdx = p_sys->p_options[p_sys->i_sel_opt].i_sdx;
            }

            FreeHardwareInfo( p_aout );

            if( InitHardwareInfo( p_aout ) )
            {
                msg_Err( p_aout, "InitHardwareInfo failed" );
                break;
            }

            if( p_sys->b_dev_alive )
            {
                UInt32 i;

                for( i = 0; i < p_sys->i_options; i++ )
                {
                    if( p_sys->p_devices[p_sys->p_options[i].i_dev].devid ==
                        p_sys->devid && p_sys->p_options[i].i_idx == i_idx &&
                        p_sys->p_options[i].i_sdx == i_sdx )
                    {
                        i_option = i;
                        break;
                    }
                }
            }

            var_Destroy( p_aout, "audio-device" );
            InitDeviceVar( p_aout, i_option, !p_sys->b_dev_alive );
        }
        break;
    }

    return( err );
}

/*****************************************************************************
 * DeviceListener 
 *****************************************************************************/
static OSStatus DeviceListener( AudioDeviceID inDevice,
                                UInt32 inChannel,
                                Boolean isInput,
                                AudioDevicePropertyID inPropertyID,
                                void *inClientData )
{
    UInt32 i_param_size;
    OSStatus err = noErr;

    aout_instance_t * p_aout = (aout_instance_t *)inClientData;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    switch( inPropertyID )
    {
        case kAudioDevicePropertyDeviceIsAlive:
        {
            i_param_size = sizeof( p_sys->b_dev_alive );
            err = AudioDeviceGetProperty( p_sys->devid, 0, FALSE, 
                                          kAudioDevicePropertyDeviceIsAlive,
                                          &i_param_size, &p_sys->b_dev_alive );
            if( err != noErr )
            {
                msg_Err( p_aout, "could not determine wether device is alive: %4.4s",
                         (char *)&err );
            }
        }
        break;
    }

    return( err );
}

/*****************************************************************************
 * StreamListener 
 *****************************************************************************/
static OSStatus StreamListener( AudioStreamID inStream,
                                UInt32 inChannel,
                                AudioDevicePropertyID inPropertyID,
                                void * inClientData )
{
    OSStatus err = noErr;
    struct { vlc_mutex_t lock; vlc_cond_t cond; } * w = inClientData;

    switch( inPropertyID )
    {
        case kAudioStreamPropertyPhysicalFormat:
            vlc_mutex_lock( &w->lock );
            vlc_cond_signal( &w->cond );
            vlc_mutex_unlock( &w->lock ); 
            break;

        default:
            break;
    }
    return( err );
}

/*****************************************************************************
 * InitDeviceVar
 *****************************************************************************/
static void InitDeviceVar( aout_instance_t * p_aout, int i_option,
                           vlc_bool_t b_change )
{ 
    UInt32 i;
    vlc_value_t val, text;

    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    if( i_option == -1 || i_option >= (int)p_sys->i_options )
    {
        for( i = 0; i < p_sys->i_options; i++ )
        {
            if( p_sys->p_options[i].i_dev == p_sys->i_def_dev )
            {
                i_option = i;
                break;
            }
        }
    }

    var_Create( p_aout, "audio-device", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = ADEV_TEXT;
    var_Change( p_aout, "audio-device", VLC_VAR_SETTEXT, &text, NULL );

    for( i = 0; i < p_sys->i_options; i++ )
    {
        text.psz_string = p_sys->p_options[i].sz_option;
        val.i_int = i;
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text );

        if( !b_change && i == (UInt32)i_option )
        {
            p_sys->i_sel_opt = i;
            var_Set( p_aout, "audio-device", val );
            config_PutInt( p_aout, "coreaudio-dev", i_option );
        }
    }

    var_AddCallback( p_aout, "audio-device", aout_ChannelsRestart,
                     NULL );

    if( b_change )
    {
        val.i_int = i_option;
        var_Set( p_aout, "audio-device", val );
    }

    val.b_bool = VLC_TRUE;
    var_Set( p_aout, "intf-change", val );
}
