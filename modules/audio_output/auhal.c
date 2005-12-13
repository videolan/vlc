/*****************************************************************************
 * auhal.c: AUHAL output plugin
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
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
#include <unistd.h>

#include <vlc/vlc.h>
#include <vlc/aout.h>

#include "aout_internal.h"

#include <CoreAudio/CoreAudio.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <AudioUnit/AudioUnitProperties.h>
#include <AudioUnit/AudioUnitParameters.h>
#include <AudioUnit/AudioOutputUnit.h>
#include <AudioToolbox/AudioFormat.h>

#define STREAM_FORMAT_MSG( pre, sfm ) \
    pre "[%ld][%4.4s][%ld][%ld][%ld][%ld][%ld][%ld]", \
    (UInt32)sfm.mSampleRate, (char *)&sfm.mFormatID, \
    sfm.mFormatFlags, sfm.mBytesPerPacket, \
    sfm.mFramesPerPacket, sfm.mBytesPerFrame, \
    sfm.mChannelsPerFrame, sfm.mBitsPerChannel

#define STREAM_FORMAT_MSG_FULL( pre, sfm ) \
    pre ":\nsamplerate: [%ld]\nFormatID: [%4.4s]\nFormatFlags: [%ld]\nBypesPerPacket: [%ld]\nFramesPerPacket: [%ld]\nBytesPerFrame: [%ld]\nChannelsPerFrame: [%ld]\nBitsPerChannel[%ld]", \
    (UInt32)sfm.mSampleRate, (char *)&sfm.mFormatID, \
    sfm.mFormatFlags, sfm.mBytesPerPacket, \
    sfm.mFramesPerPacket, sfm.mBytesPerFrame, \
    sfm.mChannelsPerFrame, sfm.mBitsPerChannel

#define BUFSIZE 0xffffff
#define AOUT_VAR_SPDIF_FLAG 0xf00000

/*
 * TODO:
 * - clean up the debug info
 * - clean up C99'isms
 * - be better at changing stream setup or devices setup changes while playing.
 */

/*****************************************************************************
 * aout_sys_t: private audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the CoreAudio specific properties of an output thread.
 *****************************************************************************/
struct aout_sys_t
{
    AudioDeviceID               i_default_dev;  /* Keeps DeviceID of defaultOutputDevice */
    AudioDeviceID               i_selected_dev; /* Keeps DeviceID of the selected device */
    UInt32                      i_devices;      /* Number of CoreAudio Devices */
    vlc_bool_t                  b_supports_digital;/* Does the currently selected device support digital mode? */
    vlc_bool_t                  b_digital;      /* Are we running in digital mode? */
    mtime_t                     clock_diff;     /* Difference between VLC clock and Device clock */
    /* AUHAL specific */
    Component                   au_component;   /* The Audiocomponent we use */
    AudioUnit                   au_unit;        /* The AudioUnit we use */
    uint8_t                     p_remainder_buffer[BUFSIZE];
    uint32_t                    i_read_bytes;
    uint32_t                    i_total_bytes;
    /* CoreAudio SPDIF mode specific */
    pid_t                       i_hog_pid;      /* The keep the pid of our hog status */
    AudioStreamID               i_stream_id;    /* The StreamID that has a cac3 streamformat */
    int                         i_stream_index; /* The index of i_stream_id in an AudioBufferList */
    AudioStreamBasicDescription stream_format;  /* The format we changed the to */
    AudioStreamBasicDescription sfmt_revert;    /* The original format of the stream */
    vlc_bool_t                  b_revert;       /* Wether we need to revert the stream format */
    vlc_bool_t                  b_changed_mixing;/* Wether we need to set the mixing mode back */
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int      Open                    ( vlc_object_t * );
static int      OpenAnalog              ( aout_instance_t * );
static int      OpenSPDIF               ( aout_instance_t * );
static void     Close                   ( vlc_object_t * );

static void     Play                    ( aout_instance_t * );
static void     Probe                   ( aout_instance_t * );

static int      AudioDeviceHasOutput    ( AudioDeviceID );
static int      AudioDeviceSupportsDigital( aout_instance_t *, AudioDeviceID );
static int      AudioStreamSupportsDigital( aout_instance_t *, AudioStreamID );

static OSStatus RenderCallbackAnalog    ( vlc_object_t *, AudioUnitRenderActionFlags *, const AudioTimeStamp *,
                                          unsigned int, unsigned int, AudioBufferList *);
static OSStatus RenderCallbackSPDIF     ( AudioDeviceID, const AudioTimeStamp *, const void *, const AudioTimeStamp *,
                                          AudioBufferList *, const AudioTimeStamp *, void * );
static OSStatus HardwareListener        ( AudioHardwarePropertyID, void *);
static OSStatus StreamListener          ( AudioStreamID, UInt32,
                                          AudioDevicePropertyID, void * );
static int      AudioDeviceCallback     ( vlc_object_t *, const char *,
                                          vlc_value_t, vlc_value_t, void * );


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ADEV_TEXT N_("Audio Device")
#define ADEV_LONGTEXT N_("Choose a number corresponding to the number of an " \
    "audio device, as listed in your 'Audio Device' menu. This device will " \
    "then be used by default for audio playback.")

vlc_module_begin();
    set_shortname( "auhal" );
    set_description( _("HAL AudioUnit output") );
    set_capability( "audio output", 101 );
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_AOUT );
    set_callbacks( Open, Close );
    add_integer( "macosx-audio-device", 0, NULL, ADEV_TEXT, ADEV_LONGTEXT, VLC_FALSE ); 
vlc_module_end();

/*****************************************************************************
 * Open: open macosx audio output
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    OSStatus                err = noErr;
    UInt32                  i_param_size = 0;
    struct aout_sys_t       *p_sys = NULL;
    vlc_bool_t              b_alive = VLC_FALSE;
    vlc_value_t             val;
    aout_instance_t         *p_aout = (aout_instance_t *)p_this;

    /* Allocate structure */
    p_aout->output.p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->output.p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return( VLC_ENOMEM );
    }

    p_sys = p_aout->output.p_sys;
    p_sys->i_default_dev = 0;
    p_sys->i_selected_dev = 0;
    p_sys->i_devices = 0;
    p_sys->b_supports_digital = VLC_FALSE;
    p_sys->b_digital = VLC_FALSE;
    p_sys->au_component = NULL;
    p_sys->au_unit = NULL;
    p_sys->clock_diff = (mtime_t) 0;
    p_sys->i_read_bytes = 0;
    p_sys->i_total_bytes = 0;
    p_sys->i_hog_pid = -1;
    p_sys->i_stream_id = 0;
    p_sys->i_stream_index = 0;
    p_sys->b_revert = VLC_FALSE;
    p_sys->b_changed_mixing = VLC_FALSE;
    memset( p_sys->p_remainder_buffer, 0, sizeof(uint8_t) * BUFSIZE );

    p_aout->output.pf_play = Play;
    
    aout_FormatPrint( p_aout, "VLC is looking for:", (audio_sample_format_t *)&p_aout->output.output );
    
    /* Persistent device variable */
    if( var_Type( p_aout->p_vlc, "macosx-audio-device" ) == 0 )
    {
        msg_Dbg( p_aout, "create macosx-audio-device" );
        var_Create( p_aout->p_vlc, "macosx-audio-device", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    }

    /* Build a list of devices */
    if( var_Type( p_aout, "audio-device" ) == 0 )
    {
        Probe( p_aout );
    }

    /* What device do we want? */
    if( var_Get( p_aout, "audio-device", &val ) < 0 )
    {
        msg_Err( p_aout, "audio-device var does not exist. device probe failed." );
        free( p_sys );
        return( VLC_ENOVAR );
    }

    p_sys->i_selected_dev = val.i_int & ~AOUT_VAR_SPDIF_FLAG;
    p_sys->b_supports_digital = ( val.i_int & AOUT_VAR_SPDIF_FLAG ) ? VLC_TRUE : VLC_FALSE;

    /* Check if the desired device is alive and usable */
    i_param_size = sizeof( b_alive );
    err = AudioDeviceGetProperty( p_sys->i_selected_dev, 0, FALSE,
                                  kAudioDevicePropertyDeviceIsAlive,
                                  &i_param_size, &b_alive );

    if( err != noErr )
    {
        msg_Err( p_aout, "could not check whether device is alive: %4.4s",
                 (char *)&err );
        return VLC_EGENERIC;
    }

    if( b_alive == VLC_FALSE )
    {
        msg_Err( p_aout, "Selected audio device is not alive switching to default device" ); 
        p_sys->i_selected_dev = p_sys->i_default_dev;
    }

    i_param_size = sizeof( p_sys->i_hog_pid );
    err = AudioDeviceGetProperty( p_sys->i_selected_dev, 0, FALSE,
                                  kAudioDevicePropertyHogMode,
                                  &i_param_size, &p_sys->i_hog_pid );

    if( err != noErr )
    {
        msg_Err( p_aout, "could not check whether device is hogged: %4.4s",
                 (char *)&err );
        return VLC_EGENERIC;
    }

    if( p_sys->i_hog_pid != -1 && p_sys->i_hog_pid != getpid() )
    {
        msg_Err( p_aout, "Selected audio device is exclusively in use by another program" );
        var_Destroy( p_aout, "audio-device" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Check for Digital mode or Analog output mode */
    if( AOUT_FMT_NON_LINEAR( &p_aout->output.output ) && p_sys->b_supports_digital )
    {
        if( OpenSPDIF( p_aout ) )
            return VLC_SUCCESS;
    }
    else
    {
        if( OpenAnalog( p_aout ) )
            return VLC_SUCCESS;
    }
    
    /* If we reach this, the Open* failed */
    var_Destroy( p_aout, "audio-device" );
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Open: open and setup a HAL AudioUnit
 *****************************************************************************/
static int OpenAnalog( aout_instance_t *p_aout )
{
    struct aout_sys_t       *p_sys = p_aout->output.p_sys;
    OSStatus                err = noErr;
    UInt32                  i_param_size = 0, i = 0;
    ComponentDescription    desc;
        
    if( AOUT_FMT_NON_LINEAR( &p_aout->output.output ) && !p_sys->b_supports_digital )
    {
        msg_Dbg( p_aout, "we had requested a digital stream, but it's not possible for this device" );
    }

    /* If analog only start setting up AUHAL */
    /* Lets go find our Component */
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    p_sys->au_component = FindNextComponent( NULL, &desc );
    if( p_sys->au_component == NULL )
    {
        msg_Err( p_aout, "we cannot find our HAL component" );
        return VLC_FALSE;
    }

    err = OpenAComponent( p_sys->au_component, &p_sys->au_unit );
    if( err )
    {
        msg_Err( p_aout, "we cannot find our HAL component" );
        return VLC_FALSE;
    }
    
    /* Enable IO for the component */
    msg_Dbg( p_aout, "Device: %#x", (int)p_sys->i_selected_dev );
    
    /* Set the device */
    verify_noerr( AudioUnitSetProperty( p_sys->au_unit,
                         kAudioOutputUnitProperty_CurrentDevice,
                         kAudioUnitScope_Global,
                         0,
                         &p_sys->i_selected_dev,
                         sizeof( AudioDeviceID )));
                         
    /* Get the current format */
    AudioStreamBasicDescription DeviceFormat;
    
    i_param_size = sizeof(AudioStreamBasicDescription);

    verify_noerr( AudioUnitGetProperty( p_sys->au_unit,
                                   kAudioUnitProperty_StreamFormat,
                                   kAudioUnitScope_Input,
                                   0,
                                   &DeviceFormat,
                                   &i_param_size ));
                                   
    msg_Dbg( p_aout, STREAM_FORMAT_MSG( "current format is: " , DeviceFormat ) );

    /* Get the channel layout */
    AudioChannelLayout *layout;
    verify_noerr( AudioUnitGetPropertyInfo( p_sys->au_unit,
                                   kAudioDevicePropertyPreferredChannelLayout,
                                   kAudioUnitScope_Output,
                                   0,
                                   &i_param_size,
                                   NULL ));

    layout = (AudioChannelLayout *)malloc( i_param_size);

    verify_noerr( AudioUnitGetProperty( p_sys->au_unit,
                                   kAudioDevicePropertyPreferredChannelLayout,
                                   kAudioUnitScope_Output,
                                   0,
                                   layout,
                                   &i_param_size ));
                                   
    /* Lets fill out the ChannelLayout */
    if( layout->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelBitmap)
    {
        msg_Dbg( p_aout, "bitmap defined channellayout" );
        verify_noerr( AudioFormatGetProperty( kAudioFormatProperty_ChannelLayoutForBitmap,
                                sizeof( UInt32), &layout->mChannelBitmap,
                                &i_param_size,
                                layout ));
    }
    else if( layout->mChannelLayoutTag != kAudioChannelLayoutTag_UseChannelDescriptions )
    {
        msg_Dbg( p_aout, "layouttags defined channellayout" );
        verify_noerr( AudioFormatGetProperty( kAudioFormatProperty_ChannelLayoutForTag,
                                sizeof( AudioChannelLayoutTag ), &layout->mChannelLayoutTag,
                                &i_param_size,
                                layout ));
    }

    msg_Dbg( p_aout, "Layout of AUHAL has %d channels" , (int)layout->mNumberChannelDescriptions );
    
    p_aout->output.output.i_physical_channels = 0;
    int i_original = p_aout->output.output.i_original_channels & AOUT_CHAN_PHYSMASK;
    
    if( i_original == AOUT_CHAN_CENTER || layout->mNumberChannelDescriptions < 2 )
    {
        // We only need Mono or cannot output more
        p_aout->output.output.i_physical_channels |= AOUT_CHAN_CENTER;
    }
    else if( i_original == (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT) || layout->mNumberChannelDescriptions < 3 )
    {
        // We only need Stereo or cannot output more
        p_aout->output.output.i_physical_channels |= AOUT_CHAN_RIGHT;
        p_aout->output.output.i_physical_channels |= AOUT_CHAN_LEFT;
    }
    else
    {
        // We want more then stereo and we can do that
        for( i = 0; i < layout->mNumberChannelDescriptions; i++ )
        {
            msg_Dbg( p_aout, "This is channel: %d", (int)layout->mChannelDescriptions[i].mChannelLabel );

            switch( layout->mChannelDescriptions[i].mChannelLabel )
            {
                case kAudioChannelLabel_Left:
                    p_aout->output.output.i_physical_channels |= AOUT_CHAN_LEFT;
                    continue;
                case kAudioChannelLabel_Right:
                    p_aout->output.output.i_physical_channels |= AOUT_CHAN_RIGHT;
                    continue;
                case kAudioChannelLabel_Center:
                    p_aout->output.output.i_physical_channels |= AOUT_CHAN_CENTER;
                    continue;
                case kAudioChannelLabel_LFEScreen:
                    p_aout->output.output.i_physical_channels |= AOUT_CHAN_LFE;
                    continue;
                case kAudioChannelLabel_LeftSurround:
                    p_aout->output.output.i_physical_channels |= AOUT_CHAN_REARLEFT;
                    continue;
                case kAudioChannelLabel_RightSurround:
                    p_aout->output.output.i_physical_channels |= AOUT_CHAN_REARRIGHT;
                    continue;
                case kAudioChannelLabel_RearSurroundLeft:
                    p_aout->output.output.i_physical_channels |= AOUT_CHAN_MIDDLELEFT;
                    continue;
                case kAudioChannelLabel_RearSurroundRight:
                    p_aout->output.output.i_physical_channels |= AOUT_CHAN_MIDDLERIGHT;
                    continue;
                case kAudioChannelLabel_CenterSurround:
                    p_aout->output.output.i_physical_channels |= AOUT_CHAN_REARCENTER;
                    continue;
                default:
                    msg_Warn( p_aout, "Unrecognized channel form provided by driver: %d", (int)layout->mChannelDescriptions[i].mChannelLabel );
                    if( i == 0 )
                    {
                        msg_Warn( p_aout, "Probably no channellayout is set. force based on channelcount" );
                        switch( layout->mNumberChannelDescriptions )
                        {
                            /* We make assumptions based on number of channels here.
                             * Unfortunatly Apple has provided no 100% method to retrieve the speaker configuration */
                            case 1:
                                p_aout->output.output.i_physical_channels = AOUT_CHAN_CENTER;
                                break;
                            case 4:
                                p_aout->output.output.i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                                                            AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
                                break;
                            case 6:
                                p_aout->output.output.i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                                                            AOUT_CHAN_CENTER | AOUT_CHAN_LFE |
                                                                            AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
                                break;
                            case 7:
                                p_aout->output.output.i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                                                            AOUT_CHAN_CENTER | AOUT_CHAN_LFE |
                                                                            AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT |
                                                                            AOUT_CHAN_REARCENTER;
                                break;
                            case 8:
                                p_aout->output.output.i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                                                            AOUT_CHAN_CENTER | AOUT_CHAN_LFE |
                                                                            AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT |
                                                                            AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT;
                                break;
                            case 2:
                            default:
                                p_aout->output.output.i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
                        }
                    }
                    break;
            }
        }
    }
    if( layout ) free( layout );

    msg_Dbg( p_aout, "we want to output these channels: %#x", p_aout->output.output.i_original_channels);
    msg_Dbg( p_aout, "selected %d physical channels for device output", aout_FormatNbChannels( &p_aout->output.output ) );
    msg_Dbg( p_aout, "%s", aout_FormatPrintChannels( &p_aout->output.output ));

    AudioChannelLayout new_layout;
    memset (&new_layout, 0, sizeof(new_layout));
    switch( aout_FormatNbChannels( &p_aout->output.output ) )
    {
        case 1:
            new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;
            break;
        case 2:
            new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
            break;
        case 3:
            if( p_aout->output.output.i_physical_channels & AOUT_CHAN_CENTER )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_7; // L R C
            }
            else if( p_aout->output.output.i_physical_channels & AOUT_CHAN_LFE )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_4; // L R LFE
            }
            break;
        case 4:
            if( p_aout->output.output.i_physical_channels & ( AOUT_CHAN_CENTER | AOUT_CHAN_LFE ) )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_10; // L R C LFE
            }
            else if( p_aout->output.output.i_physical_channels & ( AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT ) )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_3; // L R Ls Rs
            }
            else if( p_aout->output.output.i_physical_channels & ( AOUT_CHAN_CENTER | AOUT_CHAN_REARCENTER ) )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_3; // L R C Cs
            }
            break;
        case 5:
            if( p_aout->output.output.i_physical_channels & ( AOUT_CHAN_CENTER ) )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_19; // L R Ls Rs C
            }
            else if( p_aout->output.output.i_physical_channels & ( AOUT_CHAN_LFE ) )
            {
                new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_18; // L R Ls Rs LFE
            }
            break;
        case 6:
            new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_DVD_20; // L R Ls Rs C LFE
            break;
        case 7:
            /* FIXME: This is incorrect. VLC uses the internal ordering: L R Lm Rm Lr Rr C LFE but this is wrong */
            new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_MPEG_6_1_A; // L R C LFE Ls Rs Cs
            break;
        case 8:
            /* FIXME: This is incorrect. VLC uses the internal ordering: L R Lm Rm Lr Rr C LFE but this is wrong */
            new_layout.mChannelLayoutTag = kAudioChannelLayoutTag_MPEG_7_1_A; // L R C LFE Ls Rs Lc Rc
            break;
    }

    /* Set up the format to be used */
    DeviceFormat.mSampleRate = p_aout->output.output.i_rate;
    DeviceFormat.mFormatID = kAudioFormatLinearPCM;

    /* We use float 32. It's the best supported format by both VLC and Coreaudio */
    p_aout->output.output.i_format = VLC_FOURCC( 'f','l','3','2');
    DeviceFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    DeviceFormat.mBitsPerChannel = 32;
    DeviceFormat.mChannelsPerFrame = aout_FormatNbChannels( &p_aout->output.output );
    
    /* Calculate framesizes and stuff */
    aout_FormatPrepare( &p_aout->output.output );
    DeviceFormat.mFramesPerPacket = 1;
    DeviceFormat.mBytesPerFrame = DeviceFormat.mBitsPerChannel * DeviceFormat.mChannelsPerFrame / 8;
    DeviceFormat.mBytesPerPacket = DeviceFormat.mBytesPerFrame * DeviceFormat.mFramesPerPacket;
 
    i_param_size = sizeof(AudioStreamBasicDescription);
    /* Set desired format (Use CAStreamBasicDescription )*/
    verify_noerr( AudioUnitSetProperty( p_sys->au_unit,
                                   kAudioUnitProperty_StreamFormat,
                                   kAudioUnitScope_Input,
                                   0,
                                   &DeviceFormat,
                                   i_param_size ));
                                   
    msg_Dbg( p_aout, STREAM_FORMAT_MSG( "we set the AU format: " , DeviceFormat ) );
    
    /* Retrieve actual format??? */
    verify_noerr( AudioUnitGetProperty( p_sys->au_unit,
                                   kAudioUnitProperty_StreamFormat,
                                   kAudioUnitScope_Input,
                                   0,
                                   &DeviceFormat,
                                   &i_param_size ));
                                   
    msg_Dbg( p_aout, STREAM_FORMAT_MSG( "the actual set AU format is " , DeviceFormat ) );

    p_aout->output.i_nb_samples = 2048;
    aout_VolumeSoftInit( p_aout );

    /* Find the difference between device clock and mdate clock */
    p_sys->clock_diff = - (mtime_t)
        AudioConvertHostTimeToNanos( AudioGetCurrentHostTime() ) / 1000; 
    p_sys->clock_diff += mdate();

    /* set the IOproc callback */
    AURenderCallbackStruct input;
    input.inputProc = (AURenderCallback) RenderCallbackAnalog;
    input.inputProcRefCon = p_aout;
    
    verify_noerr( AudioUnitSetProperty( p_sys->au_unit,
                            kAudioUnitProperty_SetRenderCallback,
                            kAudioUnitScope_Input,
                            0, &input, sizeof( input ) ) );

    input.inputProc = (AURenderCallback) RenderCallbackAnalog;
    input.inputProcRefCon = p_aout;
    
    /* Set the new_layout as the layout VLC feeds to the AU unit */
    verify_noerr( AudioUnitSetProperty( p_sys->au_unit,
                            kAudioUnitProperty_AudioChannelLayout,
                            kAudioUnitScope_Input,
                            0, &new_layout, sizeof(new_layout) ) );
    
    /* AU initiliaze */
    verify_noerr( AudioUnitInitialize(p_sys->au_unit) );

    verify_noerr( AudioOutputUnitStart(p_sys->au_unit) );
    
    return VLC_TRUE;
}

/*****************************************************************************
 * Setup a encoded digital stream (SPDIF)
 *****************************************************************************/
static int OpenSPDIF( aout_instance_t * p_aout )
{
    struct aout_sys_t       *p_sys = p_aout->output.p_sys;
    OSStatus                err = noErr;
    UInt32                  i_param_size = 0, b_mix = 0;
    Boolean                 b_writeable = VLC_FALSE;
    AudioStreamID           *p_streams = NULL;
    int                     i = 0, i_streams = 0;

    struct timeval now;
    struct timespec timeout;
    struct { vlc_mutex_t lock; vlc_cond_t cond; } w;

    /* Start doing the SPDIF setup proces */
    p_sys->b_digital = VLC_TRUE;
    msg_Dbg( p_aout, "opening in SPDIF mode" );

    /* Hog the device */
    i_param_size = sizeof( p_sys->i_hog_pid );
    p_sys->i_hog_pid = getpid() ;
    
    err = AudioDeviceSetProperty( p_sys->i_selected_dev, 0, 0, FALSE,
                                  kAudioDevicePropertyHogMode, i_param_size, &p_sys->i_hog_pid );
    
    if( err != noErr )
    {
        msg_Err( p_aout, "failed to set hogmode: : [%4.4s]", (char *)&err );
        return VLC_FALSE;
    }

    /* Set mixable to false if we are allowed to */
    err = AudioDeviceGetPropertyInfo( p_sys->i_selected_dev, 0, FALSE, kAudioDevicePropertySupportsMixing,
                                    &i_param_size, &b_writeable );

    err = AudioDeviceGetProperty( p_sys->i_selected_dev, 0, FALSE, kAudioDevicePropertySupportsMixing,
                                    &i_param_size, &b_mix );
                                    
    if( !err && b_writeable )
    {
        b_mix = 0;
        err = AudioDeviceSetProperty( p_sys->i_selected_dev, 0, 0, FALSE,
                            kAudioDevicePropertySupportsMixing, i_param_size, &b_mix );
        p_sys->b_changed_mixing = VLC_TRUE;
    }
    
    if( err != noErr )
    {
        msg_Err( p_aout, "failed to set mixmode: [%4.4s]", (char *)&err );
        return VLC_FALSE;
    }

    // Find stream_id of selected device with a cac3 stream
    err = AudioDeviceGetPropertyInfo( p_sys->i_selected_dev, 0, FALSE,
                                      kAudioDevicePropertyStreams,
                                      &i_param_size, NULL );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get number of streams: [%4.4s]", (char *)&err );
        return VLC_FALSE;
    }
    
    i_streams = i_param_size / sizeof( AudioStreamID );
    p_streams = (AudioStreamID *)malloc( i_param_size );
    if( p_streams == NULL )
    {
        msg_Err( p_aout, "Out of memory" );
        return VLC_FALSE;
    }
    
    err = AudioDeviceGetProperty( p_sys->i_selected_dev, 0, FALSE,
                                    kAudioDevicePropertyStreams,
                                    &i_param_size, p_streams );
    
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get number of streams: [%4.4s]", (char *)&err );
        if( p_streams ) free( p_streams );
        return VLC_FALSE;
    }

    for( i = 0; i < i_streams; i++ )
    {
        // Find a stream with a cac3 stream
        AudioStreamBasicDescription *p_format_list = NULL;
        int                         i_formats = 0, j = 0;
        
        /* Retrieve all the stream formats supported by each output stream */
        err = AudioStreamGetPropertyInfo( p_streams[i], 0,
                                          kAudioStreamPropertyPhysicalFormats,
                                          &i_param_size, NULL );
        if( err != noErr )
        {
            msg_Err( p_aout, "could not get number of streamformats: [%4.4s]", (char *)&err );
            continue;
        }
        
        i_formats = i_param_size / sizeof( AudioStreamBasicDescription );
        p_format_list = (AudioStreamBasicDescription *)malloc( i_param_size );
        if( p_format_list == NULL )
        {
            msg_Err( p_aout, "Could not malloc the memory" );
            continue;
        }
        
        err = AudioStreamGetProperty( p_streams[i], 0,
                                          kAudioStreamPropertyPhysicalFormats,
                                          &i_param_size, p_format_list );
        if( err != noErr )
        {
            msg_Err( p_aout, "could not get the list of streamformats: [%4.4s]", (char *)&err );
            if( p_format_list) free( p_format_list);
            continue;
        }

        for( j = 0; j < i_formats; j++ )
        {
            if( p_format_list[j].mFormatID == 'IAC3' ||
                  p_format_list[j].mFormatID == kAudioFormat60958AC3 )
            {
                // found a cac3 format
                p_sys->i_stream_id = p_streams[i];
                p_sys->i_stream_index = i;

                if( p_sys->b_revert == VLC_FALSE )
                {
                    i_param_size = sizeof( p_sys->sfmt_revert );
                    err = AudioStreamGetProperty( p_sys->i_stream_id, 0,
                                                  kAudioStreamPropertyPhysicalFormat,
                                                  &i_param_size, 
                                                  &p_sys->sfmt_revert );
                    if( err != noErr )
                    {
                        msg_Err( p_aout, "could not retrieve the original streamformat: [%4.4s]", (char *)&err );
                        continue; 
                    }
                    p_sys->b_revert = VLC_TRUE;
                }
                if( p_format_list[j].mSampleRate == p_sys->sfmt_revert.mSampleRate )
                {
                    p_sys->stream_format = p_format_list[j];
                }
            }
        }
        if( p_format_list ) free( p_format_list );
    }
    
    if( p_streams ) free( p_streams );

    msg_Dbg( p_aout, STREAM_FORMAT_MSG( "original stream format: ", p_sys->sfmt_revert ) );
    msg_Dbg( p_aout, STREAM_FORMAT_MSG( "setting stream format: ", p_sys->stream_format ) );

    /* Install the callback */
    err = AudioStreamAddPropertyListener( p_sys->i_stream_id, 0,
                                      kAudioStreamPropertyPhysicalFormat,
                                      StreamListener, (void *)&w );
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioStreamAddPropertyListener failed: [%4.4s]", (char *)&err );
        return VLC_FALSE;
    }

    /* Condition because SetProperty is asynchronious */ 
    vlc_cond_init( p_aout, &w.cond );
    vlc_mutex_init( p_aout, &w.lock );
    vlc_mutex_lock( &w.lock );
    
    /* change the format */
    err = AudioStreamSetProperty( p_sys->i_stream_id, 0, 0,
                                  kAudioStreamPropertyPhysicalFormat,
                                  sizeof( AudioStreamBasicDescription ),
                                  &p_sys->stream_format ); 
    if( err != noErr )
    {
        msg_Err( p_aout, "could not set the stream format: [%4.4s]", (char *)&err );
        vlc_mutex_unlock( &w.lock );
        vlc_mutex_destroy( &w.lock );
        vlc_cond_destroy( &w.cond );
        return VLC_FALSE;
    }

    gettimeofday( &now, NULL );
    timeout.tv_sec = now.tv_sec;
    timeout.tv_nsec = (now.tv_usec + 900000) * 1000;

    pthread_cond_timedwait( &w.cond.cond, &w.lock.mutex, &timeout );
    vlc_mutex_unlock( &w.lock );

    err = AudioStreamRemovePropertyListener( p_sys->i_stream_id, 0,
                                        kAudioStreamPropertyPhysicalFormat,
                                        StreamListener );
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioStreamRemovePropertyListener failed: [%4.4s]", (char *)&err );
        vlc_mutex_destroy( &w.lock );
        vlc_cond_destroy( &w.cond );
        return VLC_FALSE;
    }

    vlc_mutex_destroy( &w.lock );
    vlc_cond_destroy( &w.cond );
    
    i_param_size = sizeof( AudioStreamBasicDescription );
    err = AudioStreamGetProperty( p_sys->i_stream_id, 0,
                                  kAudioStreamPropertyPhysicalFormat,
                                  &i_param_size, 
                                  &p_sys->stream_format );

    msg_Dbg( p_aout, STREAM_FORMAT_MSG( "actual format in use: ", p_sys->stream_format ) );

    /* set the format flags */
    if( p_sys->stream_format.mFormatFlags & kAudioFormatFlagIsBigEndian )
        p_aout->output.output.i_format = VLC_FOURCC('s','p','d','b');
    else
        p_aout->output.output.i_format = VLC_FOURCC('s','p','d','i');
    p_aout->output.output.i_bytes_per_frame = AOUT_SPDIF_SIZE;
    p_aout->output.output.i_frame_length = A52_FRAME_NB;
    p_aout->output.i_nb_samples = p_aout->output.output.i_frame_length;
    p_aout->output.output.i_rate = (unsigned int)p_sys->stream_format.mSampleRate;

    aout_VolumeNoneInit( p_aout );

    /* Add IOProc callback */
    err = AudioDeviceAddIOProc( p_sys->i_selected_dev,
                                (AudioDeviceIOProc)RenderCallbackSPDIF,
                                (void *)p_aout );
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioDeviceAddIOProc failed: [%4.4s]", (char *)&err );
        return VLC_FALSE;
    }

    /* Check for the difference between the Device clock and mdate */
    p_sys->clock_diff = - (mtime_t)
        AudioConvertHostTimeToNanos( AudioGetCurrentHostTime() ) / 1000; 
    p_sys->clock_diff += mdate();
 
    /* Start device */
    err = AudioDeviceStart( p_sys->i_selected_dev, (AudioDeviceIOProc)RenderCallbackSPDIF ); 
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioDeviceStart failed: [%4.4s]", (char *)&err );

        err = AudioDeviceRemoveIOProc( p_sys->i_selected_dev, 
                                       (AudioDeviceIOProc)RenderCallbackSPDIF );
        if( err != noErr )
        {
            msg_Err( p_aout, "AudioDeviceRemoveIOProc failed: [%4.4s]", (char *)&err );
        }
        return VLC_FALSE;
    }

    return VLC_TRUE;
}


/*****************************************************************************
 * Close: Close HAL AudioUnit
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    aout_instance_t     *p_aout = (aout_instance_t *)p_this;
    struct aout_sys_t   *p_sys = p_aout->output.p_sys;
    OSStatus            err = noErr;
    UInt32              i_param_size = 0;
    
    if( p_sys->au_unit )
    {
        verify_noerr( AudioOutputUnitStop( p_sys->au_unit ) );
        verify_noerr( AudioUnitUninitialize( p_sys->au_unit ) );
        verify_noerr( CloseComponent( p_sys->au_unit ) );
    }
    
    if( p_sys->b_digital )
    {
        /* Stop device */
        err = AudioDeviceStop( p_sys->i_selected_dev, 
                               (AudioDeviceIOProc)RenderCallbackSPDIF ); 
        if( err != noErr )
        {
            msg_Err( p_aout, "AudioDeviceStop failed: [%4.4s]", (char *)&err );
        }

        /* Remove callback */
        err = AudioDeviceRemoveIOProc( p_sys->i_selected_dev,
                                       (AudioDeviceIOProc)RenderCallbackSPDIF );
        if( err != noErr )
        {
            msg_Err( p_aout, "AudioDeviceRemoveIOProc failed: [%4.4s]", (char *)&err );
        }
        
        if( p_sys->b_revert )
        {
            struct timeval now;
            struct timespec timeout;
            struct { vlc_mutex_t lock; vlc_cond_t cond; } w;

            /* Install the callback */
            err = AudioStreamAddPropertyListener( p_sys->i_stream_id, 0,
                                              kAudioStreamPropertyPhysicalFormat,
                                              StreamListener, (void *)&w );
            if( err != noErr )
            {
                msg_Err( p_aout, "AudioStreamAddPropertyListener failed: [%4.4s]", (char *)&err );
            }

            /* Condition because SetProperty is asynchronious */ 
            vlc_cond_init( p_aout, &w.cond );
            vlc_mutex_init( p_aout, &w.lock );
            vlc_mutex_lock( &w.lock );

            msg_Dbg( p_aout, STREAM_FORMAT_MSG( "setting stream format: ", p_sys->sfmt_revert ) );

            err = AudioStreamSetProperty( p_sys->i_stream_id, NULL, 0,
                                            kAudioStreamPropertyPhysicalFormat,
                                            sizeof( AudioStreamBasicDescription ),
                                            &p_sys->sfmt_revert );
                                            
            if( err != noErr )
            {
                msg_Err( p_aout, "Streamformat reverse failed: [%4.4s]", (char *)&err );
            }
            
            gettimeofday( &now, NULL );
            timeout.tv_sec = now.tv_sec;
            timeout.tv_nsec = (now.tv_usec + 900000) * 1000;

            pthread_cond_timedwait( &w.cond.cond, &w.lock.mutex, &timeout );
            vlc_mutex_unlock( &w.lock );

            err = AudioStreamRemovePropertyListener( p_sys->i_stream_id, 0,
                                                kAudioStreamPropertyPhysicalFormat,
                                                StreamListener );
            if( err != noErr )
            {
                msg_Err( p_aout, "AudioStreamRemovePropertyListener failed: [%4.4s]", (char *)&err );
            }

            vlc_mutex_destroy( &w.lock );
            vlc_cond_destroy( &w.cond );
            
            i_param_size = sizeof( AudioStreamBasicDescription );
            err = AudioStreamGetProperty( p_sys->i_stream_id, 0,
                                          kAudioStreamPropertyPhysicalFormat,
                                          &i_param_size, 
                                          &p_sys->stream_format );

            msg_Dbg( p_aout, STREAM_FORMAT_MSG( "actual format in use: ", p_sys->stream_format ) );
        }
        if( p_sys->b_changed_mixing && p_sys->sfmt_revert.mFormatID != kAudioFormat60958AC3 )
        {
            int b_mix;
            Boolean b_writeable;
            /* Revert mixable to true if we are allowed to */
            err = AudioDeviceGetPropertyInfo( p_sys->i_selected_dev, 0, FALSE, kAudioDevicePropertySupportsMixing,
                                        &i_param_size, &b_writeable );

            err = AudioDeviceGetProperty( p_sys->i_selected_dev, 0, FALSE, kAudioDevicePropertySupportsMixing,
                                        &i_param_size, &b_mix );
                                        
            if( !err && b_writeable )
            {
                msg_Dbg( p_aout, "mixable is: %d", b_mix );
                b_mix = 1;
                err = AudioDeviceSetProperty( p_sys->i_selected_dev, 0, 0, FALSE,
                                    kAudioDevicePropertySupportsMixing, i_param_size, &b_mix );
            }

            if( err != noErr )
            {
                msg_Err( p_aout, "failed to set mixmode: [%4.4s]", (char *)&err );
            }
        }
    }

    err = AudioHardwareRemovePropertyListener( kAudioHardwarePropertyDevices,
                                               HardwareListener );
                                               
    if( err != noErr )
    {
        msg_Err( p_aout, "AudioHardwareRemovePropertyListener failed: [%4.4s]", (char *)&err );
    }
    
    if( p_sys->i_hog_pid == getpid() )
    {
        p_sys->i_hog_pid = -1;
        i_param_size = sizeof( p_sys->i_hog_pid );
        err = AudioDeviceSetProperty( p_sys->i_selected_dev, 0, 0, FALSE,
                                         kAudioDevicePropertyHogMode, i_param_size, &p_sys->i_hog_pid );
        if( err != noErr ) msg_Err( p_aout, "Could not release hogmode: [%4.4s]", (char *)&err );
    }
    
    if( p_sys ) free( p_sys );
}

/*****************************************************************************
 * Play: nothing to do
 *****************************************************************************/
static void Play( aout_instance_t * p_aout )
{
}


/*****************************************************************************
 * Probe
 *****************************************************************************/
static void Probe( aout_instance_t * p_aout )
{
    OSStatus            err = noErr;
    UInt32              i = 0, i_param_size = 0;
    AudioDeviceID       devid_def = 0;
    AudioDeviceID       *p_devices = NULL;
    vlc_value_t         val, text;

    struct aout_sys_t   *p_sys = p_aout->output.p_sys;

    /* Get number of devices */
    err = AudioHardwareGetPropertyInfo( kAudioHardwarePropertyDevices,
                                        &i_param_size, NULL );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get number of devices: [%4.4s]", (char *)&err );
        goto error;
    }

    p_sys->i_devices = i_param_size / sizeof( AudioDeviceID );

    if( p_sys->i_devices < 1 )
    {
        msg_Err( p_aout, "no devices found" );
        goto error;
    }

    msg_Dbg( p_aout, "system has [%ld] device(s)", p_sys->i_devices );

    /* Allocate DeviceID array */
    p_devices = (AudioDeviceID*)malloc( sizeof(AudioDeviceID) * p_sys->i_devices );
    if( p_devices == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        goto error;
    }

    /* Populate DeviceID array */
    err = AudioHardwareGetProperty( kAudioHardwarePropertyDevices,
                                    &i_param_size, p_devices );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get the device ID's: [%4.4s]", (char *)&err );
        goto error;
    }

    /* Find the ID of the default Device */
    i_param_size = sizeof( AudioDeviceID );
    err = AudioHardwareGetProperty( kAudioHardwarePropertyDefaultOutputDevice,
                                    &i_param_size, &devid_def );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get default audio device: [%4.4s]", (char *)&err );
        goto error;
    }
    p_sys->i_default_dev = devid_def;
    
    var_Create( p_aout, "audio-device", VLC_VAR_INTEGER|VLC_VAR_HASCHOICE );
    text.psz_string = _("Audio Device");
    var_Change( p_aout, "audio-device", VLC_VAR_SETTEXT, &text, NULL );
    
    for( i = 0; i < p_sys->i_devices; i++ )
    {
        char *psz_name;
        i_param_size = 0;

        /* Retrieve the length of the device name */
        err = AudioDeviceGetPropertyInfo(
                    p_devices[i], 0, VLC_FALSE,
                    kAudioDevicePropertyDeviceName,
                    &i_param_size, NULL);
        if( err ) goto error;

        /* Retrieve the name of the device */
        psz_name = (char *)malloc( i_param_size );
        err = AudioDeviceGetProperty(
                    p_devices[i], 0, VLC_FALSE,
                    kAudioDevicePropertyDeviceName,
                    &i_param_size, psz_name);
        if( err ) goto error;

        msg_Dbg( p_aout, "DevID: %#lx  DevName: %s", p_devices[i], psz_name );

        if( !AudioDeviceHasOutput( p_devices[i]) )
        {
            msg_Dbg( p_aout, "this device is INPUT only. skipping..." );
            continue;
        }

        val.i_int = (int)p_devices[i];
        text.psz_string = strdup( psz_name );
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text );
        if( p_sys->i_default_dev == p_devices[i] )
        {
            var_Change( p_aout, "audio-device", VLC_VAR_SETDEFAULT, &val, NULL );
            var_Set( p_aout, "audio-device", val );
        }

        if( AudioDeviceSupportsDigital( p_aout, p_devices[i] ) )
        {
            val.i_int = (int)p_devices[i] | AOUT_VAR_SPDIF_FLAG;
            asprintf( &text.psz_string, "%s (Encoded Output)", psz_name );
            var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text );
            if( p_sys->i_default_dev == p_devices[i] && config_GetInt( p_aout, "spdif" ) )
            {
                var_Change( p_aout, "audio-device", VLC_VAR_SETDEFAULT, &val, NULL );
                var_Set( p_aout, "audio-device", val );
            }
        }
        
        free( psz_name);
    }
    
    var_Get( p_aout->p_vlc, "macosx-audio-device", &val );
    msg_Dbg( p_aout, "device value override1: %#x", val.i_int );
    if( val.i_int > 0 )
    {
        msg_Dbg( p_aout, "device value override2: %#x", val.i_int );
        var_Change( p_aout, "audio-device", VLC_VAR_SETDEFAULT, &val, NULL );
        var_Set( p_aout, "audio-device", val );
    }
    
    var_AddCallback( p_aout, "audio-device", AudioDeviceCallback, NULL );

    /* attach a Listener so that we are notified of a change in the Device setup */
    err = AudioHardwareAddPropertyListener( kAudioHardwarePropertyDevices,
                                            HardwareListener, 
                                            (void *)p_aout );
    if( err )
        goto error;

    msg_Dbg( p_aout, "succesful finish of deviceslist" );
    if( p_devices ) free( p_devices );
    return;

error:
    var_Destroy( p_aout, "audio-device" );
    if( p_devices ) free( p_devices );
    return;
}

/*****************************************************************************
 * AudioDeviceHasOutput: Checks if the Device actually provides any outputs at all
 *****************************************************************************/
static int AudioDeviceHasOutput( AudioDeviceID i_dev_id )
{
    UInt32			dataSize;
    Boolean			isWritable;
	
    verify_noerr( AudioDeviceGetPropertyInfo( i_dev_id, 0, FALSE, kAudioDevicePropertyStreams, &dataSize, &isWritable) );
    if (dataSize == 0) return FALSE;
    
    return TRUE;
}

/*****************************************************************************
 * AudioDeviceSupportsDigital: Check i_dev_id for digital stream support.
 *****************************************************************************/
static int AudioDeviceSupportsDigital( aout_instance_t *p_aout, AudioDeviceID i_dev_id )
{
    OSStatus                    err = noErr;
    UInt32                      i_param_size = 0;
    AudioStreamID               *p_streams = NULL;
    int                         i = 0, i_streams = 0;
    vlc_bool_t                  b_return = VLC_FALSE;
    
    /* Retrieve all the output streams */
    err = AudioDeviceGetPropertyInfo( i_dev_id, 0, FALSE,
                                      kAudioDevicePropertyStreams,
                                      &i_param_size, NULL );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get number of streams: [%4.4s]", (char *)&err );
        return VLC_FALSE;
    }
    
    i_streams = i_param_size / sizeof( AudioStreamID );
    p_streams = (AudioStreamID *)malloc( i_param_size );
    if( p_streams == NULL )
    {
        msg_Err( p_aout, "Out of memory" );
        return VLC_ENOMEM;
    }
    
    err = AudioDeviceGetProperty( i_dev_id, 0, FALSE,
                                    kAudioDevicePropertyStreams,
                                    &i_param_size, p_streams );
    
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get number of streams: [%4.4s]", (char *)&err );
        return VLC_FALSE;
    }

    for( i = 0; i < i_streams; i++ )
    {
        if( AudioStreamSupportsDigital( p_aout, p_streams[i] ) )
            b_return = VLC_TRUE;
    }
    
    if( p_streams ) free( p_streams );
    return b_return;
}

/*****************************************************************************
 * AudioStreamSupportsDigital: Check i_stream_id for digital stream support.
 *****************************************************************************/
static int AudioStreamSupportsDigital( aout_instance_t *p_aout, AudioStreamID i_stream_id )
{
    OSStatus                    err = noErr;
    UInt32                      i_param_size = 0;
    AudioStreamBasicDescription *p_format_list = NULL;
    int                         i = 0, i_formats = 0;
    vlc_bool_t                  b_return = VLC_FALSE;
    
    /* Retrieve all the stream formats supported by each output stream */
    err = AudioStreamGetPropertyInfo( i_stream_id, 0,
                                      kAudioStreamPropertyPhysicalFormats,
                                      &i_param_size, NULL );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get number of streamformats: [%4.4s]", (char *)&err );
        return VLC_FALSE;
    }
    
    i_formats = i_param_size / sizeof( AudioStreamBasicDescription );
    msg_Dbg( p_aout, "number of formats: %d", i_formats );
    p_format_list = (AudioStreamBasicDescription *)malloc( i_param_size );
    if( p_format_list == NULL )
    {
        msg_Err( p_aout, "Could not malloc the memory" );
        return VLC_FALSE;
    }
    
    err = AudioStreamGetProperty( i_stream_id, 0,
                                      kAudioStreamPropertyPhysicalFormats,
                                      &i_param_size, p_format_list );
    if( err != noErr )
    {
        msg_Err( p_aout, "could not get the list of streamformats: [%4.4s]", (char *)&err );
        free( p_format_list);
        p_format_list = NULL;
        return VLC_FALSE;
    }

    for( i = 0; i < i_formats; i++ )
    {
        msg_Dbg( p_aout, STREAM_FORMAT_MSG( "supported format: ", p_format_list[i] ) );
        
        if( p_format_list[i].mFormatID == 'IAC3' ||
                  p_format_list[i].mFormatID == kAudioFormat60958AC3 )
        {
            b_return = VLC_TRUE;
        }
    }
    
    if( p_format_list ) free( p_format_list );
    return b_return;
}

/*****************************************************************************
 * RenderCallbackAnalog: This function is called everytime the AudioUnit wants
 * us to provide some more audio data.
 * Don't print anything during normal playback, calling blocking function from
 * this callback is not allowed.
 *****************************************************************************/
static OSStatus RenderCallbackAnalog( vlc_object_t *_p_aout,
                                      AudioUnitRenderActionFlags *ioActionFlags,
                                      const AudioTimeStamp *inTimeStamp,
                                      unsigned int inBusNummer,
                                      unsigned int inNumberFrames,
                                      AudioBufferList *ioData )
{
    AudioTimeStamp  host_time;
    mtime_t         current_date = 0;
    uint32_t        i_mData_bytes = 0;    

    aout_instance_t * p_aout = (aout_instance_t *)_p_aout;
    struct aout_sys_t * p_sys = p_aout->output.p_sys;

    host_time.mFlags = kAudioTimeStampHostTimeValid;
    AudioDeviceTranslateTime( p_sys->i_selected_dev, inTimeStamp, &host_time );

    /* Check for the difference between the Device clock and mdate */
    p_sys->clock_diff = - (mtime_t)
        AudioConvertHostTimeToNanos( AudioGetCurrentHostTime() ) / 1000; 
    p_sys->clock_diff += mdate();

    current_date = p_sys->clock_diff +
                   AudioConvertHostTimeToNanos( host_time.mHostTime ) / 1000;
                   //- ((mtime_t) 1000000 / p_aout->output.output.i_rate * 31 ); // 31 = Latency in Frames. retrieve somewhere

    if( ioData == NULL && ioData->mNumberBuffers < 1 )
    {
        msg_Err( p_aout, "no iodata or buffers");
        return 0;
    }
    if( ioData->mNumberBuffers > 1 )
        msg_Err( p_aout, "well this is weird. seems like there is more than one buffer..." );


    if( p_sys->i_total_bytes > 0 )
    {
        i_mData_bytes = __MIN( p_sys->i_total_bytes - p_sys->i_read_bytes, ioData->mBuffers[0].mDataByteSize );
        p_aout->p_vlc->pf_memcpy( ioData->mBuffers[0].mData, &p_sys->p_remainder_buffer[p_sys->i_read_bytes], i_mData_bytes );
        p_sys->i_read_bytes += i_mData_bytes;
        current_date += (mtime_t) ( (mtime_t) 1000000 / p_aout->output.output.i_rate ) *
                        ( i_mData_bytes / 4 / aout_FormatNbChannels( &p_aout->output.output )  ); // 4 is fl32 specific
        
        if( p_sys->i_read_bytes >= p_sys->i_total_bytes )
            p_sys->i_read_bytes = p_sys->i_total_bytes = 0;
    }
    
    while( i_mData_bytes < ioData->mBuffers[0].mDataByteSize )
    {
        /* We don't have enough data yet */
        aout_buffer_t * p_buffer;
        p_buffer = aout_OutputNextBuffer( p_aout, current_date , VLC_FALSE );
        
        if( p_buffer != NULL )
        {
            uint32_t i_second_mData_bytes = __MIN( p_buffer->i_nb_bytes, ioData->mBuffers[0].mDataByteSize - i_mData_bytes );
            
            p_aout->p_vlc->pf_memcpy( (uint8_t *)ioData->mBuffers[0].mData + i_mData_bytes, p_buffer->p_buffer, i_second_mData_bytes );
            i_mData_bytes += i_second_mData_bytes;

            if( i_mData_bytes >= ioData->mBuffers[0].mDataByteSize )
            {
                p_sys->i_total_bytes = p_buffer->i_nb_bytes - i_second_mData_bytes;
                p_aout->p_vlc->pf_memcpy( p_sys->p_remainder_buffer, &p_buffer->p_buffer[i_second_mData_bytes], p_sys->i_total_bytes );
            }
            else
            {
                // update current_date
                current_date += (mtime_t) ( (mtime_t) 1000000 / p_aout->output.output.i_rate ) *
                                ( i_second_mData_bytes / 4 / aout_FormatNbChannels( &p_aout->output.output )  ); // 4 is fl32 specific
            }
            aout_BufferFree( p_buffer );
        }
        else
        {
             p_aout->p_vlc->pf_memset( (uint8_t *)ioData->mBuffers[0].mData +i_mData_bytes, 0, ioData->mBuffers[0].mDataByteSize - i_mData_bytes );
             i_mData_bytes += ioData->mBuffers[0].mDataByteSize - i_mData_bytes;
        }
    }
    return( noErr );     
}

/*****************************************************************************
 * RenderCallbackSPDIF: callback for SPDIF audio output
 *****************************************************************************/
static OSStatus RenderCallbackSPDIF( AudioDeviceID inDevice,
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

    /* Check for the difference between the Device clock and mdate */
    p_sys->clock_diff = - (mtime_t)
        AudioConvertHostTimeToNanos( inNow->mHostTime ) / 1000; 
    p_sys->clock_diff += mdate();

    current_date = p_sys->clock_diff +
                   AudioConvertHostTimeToNanos( inOutputTime->mHostTime ) / 1000;
                   //- ((mtime_t) 1000000 / p_aout->output.output.i_rate * 31 ); // 31 = Latency in Frames. retrieve somewhere

    p_buffer = aout_OutputNextBuffer( p_aout, current_date, VLC_TRUE );

#define BUFFER outOutputData->mBuffers[p_sys->i_stream_index]
    if( p_buffer != NULL )
    {
        if( (int)BUFFER.mDataByteSize != (int)p_buffer->i_nb_bytes)
            msg_Warn( p_aout, "bytesize: %d nb_bytes: %d", (int)BUFFER.mDataByteSize, (int)p_buffer->i_nb_bytes );
        
        /* move data into output data buffer */
        p_aout->p_vlc->pf_memcpy( BUFFER.mData,
                                  p_buffer->p_buffer, p_buffer->i_nb_bytes );
        aout_BufferFree( p_buffer );
    }
    else
    {
        p_aout->p_vlc->pf_memset( BUFFER.mData, 0, BUFFER.mDataByteSize );
    }
#undef BUFFER

    return( noErr );     
}

/*****************************************************************************
 * HardwareListener: Warns us of changes in the list of registered devices
 *****************************************************************************/
static OSStatus HardwareListener( AudioHardwarePropertyID inPropertyID,
                                  void * inClientData )
{
    OSStatus err = noErr;
    aout_instance_t     *p_aout = (aout_instance_t *)inClientData;

    switch( inPropertyID )
    {
        case kAudioHardwarePropertyDevices:
        {
            /* something changed in the list of devices */
            /* We trigger the audio-device's aout_ChannelsRestart callback */
            var_Change( p_aout, "audio-device", VLC_VAR_TRIGGER_CALLBACKS, NULL, NULL );
            var_Destroy( p_aout, "audio-device" );
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
 * AudioDeviceCallback: Callback triggered when the audio-device variable is changed
 *****************************************************************************/
static int AudioDeviceCallback( vlc_object_t *p_this, const char *psz_variable,
                     vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    var_Set( p_aout->p_vlc, "macosx-audio-device", new_val );
    msg_Dbg( p_aout, "Set Device: %#x", new_val.i_int );
    return aout_ChannelsRestart( p_this, psz_variable, old_val, new_val, param );
}

