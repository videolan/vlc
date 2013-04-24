/*****************************************************************************
 * directx.c: Windows DirectX audio output method
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_charset.h>

#include "windows_audio_common.h"

#define DS_BUF_SIZE (1024*1024)

/*****************************************************************************
 * aout_sys_t: directx audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the direct sound specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    HINSTANCE           hdsound_dll;  /* handle of the opened dsound dll */

    LPDIRECTSOUND       p_dsobject;   /* main Direct Sound object */
    LPDIRECTSOUNDBUFFER p_dsbuffer;   /* the sound buffer we use (direct sound
                                       * takes care of mixing all the
                                       * secondary buffers into the primary) */

    LPDIRECTSOUNDNOTIFY p_notify;
    HANDLE hnotify_evt;
    struct
    {
        float            volume;
        LONG             mb;
        bool             mute;
    } volume;

    int      i_bytes_per_sample;      /* Size in bytes of one frame */     
    int      i_rate;                  /* Sample rate */ 

    int      i_speaker_setup;         /* Speaker setup override */

    uint8_t  chans_to_reorder;        /* do we need channel reordering */
    uint8_t  chan_table[AOUT_CHAN_MAX];
    uint32_t i_channel_mask;
    vlc_fourcc_t format;

    size_t  i_write;
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );
static void Stop( audio_output_t * );
static void Play( audio_output_t *, block_t * );
static int  VolumeSet( audio_output_t *, float );
static int  MuteSet( audio_output_t *, bool );
static void Flush( audio_output_t *, bool );
static void Pause( audio_output_t *, bool, mtime_t );
static int  TimeGet( audio_output_t *, mtime_t *);

/* local functions */
static void Probe( audio_output_t *, const audio_sample_format_t * );
static int  InitDirectSound   ( audio_output_t * );
static int  CreateDSBuffer    ( audio_output_t *, int, int, int, int, bool );
static int  CreateDSBufferPCM ( audio_output_t *, vlc_fourcc_t*, int, int, bool );
static void DestroyDSBuffer   ( audio_output_t * );
static int  FillBuffer        ( audio_output_t *, block_t * );

static int ReloadDirectXDevices( vlc_object_t *, const char *,
                                 char ***, char *** );

/* Speaker setup override options list */
static const char *const speaker_list[] = { "Windows default", "Mono", "Stereo",
                                            "Quad", "5.1", "7.1" };

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DEVICE_TEXT N_("Output device")
#define DEVICE_LONGTEXT N_("Select your audio output device")

#define SPEAKER_TEXT N_("Speaker configuration")
#define SPEAKER_LONGTEXT N_("Select speaker configuration you want to use. " \
    "This option doesn't upmix! So NO e.g. Stereo -> 5.1 conversion." )

#define VOLUME_TEXT N_("Audio volume")
#define VOLUME_LONGTEXT N_("Audio volume in hundredths of decibels (dB).")

vlc_module_begin ()
    set_description( N_("DirectX audio output") )
    set_shortname( "DirectX" )
    set_capability( "audio output", 100 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_shortcut( "directx", "aout_directx" )

    add_string( "directx-audio-device", NULL,
             DEVICE_TEXT, DEVICE_LONGTEXT, false )
        change_string_cb( ReloadDirectXDevices )
    add_obsolete_string( "directx-audio-device-name")
    add_bool( "directx-audio-float32", false, FLOAT_TEXT,
              FLOAT_LONGTEXT, true )
    add_string( "directx-audio-speaker", "Windows default",
                 SPEAKER_TEXT, SPEAKER_LONGTEXT, true )
        change_string_list( speaker_list, speaker_list )
    add_float( "directx-volume", 1.0f,
                 VOLUME_TEXT, VOLUME_LONGTEXT, true )
        change_integer_range( DSBVOLUME_MIN, DSBVOLUME_MAX )

    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * OpenAudio: open the audio device
 *****************************************************************************
 * This function opens and setups Direct Sound.
 *****************************************************************************/
static int Start( audio_output_t *p_aout, audio_sample_format_t *restrict fmt )
{
    vlc_value_t val;
    char * psz_speaker;
    int i = 0;

    const char * const * ppsz_compare = speaker_list;

    msg_Dbg( p_aout, "Opening DirectSound Audio Output" );

    /* Retrieve config values */
    var_Create( p_aout, "directx-audio-float32",
                VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    psz_speaker = var_CreateGetString( p_aout, "directx-audio-speaker" );

    while ( *ppsz_compare != NULL )
    {
        if ( !strncmp( *ppsz_compare, psz_speaker, strlen(*ppsz_compare) ) )
        {
            break;
        }
        ppsz_compare++; i++;
    }

    if ( *ppsz_compare == NULL )
    {
        msg_Err( p_aout, "(%s) isn't valid speaker setup option", psz_speaker );
        msg_Err( p_aout, "Defaulting to Windows default speaker config");
        i = 0;
    }
    free( psz_speaker );
    p_aout->sys->i_speaker_setup = i;

    /* Initialise DirectSound */
    if( InitDirectSound( p_aout ) )
    {
        msg_Err( p_aout, "cannot initialize DirectSound" );
        goto error;
    }

    if( var_Type( p_aout, "audio-device" ) == 0 )
    {
        Probe( p_aout, fmt );
    }

    if( var_Get( p_aout, "audio-device", &val ) < 0 )
    {
        msg_Err( p_aout, "DirectSound Probe failed()" );
        goto error;
    }

    /* Open the device */
    if( val.i_int == AOUT_VAR_SPDIF )
    {
        fmt->i_format = VLC_CODEC_SPDIFL;

        /* Calculate the frame size in bytes */
        fmt->i_bytes_per_frame = AOUT_SPDIF_SIZE;
        fmt->i_frame_length = A52_FRAME_NB;

        if( CreateDSBuffer( p_aout, VLC_CODEC_SPDIFL,
                            fmt->i_physical_channels,
                            aout_FormatNbChannels( fmt ), fmt->i_rate, false )
            != VLC_SUCCESS )
        {
            msg_Err( p_aout, "cannot open directx audio device" );
            goto error;
        }
    }
    else
    {
        if( val.i_int == AOUT_VAR_5_1 )
            fmt->i_physical_channels = AOUT_CHANS_5_0;
        else if( val.i_int == AOUT_VAR_7_1 )
            fmt->i_physical_channels = AOUT_CHANS_7_1;
        else if( val.i_int == AOUT_VAR_3F2R )
            fmt->i_physical_channels = AOUT_CHANS_5_0;
        else if( val.i_int == AOUT_VAR_2F2R )
            fmt->i_physical_channels = AOUT_CHANS_4_0;
        else if( val.i_int == AOUT_VAR_MONO )
            fmt->i_physical_channels = AOUT_CHAN_CENTER;
        else
            fmt->i_physical_channels = AOUT_CHANS_2_0;

        aout_FormatPrepare( fmt );

        if( CreateDSBufferPCM( p_aout, &fmt->i_format,
                               fmt->i_physical_channels, fmt->i_rate, false )
            != VLC_SUCCESS )
        {
            msg_Err( p_aout, "cannot open directx audio device" );
            goto error;
        }
    }
    p_aout->sys->i_write = 0;

    /* Force volume update */
    VolumeSet( p_aout, p_aout->sys->volume.volume );
    MuteSet( p_aout, p_aout->sys->volume.mute );

    /* then launch the notification thread */
    p_aout->time_get = TimeGet;
    p_aout->play = Play;
    p_aout->pause = Pause;
    p_aout->flush = Flush;

    return VLC_SUCCESS;

 error:
    Stop( p_aout );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Probe: probe the audio device for available formats and channels
 *****************************************************************************/
static void Probe( audio_output_t * p_aout, const audio_sample_format_t *fmt )
{
    vlc_value_t val, text;
    vlc_fourcc_t i_format;
    DWORD ui_speaker_config;
    bool is_default_output_set = false;

    var_Create( p_aout, "audio-device", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Audio Device");
    var_Change( p_aout, "audio-device", VLC_VAR_SETTEXT, &text, NULL );

    /* Test for 5.1 support */
    if( fmt->i_physical_channels == AOUT_CHANS_5_1 )
    {
        if( CreateDSBufferPCM( p_aout, &i_format, AOUT_CHANS_5_1,
                               fmt->i_rate, true ) == VLC_SUCCESS )
        {
            val.i_int = AOUT_VAR_5_1;
            text.psz_string = (char*) "5.1";
            var_Change( p_aout, "audio-device",
                        VLC_VAR_ADDCHOICE, &val, &text );
            var_Change( p_aout, "audio-device", VLC_VAR_SETDEFAULT, &val, NULL );
            is_default_output_set = true;
            msg_Dbg( p_aout, "device supports 5.1 channels" );
        }
    }

    /* Test for 7.1 support */
    if( fmt->i_physical_channels == AOUT_CHANS_7_1 )
    {
        if( CreateDSBufferPCM( p_aout, &i_format, AOUT_CHANS_7_1,
                               fmt->i_rate, true ) == VLC_SUCCESS )
        {
            val.i_int = AOUT_VAR_7_1;
            text.psz_string = (char*) "7.1";
            var_Change( p_aout, "audio-device",
                        VLC_VAR_ADDCHOICE, &val, &text );
            var_Change( p_aout, "audio-device", VLC_VAR_SETDEFAULT, &val, NULL );
            is_default_output_set = true;
            msg_Dbg( p_aout, "device supports 7.1 channels" );
        }
    }

    /* Test for 3 Front 2 Rear support */
    if( fmt->i_physical_channels == AOUT_CHANS_5_0 )
    {
        if( CreateDSBufferPCM( p_aout, &i_format, AOUT_CHANS_5_0,
                               fmt->i_rate, true ) == VLC_SUCCESS )
        {
            val.i_int = AOUT_VAR_3F2R;
            text.psz_string = _("3 Front 2 Rear");
            var_Change( p_aout, "audio-device",
                        VLC_VAR_ADDCHOICE, &val, &text );
            if(!is_default_output_set)
            {
                var_Change( p_aout, "audio-device", VLC_VAR_SETDEFAULT, &val, NULL );
                is_default_output_set = true;
            }
            msg_Dbg( p_aout, "device supports 5 channels" );
        }
    }

    /* Test for 2 Front 2 Rear support */
    if( ( fmt->i_physical_channels & AOUT_CHANS_4_0 ) == AOUT_CHANS_4_0 )
    {
        if( CreateDSBufferPCM( p_aout, &i_format, AOUT_CHANS_4_0,
                               fmt->i_rate, true ) == VLC_SUCCESS )
        {
            val.i_int = AOUT_VAR_2F2R;
            text.psz_string = _("2 Front 2 Rear");
            var_Change( p_aout, "audio-device",
                        VLC_VAR_ADDCHOICE, &val, &text );
            if(!is_default_output_set)
            {
                var_Change( p_aout, "audio-device", VLC_VAR_SETDEFAULT, &val, NULL );
                is_default_output_set = true;
            }
            msg_Dbg( p_aout, "device supports 4 channels" );
        }
    }

    /* Test for stereo support */
    if( CreateDSBufferPCM( p_aout, &i_format, AOUT_CHANS_2_0,
                           fmt->i_rate, true ) == VLC_SUCCESS )
    {
        val.i_int = AOUT_VAR_STEREO;
        text.psz_string = _("Stereo");
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text );
        if(!is_default_output_set)
        {
            var_Change( p_aout, "audio-device", VLC_VAR_SETDEFAULT, &val, NULL );
            is_default_output_set = true;
            msg_Dbg( p_aout, "device supports 2 channels (DEFAULT!)" );
        }
        else msg_Dbg( p_aout, "device supports 2 channels" );
    }

    /* Test for mono support */
    if( CreateDSBufferPCM( p_aout, &i_format, AOUT_CHAN_CENTER,
                           fmt->i_rate, true ) == VLC_SUCCESS )
    {
        val.i_int = AOUT_VAR_MONO;
        text.psz_string = _("Mono");
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text );
        msg_Dbg( p_aout, "device supports 1 channel" );
    }

    /* Check the speaker configuration to determine which channel config should
     * be the default */
    if FAILED( IDirectSound_GetSpeakerConfig( p_aout->sys->p_dsobject,
                                              &ui_speaker_config ) )
    {
        ui_speaker_config = DSSPEAKER_STEREO;
        msg_Dbg( p_aout, "GetSpeakerConfig failed" );
    }
    switch( DSSPEAKER_CONFIG(ui_speaker_config) )
    {
    case DSSPEAKER_7POINT1:
    case DSSPEAKER_7POINT1_SURROUND:
        msg_Dbg( p_aout, "Windows says your SpeakerConfig is 7.1" );
        val.i_int = AOUT_VAR_7_1;
        break;
    case DSSPEAKER_5POINT1:
    case DSSPEAKER_5POINT1_SURROUND:
        msg_Dbg( p_aout, "Windows says your SpeakerConfig is 5.1" );
        val.i_int = AOUT_VAR_5_1;
        break;
    case DSSPEAKER_QUAD:
        msg_Dbg( p_aout, "Windows says your SpeakerConfig is Quad" );
        val.i_int = AOUT_VAR_2F2R;
        break;
#if 0 /* Lots of people just get their settings wrong and complain that
       * this is a problem with VLC so just don't ever set mono by default. */
    case DSSPEAKER_MONO:
        val.i_int = AOUT_VAR_MONO;
        break;
#endif
    case DSSPEAKER_SURROUND:
        msg_Dbg( p_aout, "Windows says your SpeakerConfig is surround" );
    case DSSPEAKER_STEREO:
        msg_Dbg( p_aout, "Windows says your SpeakerConfig is stereo" );
    default:
        /* If nothing else is found, choose stereo output */
        val.i_int = AOUT_VAR_STEREO;
        break;
    }

    /* Check if we want to override speaker config */
    switch( p_aout->sys->i_speaker_setup )
    {
    case 0: /* Default value aka Windows default speaker setup */
        break;
    case 1: /* Mono */
        msg_Dbg( p_aout, "SpeakerConfig is forced to Mono" );
        val.i_int = AOUT_VAR_MONO;
        break;
    case 2: /* Stereo */
        msg_Dbg( p_aout, "SpeakerConfig is forced to Stereo" );
        val.i_int = AOUT_VAR_STEREO;
        break;
    case 3: /* Quad */
        msg_Dbg( p_aout, "SpeakerConfig is forced to Quad" );
        val.i_int = AOUT_VAR_2F2R;
        break;
    case 4: /* 5.1 */
        msg_Dbg( p_aout, "SpeakerConfig is forced to 5.1" );
        val.i_int = AOUT_VAR_5_1;
        break;
    case 5: /* 7.1 */
        msg_Dbg( p_aout, "SpeakerConfig is forced to 7.1" );
        val.i_int = AOUT_VAR_7_1;
        break;
    default:
        msg_Dbg( p_aout, "SpeakerConfig is forced to non-existing value" );
        break;
    }

    var_Set( p_aout, "audio-device", val );

    /* Test for SPDIF support */
    if ( AOUT_FMT_SPDIF( fmt ) )
    {
        if( CreateDSBuffer( p_aout, VLC_CODEC_SPDIFL,
                            fmt->i_physical_channels,
                            aout_FormatNbChannels( fmt ), fmt->i_rate, true )
            == VLC_SUCCESS )
        {
            msg_Dbg( p_aout, "device supports A/52 over S/PDIF" );
            val.i_int = AOUT_VAR_SPDIF;
            text.psz_string = _("A/52 over S/PDIF");
            var_Change( p_aout, "audio-device",
                        VLC_VAR_ADDCHOICE, &val, &text );
            if( var_InheritBool( p_aout, "spdif" ) )
                var_Set( p_aout, "audio-device", val );
        }
    }

    var_Change( p_aout, "audio-device", VLC_VAR_CHOICESCOUNT, &val, NULL );
    if( val.i_int <= 0 )
    {
        /* Probe() has failed. */
        var_Destroy( p_aout, "audio-device" );
        return;
    }

    var_AddCallback( p_aout, "audio-device", aout_ChannelsRestart, NULL );
}

/*****************************************************************************
 * Play: we'll start playing the directsound buffer here because at least here
 *       we know the first buffer has been put in the aout fifo and we also
 *       know its date.
 *****************************************************************************/
static void Play( audio_output_t *p_aout, block_t *p_buffer )
{
    if(  FillBuffer( p_aout, p_buffer ) ==  VLC_SUCCESS )
    {
        /* start playing the buffer */
        HRESULT dsresult = IDirectSoundBuffer_Play( p_aout->sys->p_dsbuffer,
                                                    0, 0, DSBPLAY_LOOPING );
        if( dsresult == DSERR_BUFFERLOST )
        {
            IDirectSoundBuffer_Restore( p_aout->sys->p_dsbuffer );
            dsresult = IDirectSoundBuffer_Play( p_aout->sys->p_dsbuffer,
                                                0, 0, DSBPLAY_LOOPING );
        }
        if( dsresult != DS_OK )
            msg_Err( p_aout, "cannot start playing buffer" );
    }
}

static int VolumeSet( audio_output_t *p_aout, float volume )
{
    aout_sys_t *sys = p_aout->sys;
    int ret = 0;

    /* Convert UI volume to linear factor (cube) */
    float vol = volume * volume * volume;

    /* millibels from linear amplification */
    LONG mb = lroundf(2000.f * log10f(vol));

    /* Clamp to allowed DirectSound range */
    static_assert( DSBVOLUME_MIN < DSBVOLUME_MAX, "DSBVOLUME_* confused" );
    if( mb > DSBVOLUME_MAX )
    {
        mb = DSBVOLUME_MAX;
        ret = -1;
    }
    if( mb <= DSBVOLUME_MIN )
        mb = DSBVOLUME_MIN;

    sys->volume.mb = mb;
    sys->volume.volume = volume;
    if( !sys->volume.mute && sys->p_dsbuffer &&
        IDirectSoundBuffer_SetVolume( sys->p_dsbuffer, mb ) != DS_OK )
        return -1;
    /* Convert back to UI volume */
    aout_VolumeReport( p_aout, volume );

    if( var_InheritBool( p_aout, "volume-save" ) )
        config_PutFloat( p_aout, "directx-volume", volume );
    return ret;
}

static int MuteSet( audio_output_t *p_aout, bool mute )
{
    HRESULT res = DS_OK;
    aout_sys_t *sys = p_aout->sys;

    sys->volume.mute = mute;

    if( sys->p_dsbuffer )
        res = IDirectSoundBuffer_SetVolume( sys->p_dsbuffer,
                                            mute? DSBVOLUME_MIN : sys->volume.mb );

    aout_MuteReport( p_aout, mute );
    return (res != DS_OK);
}

/*****************************************************************************
 * CloseAudio: close the audio device
 *****************************************************************************/
static void Stop( audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;
    msg_Dbg( p_aout, "closing audio device" );

    if( p_sys->p_notify )
        IDirectSoundNotify_Release(p_sys->p_notify );
    p_sys->p_notify = NULL;

    IDirectSoundBuffer_Stop( p_sys->p_dsbuffer );
    /* release the secondary buffer */
    DestroyDSBuffer( p_aout );

    /* finally release the DirectSound object */
    if( p_sys->p_dsobject )
        IDirectSound_Release( p_sys->p_dsobject );
}

/*****************************************************************************
 * InitDirectSound: handle all the gory details of DirectSound initialisation
 *****************************************************************************/
static int InitDirectSound( audio_output_t *p_aout )
{
    aout_sys_t *sys = p_aout->sys;
    GUID guid, *p_guid = NULL;
    HRESULT (WINAPI *OurDirectSoundCreate)(LPGUID, LPDIRECTSOUND *, LPUNKNOWN);

    OurDirectSoundCreate = (void *)
        GetProcAddress( p_aout->sys->hdsound_dll,
                        "DirectSoundCreate" );
    if( OurDirectSoundCreate == NULL )
    {
        msg_Warn( p_aout, "GetProcAddress FAILED" );
        goto error;
    }

    char *dev = var_GetNonEmptyString( p_aout, "directx-audio-device" );
    if( dev != NULL )
    {
        LPOLESTR lpsz = ToWide( dev );
        free( dev );

        if( SUCCEEDED( IIDFromString( lpsz, &guid ) ) )
            p_guid = &guid;
        else
            msg_Err( p_aout, "bad device GUID: %ls", lpsz );
        free( lpsz );
    }

    /* Create the direct sound object */
    if FAILED( OurDirectSoundCreate( p_guid, &sys->p_dsobject, NULL ) )
    {
        msg_Warn( p_aout, "cannot create a direct sound device" );
        goto error;
    }

    /* Set DirectSound Cooperative level, ie what control we want over Windows
     * sound device. In our case, DSSCL_EXCLUSIVE means that we can modify the
     * settings of the primary buffer, but also that only the sound of our
     * application will be hearable when it will have the focus.
     * !!! (this is not really working as intended yet because to set the
     * cooperative level you need the window handle of your application, and
     * I don't know of any easy way to get it. Especially since we might play
     * sound without any video, and so what window handle should we use ???
     * The hack for now is to use the Desktop window handle - it seems to be
     * working */
#if !VLC_WINSTORE_APP
    if( IDirectSound_SetCooperativeLevel( p_aout->sys->p_dsobject,
                                          GetDesktopWindow(),
                                          DSSCL_EXCLUSIVE) )
    {
        msg_Warn( p_aout, "cannot set direct sound cooperative level" );
    }
#endif
    return VLC_SUCCESS;

 error:
    sys->p_dsobject = NULL;
    return VLC_EGENERIC;

}

/*****************************************************************************
 * CreateDSBuffer: Creates a direct sound buffer of the required format.
 *****************************************************************************
 * This function creates the buffer we'll use to play audio.
 * In DirectSound there are two kinds of buffers:
 * - the primary buffer: which is the actual buffer that the soundcard plays
 * - the secondary buffer(s): these buffers are the one actually used by
 *    applications and DirectSound takes care of mixing them into the primary.
 *
 * Once you create a secondary buffer, you cannot change its format anymore so
 * you have to release the current one and create another.
 *****************************************************************************/
static int CreateDSBuffer( audio_output_t *p_aout, int i_format,
                           int i_channels, int i_nb_channels, int i_rate,
                           bool b_probe )
{
    WAVEFORMATEXTENSIBLE waveformat;
    DSBUFFERDESC         dsbdesc;

    /* First set the sound buffer format */
    waveformat.dwChannelMask = 0;
    for( unsigned i = 0; pi_vlc_chan_order_wg4[i]; i++ )
        if( i_channels & pi_vlc_chan_order_wg4[i] )
            waveformat.dwChannelMask |= pi_channels_in[i];

    switch( i_format )
    {
    case VLC_CODEC_SPDIFL:
        i_nb_channels = 2;
        /* To prevent channel re-ordering */
        waveformat.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
        waveformat.Format.wBitsPerSample = 16;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_DOLBY_AC3_SPDIF;
        waveformat.SubFormat = _KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF;
        break;

    case VLC_CODEC_FL32:
        waveformat.Format.wBitsPerSample = sizeof(float) * 8;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        waveformat.SubFormat = _KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        break;

    case VLC_CODEC_S16N:
        waveformat.Format.wBitsPerSample = 16;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_PCM;
        waveformat.SubFormat = _KSDATAFORMAT_SUBTYPE_PCM;
        break;
    }

    waveformat.Format.nChannels = i_nb_channels;
    waveformat.Format.nSamplesPerSec = i_rate;
    waveformat.Format.nBlockAlign =
        waveformat.Format.wBitsPerSample / 8 * i_nb_channels;
    waveformat.Format.nAvgBytesPerSec =
        waveformat.Format.nSamplesPerSec * waveformat.Format.nBlockAlign;

    p_aout->sys->i_bytes_per_sample = waveformat.Format.nBlockAlign;
    p_aout->sys->format = i_format;

    /* Then fill in the direct sound descriptor */
    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 /* Better position accuracy */
                    | DSBCAPS_GLOBALFOCUS         /* Allows background playing */
                    | DSBCAPS_CTRLVOLUME          /* Allows volume control */
                    | DSBCAPS_CTRLPOSITIONNOTIFY; /* Allow position notifications */

    /* Only use the new WAVE_FORMAT_EXTENSIBLE format for multichannel audio */
    if( i_nb_channels <= 2 )
    {
        waveformat.Format.cbSize = 0;
    }
    else
    {
        waveformat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        waveformat.Format.cbSize =
            sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

        /* Needed for 5.1 on emu101k */
        dsbdesc.dwFlags |= DSBCAPS_LOCHARDWARE;
    }

    dsbdesc.dwBufferBytes = DS_BUF_SIZE; /* buffer size */
    dsbdesc.lpwfxFormat = (WAVEFORMATEX *)&waveformat;

    if FAILED( IDirectSound_CreateSoundBuffer(
                   p_aout->sys->p_dsobject, &dsbdesc,
                   &p_aout->sys->p_dsbuffer, NULL) )
    {
        if( dsbdesc.dwFlags & DSBCAPS_LOCHARDWARE )
        {
            /* Try without DSBCAPS_LOCHARDWARE */
            dsbdesc.dwFlags &= ~DSBCAPS_LOCHARDWARE;
            if FAILED( IDirectSound_CreateSoundBuffer(
                   p_aout->sys->p_dsobject, &dsbdesc,
                   &p_aout->sys->p_dsbuffer, NULL) )
            {
                return VLC_EGENERIC;
            }
            if( !b_probe )
                msg_Dbg( p_aout, "couldn't use hardware sound buffer" );
        }
        else
        {
            return VLC_EGENERIC;
        }
    }

    /* Stop here if we were just probing */
    if( b_probe )
    {
        IDirectSoundBuffer_Release( p_aout->sys->p_dsbuffer );
        p_aout->sys->p_dsbuffer = NULL;
        return VLC_SUCCESS;
    }

    p_aout->sys->i_rate = i_rate;
    p_aout->sys->i_channel_mask = waveformat.dwChannelMask;
    p_aout->sys->chans_to_reorder =
        aout_CheckChannelReorder( pi_channels_in, pi_channels_out,
                                  waveformat.dwChannelMask,
                                  p_aout->sys->chan_table );
    if( p_aout->sys->chans_to_reorder )
    {
        msg_Dbg( p_aout, "channel reordering needed" );
    }

    if( IDirectSoundBuffer_QueryInterface( p_aout->sys->p_dsbuffer,
                                           &IID_IDirectSoundNotify,
                                           (void **) &p_aout->sys->p_notify )
            != DS_OK )
    {
        
        msg_Err(p_aout, "Couldn't query IDirectSoundNotify");
        p_aout->sys->p_notify = NULL;
    }

    FillBuffer(p_aout,NULL);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CreateDSBufferPCM: creates a PCM direct sound buffer.
 *****************************************************************************
 * We first try to create a WAVE_FORMAT_IEEE_FLOAT buffer if supported by
 * the hardware, otherwise we create a WAVE_FORMAT_PCM buffer.
 ****************************************************************************/
static int CreateDSBufferPCM( audio_output_t *p_aout, vlc_fourcc_t *i_format,
                              int i_channels, int i_rate, bool b_probe )
{
    unsigned i_nb_channels = popcount( i_channels );

    /* Float32 audio samples are not supported for 5.1 output on the emu101k */
    if( !var_GetBool( p_aout, "directx-audio-float32" ) ||
        i_nb_channels > 2 ||
        CreateDSBuffer( p_aout, VLC_CODEC_FL32,
                        i_channels, i_nb_channels, i_rate, b_probe )
        != VLC_SUCCESS )
    {
        if ( CreateDSBuffer( p_aout, VLC_CODEC_S16N,
                             i_channels, i_nb_channels, i_rate, b_probe )
             != VLC_SUCCESS )
        {
            return VLC_EGENERIC;
        }
        else
        {
            *i_format = VLC_CODEC_S16N;
            return VLC_SUCCESS;
        }
    }
    else
    {
        *i_format = VLC_CODEC_FL32;
        return VLC_SUCCESS;
    }
}

/*****************************************************************************
 * DestroyDSBuffer
 *****************************************************************************
 * This function destroys the secondary buffer.
 *****************************************************************************/
static void DestroyDSBuffer( audio_output_t *p_aout )
{
    if( p_aout->sys->p_dsbuffer )
    {
        IDirectSoundBuffer_Release( p_aout->sys->p_dsbuffer );
        p_aout->sys->p_dsbuffer = NULL;
    }
}

/*****************************************************************************
 * FillBuffer: Fill in one of the direct sound frame buffers.
 *****************************************************************************
 * Returns VLC_SUCCESS on success.
 *****************************************************************************/
static int FillBuffer( audio_output_t *p_aout, block_t *p_buffer )
{
    aout_sys_t *p_sys = p_aout->sys;

    size_t towrite = (p_buffer)?p_buffer->i_buffer:DS_BUF_SIZE;
    void *p_write_position, *p_wrap_around;
    unsigned long l_bytes1, l_bytes2;
    HRESULT dsresult;

    /* Before copying anything, we have to lock the buffer */
    dsresult = IDirectSoundBuffer_Lock(
                p_sys->p_dsbuffer,         /* DS buffer */
                p_aout->sys->i_write,      /* Start offset */
                towrite,                   /* Number of bytes */
                &p_write_position,         /* Address of lock start */
                &l_bytes1,                 /* Count of bytes locked before wrap around */
                &p_wrap_around,            /* Buffer address (if wrap around) */
                &l_bytes2,                 /* Count of bytes after wrap around */
                0 );                       /* Flags: DSBLOCK_FROMWRITECURSOR is buggy */
    if( dsresult == DSERR_BUFFERLOST )
    {
        IDirectSoundBuffer_Restore( p_sys->p_dsbuffer );
        dsresult = IDirectSoundBuffer_Lock(
                               p_sys->p_dsbuffer,
                               p_aout->sys->i_write,
                               towrite,
                               &p_write_position,
                               &l_bytes1,
                               &p_wrap_around,
                               &l_bytes2,
                               0 );
    }
    if( dsresult != DS_OK )
    {
        msg_Warn( p_aout, "cannot lock buffer" );
        if( p_buffer ) block_Release( p_buffer );
        return VLC_EGENERIC;
    }

    if( p_buffer == NULL )
    {
        memset( p_write_position, 0, l_bytes1 );
    }
    else
    {
        if( p_sys->chans_to_reorder )
            /* Do the channel reordering here */
            aout_ChannelReorder( p_buffer->p_buffer, p_buffer->i_buffer,
                                 p_sys->chans_to_reorder, p_sys->chan_table,
                                 p_sys->format );

        memcpy( p_write_position, p_buffer->p_buffer, l_bytes1 );
        if( l_bytes1 < p_buffer->i_buffer)
            memcpy(p_wrap_around, p_buffer->p_buffer + l_bytes1, l_bytes2);
        block_Release( p_buffer );
    }

    /* Now the data has been copied, unlock the buffer */
    IDirectSoundBuffer_Unlock( p_sys->p_dsbuffer, p_write_position, l_bytes1,
                               p_wrap_around, l_bytes2 );

    p_sys->i_write += towrite;
    p_sys->i_write %= DS_BUF_SIZE;

    return VLC_SUCCESS;
}

typedef struct
{
    unsigned count;
    char **ids;
    char **names;
} ds_list_t;

static int CALLBACK DeviceEnumCallback( LPGUID guid, LPCWSTR desc,
                                        LPCWSTR mod, LPVOID data )
{
    ds_list_t *list = data;
    OLECHAR buf[48];

    StringFromGUID2( guid, buf, 48 );

    list->count++;
    list->ids = xrealloc( list->ids, list->count * sizeof(char *) );
    list->names = xrealloc( list->names, list->count * sizeof(char *) );
    list->ids[list->count - 1] = FromWide( buf );
    list->names[list->count - 1] = FromWide( desc );
    if( list->ids == NULL || list->names == NULL )
        abort();

    (void) mod;
    return true;
}

/*****************************************************************************
 * ReloadDirectXDevices: store the list of devices in preferences
 *****************************************************************************/
static int ReloadDirectXDevices( vlc_object_t *p_this, char const *psz_name,
                                 char ***values, char ***descs )
{
    ds_list_t list = { 0, NULL, NULL };

    (void) psz_name;

    HANDLE hdsound_dll = LoadLibrary(_T("DSOUND.DLL"));
    if( hdsound_dll == NULL )
    {
        msg_Warn( p_this, "cannot open DSOUND.DLL" );
        goto out;
    }

    /* Get DirectSoundEnumerate */
    HRESULT (WINAPI *OurDirectSoundEnumerate)(LPDSENUMCALLBACKW, LPVOID) =
            (void *)GetProcAddress( hdsound_dll, "DirectSoundEnumerateW" );
    if( OurDirectSoundEnumerate != NULL )
    {
        OurDirectSoundEnumerate( DeviceEnumCallback, &list );
        msg_Dbg( p_this, "found %u devices", list.count );
    }
    FreeLibrary(hdsound_dll);

out:
    *values = list.ids;
    *descs = list.names;
    return list.count;
}

static int DeviceSelect (audio_output_t *aout, const char *id)
{
    var_SetString(aout, "directx-audio-device", (id != NULL) ? id : "");
    aout_DeviceReport (aout, id);
    aout_RestartRequest (aout, AOUT_RESTART_OUTPUT);
    return 0;
}

static int Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->hdsound_dll = LoadLibrary(_T("DSOUND.DLL"));
    if (sys->hdsound_dll == NULL)
    {
        msg_Warn(aout, "cannot open DSOUND.DLL");
        free(sys);
        return VLC_EGENERIC;
    }

    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;
    aout->volume_set = VolumeSet;
    aout->mute_set = MuteSet;
    aout->device_select = DeviceSelect;

    /* Volume */
    sys->volume.volume = var_InheritFloat(aout, "directx-volume");
    aout_VolumeReport(aout, sys->volume.volume );
    MuteSet(aout, var_InheritBool(aout, "mute"));

    sys->hnotify_evt = CreateEvent(NULL, FALSE, TRUE, NULL);
    if( !sys->hnotify_evt )
    {
        msg_Err(aout, "cannot create Event");
        FreeLibrary(sys->hdsound_dll);
        free(sys);
        return VLC_EGENERIC;
    }

    /* DirectSound does not support hot-plug events (unless with WASAPI) */
    char **ids, **names;
    int count = ReloadDirectXDevices(obj, NULL, &ids, &names);
    if (count >= 0)
    {
        for (int i = 0; i < count; i++)
        {
            aout_HotplugReport(aout, ids[i], names[i]);
            free(names[i]);
            free(ids[i]);
        }
        free(names);
        free(ids);
    }
    var_Create(aout, "directx-audio-device", VLC_VAR_STRING|VLC_VAR_DOINHERIT);
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    var_Destroy(aout, "directx-audio-device");
    CloseHandle(sys->hnotify_evt);
    FreeLibrary(sys->hdsound_dll); /* free DSOUND.DLL */
    free(sys);
}

static void Flush ( audio_output_t * aout, bool drain )
{
    aout_sys_t *sys = aout->sys;
    size_t read;
    if( drain )
    {
        if( sys->p_notify )
        {
            DSBPOSITIONNOTIFY notif = {.dwOffset = aout->sys->i_write, .hEventNotify = sys->hnotify_evt } ;
            if( IDirectSoundNotify_SetNotificationPositions( sys->p_notify, 1, &notif ) ==  DS_OK )
            {
                WaitForSingleObject( sys->hnotify_evt, INFINITE );
                IDirectSoundBuffer_Stop( aout->sys->p_dsbuffer );
            }
        }
        else
            while( IDirectSoundBuffer_GetCurrentPosition( aout->sys->p_dsbuffer,(LPDWORD) &read, NULL) ==  DS_OK )
            {
                read %= DS_BUF_SIZE;
                if( read == aout->sys->i_write )
                    break;
                msleep(10000);
            }
    }
    else
    {
        IDirectSoundBuffer_Stop( aout->sys->p_dsbuffer );
        IDirectSoundBuffer_SetCurrentPosition( aout->sys->p_dsbuffer, 
                                               aout->sys->i_write );
    }
}

static void Pause( audio_output_t * aout, bool pause, mtime_t date )
{
    (void) date;
    if( pause )
        IDirectSoundBuffer_Stop( aout->sys->p_dsbuffer );
    else
        IDirectSoundBuffer_Play( aout->sys->p_dsbuffer, 0, 0, DSBPLAY_LOOPING );
}


static int TimeGet( audio_output_t * aout, mtime_t * delay )
{
    uint32_t read;
    mtime_t size;

    if( IDirectSoundBuffer_GetCurrentPosition( aout->sys->p_dsbuffer, (LPDWORD) &read, NULL) !=  DS_OK )
        return 1;

    read %= DS_BUF_SIZE;

    size = (mtime_t)aout->sys->i_write - (mtime_t) read;
    if( size < 0 )
        size += DS_BUF_SIZE;

    *delay = ( size / aout->sys->i_bytes_per_sample ) * CLOCK_FREQ / aout->sys->i_rate;
    return 0;
}
