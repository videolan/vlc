/*****************************************************************************
 * waveout.c : Windows waveOut plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          André Weber
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <wchar.h>

#define UNICODE
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_aout_intf.h>
#include <vlc_charset.h> /* FromWide() */
#include <vlc_atomic.h>

#include "windows_audio_common.h"

#define FRAME_SIZE 4096              /* The size is in samples, not in bytes */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );
static void Play         ( audio_output_t *, block_t * );

/*****************************************************************************
 * notification_thread_t: waveOut event thread
 *****************************************************************************/
/* local functions */
static void Probe        ( audio_output_t * );
static int OpenWaveOut   ( audio_output_t *, uint32_t,
                           int, int, int, int, bool );
static int OpenWaveOutPCM( audio_output_t *, uint32_t,
                           vlc_fourcc_t*, int, int, int, bool );
static int PlayWaveOut   ( audio_output_t *, HWAVEOUT, WAVEHDR *,
                           aout_buffer_t *, bool );

static void CALLBACK WaveOutCallback ( HWAVEOUT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR );
static void* WaveOutThread( void * );

static int VolumeSet( audio_output_t *, float, bool );

static int WaveOutClearDoneBuffers(aout_sys_t *p_sys);

static int ReloadWaveoutDevices( vlc_object_t *, char const *,
                                vlc_value_t, vlc_value_t, void * );
static uint32_t findDeviceID(char *);

static const wchar_t device_name_fmt[] = L"%ls ($%x,$%x)";

static const char *const ppsz_adev[] = { "wavemapper", };
static const char *const ppsz_adev_text[] = { N_("Microsoft Soundmapper") };


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DEVICE_TEXT N_("Select Audio Device")
#define DEVICE_LONG N_("Select special Audio device, or let windows "\
                       "decide (default), change needs VLC restart "\
                       "to apply.")
#define DEFAULT_AUDIO_DEVICE N_("Default Audio Device")

vlc_module_begin ()
    set_shortname( "WaveOut" )
    set_description( N_("Win32 waveOut extension output") )
    set_capability( "audio output", 50 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )

    add_string( "waveout-audio-device", "wavemapper",
                 DEVICE_TEXT, DEVICE_LONG, false )
       change_string_list( ppsz_adev, ppsz_adev_text, ReloadWaveoutDevices )
       change_action_add( ReloadWaveoutDevices, N_("Refresh list") )

    add_bool( "waveout-float32", true, FLOAT_TEXT, FLOAT_LONGTEXT, true )

    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * aout_sys_t: waveOut audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the waveOut specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    aout_packet_t packet;
    uint32_t i_wave_device_id;               /* ID of selected output device */

    HWAVEOUT h_waveout;                        /* handle to waveout instance */

    WAVEFORMATEXTENSIBLE waveformat;                         /* audio format */

    WAVEHDR waveheader[FRAMES_NUM];

    vlc_thread_t thread;
    vlc_atomic_t abort;
    HANDLE event;
    HANDLE new_buffer_event;

    // rental from alsa.c to synchronize startup of audiothread
    int b_playing;                                         /* playing status */
    mtime_t start_date;

    int i_repeat_counter;

    int i_buffer_size;

    uint8_t *p_silence_buffer;              /* buffer we use to play silence */

    bool b_chan_reorder;              /* do we need channel reordering */
    int pi_chan_table[AOUT_CHAN_MAX];
};

/*****************************************************************************
 * Open: open the audio device
 *****************************************************************************
 * This function opens and setups Win32 waveOut
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    audio_output_t *p_aout = (audio_output_t *)p_this;
    vlc_value_t val;

    /* Allocate structure */
    p_aout->sys = malloc( sizeof( aout_sys_t ) );

    if( p_aout->sys == NULL )
        return VLC_ENOMEM;

    p_aout->pf_play = Play;
    p_aout->pf_pause = aout_PacketPause;
    p_aout->pf_flush = aout_PacketFlush;

    /*
     initialize/update Device selection List
    */
    ReloadWaveoutDevices( p_this, "waveout-audio-device", val, val, NULL);


    /*
      check for configured audio device!
    */
    char *psz_waveout_dev = var_CreateGetString( p_aout, "waveout-audio-device");

    p_aout->sys->i_wave_device_id =
         findDeviceID( psz_waveout_dev );

    if(p_aout->sys->i_wave_device_id == WAVE_MAPPER)
    {
       if(psz_waveout_dev &&
          stricmp(psz_waveout_dev,"wavemapper"))
       {
           msg_Warn( p_aout, "configured audio device '%s' not available, "\
                         "use default instead", psz_waveout_dev );
       }
    }
    free( psz_waveout_dev );


    WAVEOUTCAPS waveoutcaps;
    if(waveOutGetDevCaps( p_aout->sys->i_wave_device_id,
                          &waveoutcaps,
                          sizeof(WAVEOUTCAPS)) == MMSYSERR_NOERROR)
    {
      /* log debug some infos about driver, to know who to blame
         if it doesn't work */
        msg_Dbg( p_aout, "Drivername: %ls", waveoutcaps.szPname);
        msg_Dbg( p_aout, "Driver Version: %d.%d",
                          (waveoutcaps.vDriverVersion>>8)&255,
                          waveoutcaps.vDriverVersion & 255);
        msg_Dbg( p_aout, "Manufacturer identifier: 0x%x", waveoutcaps.wMid );
        msg_Dbg( p_aout, "Product identifier: 0x%x", waveoutcaps.wPid );
    }



    if( var_Type( p_aout, "audio-device" ) == 0 )
    {
        Probe( p_aout );
    }

    if( var_Get( p_aout, "audio-device", &val ) < 0 )
    {
        /* Probe() has failed. */
        var_Destroy( p_aout, "waveout-audio-device");
        free( p_aout->sys );
        return VLC_EGENERIC;
    }


    /* Open the device */
    if( val.i_int == AOUT_VAR_SPDIF )
    {
        p_aout->format.i_format = VLC_CODEC_SPDIFL;

        if( OpenWaveOut( p_aout,
                         p_aout->sys->i_wave_device_id,
                         VLC_CODEC_SPDIFL,
                         p_aout->format.i_physical_channels,
                         aout_FormatNbChannels( &p_aout->format ),
                         p_aout->format.i_rate, false )
            != VLC_SUCCESS )
        {
            msg_Err( p_aout, "cannot open waveout audio device" );
            free( p_aout->sys );
            return VLC_EGENERIC;
        }

        /* Calculate the frame size in bytes */
        p_aout->format.i_bytes_per_frame = AOUT_SPDIF_SIZE;
        p_aout->format.i_frame_length = A52_FRAME_NB;
        p_aout->sys->i_buffer_size =
            p_aout->format.i_bytes_per_frame;

        aout_PacketInit( p_aout, &p_aout->sys->packet, A52_FRAME_NB );
        aout_VolumeNoneInit( p_aout );
    }
    else
    {
        WAVEOUTCAPS wocaps;

        switch( val.i_int )
        {
        case AOUT_VAR_5_1:
            p_aout->format.i_physical_channels
                    = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
                      | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                      | AOUT_CHAN_LFE;
            break;
        case AOUT_VAR_2F2R:
            p_aout->format.i_physical_channels
                    = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                      | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
            break;
        case AOUT_VAR_MONO:
            p_aout->format.i_physical_channels = AOUT_CHAN_CENTER;
            break;
        default:
            p_aout->format.i_physical_channels
                    = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
        }

        if( OpenWaveOutPCM( p_aout,
                            p_aout->sys->i_wave_device_id,
                            &p_aout->format.i_format,
                            p_aout->format.i_physical_channels,
                            aout_FormatNbChannels( &p_aout->format ),
                            p_aout->format.i_rate, false )
            != VLC_SUCCESS )
        {
            msg_Err( p_aout, "cannot open waveout audio device" );
            free( p_aout->sys );
            return VLC_EGENERIC;
        }

        /* Calculate the frame size in bytes */
        aout_FormatPrepare( &p_aout->format );
        p_aout->sys->i_buffer_size = FRAME_SIZE *
            p_aout->format.i_bytes_per_frame;

        aout_PacketInit( p_aout, &p_aout->sys->packet, FRAME_SIZE );
        aout_VolumeSoftInit( p_aout );

        /* Check for hardware volume support */
        if( waveOutGetDevCaps( (UINT_PTR)p_aout->sys->h_waveout,
                               &wocaps, sizeof(wocaps) ) == MMSYSERR_NOERROR &&
            wocaps.dwSupport & WAVECAPS_VOLUME )
        {
            DWORD i_dummy;
            if( waveOutGetVolume( p_aout->sys->h_waveout, &i_dummy )
                == MMSYSERR_NOERROR )
            {
                p_aout->pf_volume_set = VolumeSet;
            }
        }
    }


    waveOutReset( p_aout->sys->h_waveout );

    /* Allocate silence buffer */
    p_aout->sys->p_silence_buffer =
        malloc( p_aout->sys->i_buffer_size );
    if( p_aout->sys->p_silence_buffer == NULL )
    {
        aout_PacketDestroy( p_aout );
        free( p_aout->sys );
        return VLC_ENOMEM;
    }
    p_aout->sys->i_repeat_counter = 0;


    /* Zero the buffer. WinCE doesn't have calloc(). */
    memset( p_aout->sys->p_silence_buffer, 0,
            p_aout->sys->i_buffer_size );

    /* Now we need to setup our waveOut play notification structure */
    p_aout->sys->event = CreateEvent( NULL, FALSE, FALSE, NULL );
    p_aout->sys->new_buffer_event = CreateEvent( NULL, FALSE, FALSE, NULL );

    /* define startpoint of playback on first call to play()
      like alsa does (instead of playing a blank sample) */
    p_aout->sys->b_playing = 0;
    p_aout->sys->start_date = 0;


    /* Then launch the notification thread */
    vlc_atomic_set( &p_aout->sys->abort, 0);
    if( vlc_clone( &p_aout->sys->thread,
                   WaveOutThread, p_aout, VLC_THREAD_PRIORITY_OUTPUT ) )
    {
        msg_Err( p_aout, "cannot create WaveOutThread" );
    }

    /* We need to kick off the playback in order to have the callback properly
     * working */
    for( int i = 0; i < FRAMES_NUM; i++ )
    {
        p_aout->sys->waveheader[i].dwFlags = WHDR_DONE;
        p_aout->sys->waveheader[i].dwUser = 0;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Probe: probe the audio device for available formats and channels
 *****************************************************************************/
static void Probe( audio_output_t * p_aout )
{
    vlc_value_t val, text;
    vlc_fourcc_t i_format;
    unsigned int i_physical_channels;

    var_Create( p_aout, "audio-device", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Audio Device");
    var_Change( p_aout, "audio-device", VLC_VAR_SETTEXT, &text, NULL );

    /* Test for 5.1 support */
    i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                          AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT |
                          AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE;
    if( p_aout->format.i_physical_channels == i_physical_channels )
    {
        if( OpenWaveOutPCM( p_aout,
                            p_aout->sys->i_wave_device_id,
                            &i_format,
                            i_physical_channels, 6,
                            p_aout->format.i_rate, true )
            == VLC_SUCCESS )
        {
            val.i_int = AOUT_VAR_5_1;
            text.psz_string = (char *)_("5.1");
            var_Change( p_aout, "audio-device",
                        VLC_VAR_ADDCHOICE, &val, &text );
            msg_Dbg( p_aout, "device supports 5.1 channels" );
        }
    }

    /* Test for 2 Front 2 Rear support */
    i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                          AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
    if( ( p_aout->format.i_physical_channels & i_physical_channels )
        == i_physical_channels )
    {
        if( OpenWaveOutPCM( p_aout,
                            p_aout->sys->i_wave_device_id,
                            &i_format,
                            i_physical_channels, 4,
                            p_aout->format.i_rate, true )
            == VLC_SUCCESS )
        {
            val.i_int = AOUT_VAR_2F2R;
            text.psz_string = (char *)_("2 Front 2 Rear");
            var_Change( p_aout, "audio-device",
                        VLC_VAR_ADDCHOICE, &val, &text );
            msg_Dbg( p_aout, "device supports 4 channels" );
        }
    }

    /* Test for stereo support */
    i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    if( OpenWaveOutPCM( p_aout,
                        p_aout->sys->i_wave_device_id,
                        &i_format,
                        i_physical_channels, 2,
                        p_aout->format.i_rate, true )
        == VLC_SUCCESS )
    {
        val.i_int = AOUT_VAR_STEREO;
        text.psz_string = (char *)_("Stereo");
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text );
        msg_Dbg( p_aout, "device supports 2 channels" );
    }

    /* Test for mono support */
    i_physical_channels = AOUT_CHAN_CENTER;
    if( OpenWaveOutPCM( p_aout,
                        p_aout->sys->i_wave_device_id,
                        &i_format,
                        i_physical_channels, 1,
                        p_aout->format.i_rate, true )
        == VLC_SUCCESS )
    {
        val.i_int = AOUT_VAR_MONO;
        text.psz_string = (char *)_("Mono");
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text );
        msg_Dbg( p_aout, "device supports 1 channel" );
    }

    /* Test for SPDIF support */
    if ( AOUT_FMT_SPDIF( &p_aout->format ) )
    {
        if( OpenWaveOut( p_aout,
                         p_aout->sys->i_wave_device_id,
                         VLC_CODEC_SPDIFL,
                         p_aout->format.i_physical_channels,
                         aout_FormatNbChannels( &p_aout->format ),
                         p_aout->format.i_rate, true )
            == VLC_SUCCESS )
        {
            msg_Dbg( p_aout, "device supports A/52 over S/PDIF" );
            val.i_int = AOUT_VAR_SPDIF;
            text.psz_string = (char *)_("A/52 over S/PDIF");
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
    var_TriggerCallback( p_aout, "intf-change" );
}

/*****************************************************************************
 * Play: play a sound buffer
 *****************************************************************************
 * This doesn't actually play the buffer. This just stores the buffer so it
 * can be played by the callback thread.
 *****************************************************************************/
static void Play( audio_output_t *_p_aout, block_t *block )
{
    if( !_p_aout->sys->b_playing )
    {
        _p_aout->sys->b_playing = 1;

        /* get the playing date of the first aout buffer */
        _p_aout->sys->start_date = block->i_pts;

        msg_Dbg( _p_aout, "Wakeup sleeping output thread.");

        /* wake up the audio output thread */
        SetEvent( _p_aout->sys->event );
    } else {
        SetEvent( _p_aout->sys->new_buffer_event );
    }

    aout_PacketPlay( _p_aout, block );
}

/*****************************************************************************
 * Close: close the audio device
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    audio_output_t *p_aout = (audio_output_t *)p_this;
    aout_sys_t *p_sys = p_aout->sys;

    /* Before calling waveOutClose we must reset the device */
    vlc_atomic_set( &p_sys->abort, 1);

    /* wake up the audio thread, to recognize that p_aout died */
    SetEvent( p_sys->event );
    SetEvent( p_sys->new_buffer_event );

    vlc_join( p_sys->thread, NULL );

    /*
      kill the real output then - when the feed thread
      is surely terminated!
      old code could be too early in case that "feeding"
      was running on termination

      at this point now its sure, that there will be no new
      data send to the driver, and we can cancel the last
      running playbuffers
    */
    MMRESULT result = waveOutReset( p_sys->h_waveout );
    if(result != MMSYSERR_NOERROR)
    {
       msg_Err( p_aout, "waveOutReset failed 0x%x", result );
       /*
        now we must wait, that all buffers are played
        because cancel doesn't work in this case...
       */
       if(result == MMSYSERR_NOTSUPPORTED)
       {
           /*
             clear currently played (done) buffers,
             if returnvalue > 0 (means some buffer still playing)
             wait for the driver event callback that one buffer
             is finished with playing, and check again
             the timeout of 5000ms is just, an emergency exit
             of this loop, to avoid deadlock in case of other
             (currently not known bugs, problems, errors cases?)
           */
           while(
                 (WaveOutClearDoneBuffers( p_sys ) > 0)
                 &&
                 (WaitForSingleObject( p_sys->event, 5000) == WAIT_OBJECT_0)
                )
           {
                 msg_Dbg( p_aout, "Wait for waveout device...");
           }
       }
    } else {
        WaveOutClearDoneBuffers( p_sys );
    }

    /* now we can Close the device */
    if( waveOutClose( p_sys->h_waveout ) != MMSYSERR_NOERROR )
    {
        msg_Err( p_aout, "waveOutClose failed" );
    }

    /*
      because so long, the waveout device is playing, the callback
      could occur and need the events
    */
    CloseHandle( p_sys->event );
    CloseHandle( p_sys->new_buffer_event);

    free( p_sys->p_silence_buffer );
    aout_PacketDestroy( p_aout );
    free( p_sys );
}

/*****************************************************************************
 * OpenWaveOut: open the waveout sound device
 ****************************************************************************/
static int OpenWaveOut( audio_output_t *p_aout, uint32_t i_device_id, int i_format,
                        int i_channels, int i_nb_channels, int i_rate,
                        bool b_probe )
{
    MMRESULT result;

    /* Set sound format */

#define waveformat p_aout->sys->waveformat

    waveformat.dwChannelMask = 0;
    for( unsigned i = 0; i < sizeof(pi_channels_src)/sizeof(uint32_t); i++ )
    {
        if( i_channels & pi_channels_src[i] )
            waveformat.dwChannelMask |= pi_channels_in[i];
    }

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
        waveformat.SubFormat = __KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF;
        break;

    case VLC_CODEC_FL32:
        waveformat.Format.wBitsPerSample = sizeof(float) * 8;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        waveformat.SubFormat = __KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        break;

    case VLC_CODEC_S16L:
        waveformat.Format.wBitsPerSample = 16;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_PCM;
        waveformat.SubFormat = __KSDATAFORMAT_SUBTYPE_PCM;
        break;
    }

    waveformat.Format.nChannels = i_nb_channels;
    waveformat.Format.nSamplesPerSec = i_rate;
    waveformat.Format.nBlockAlign =
        waveformat.Format.wBitsPerSample / 8 * i_nb_channels;
    waveformat.Format.nAvgBytesPerSec =
        waveformat.Format.nSamplesPerSec * waveformat.Format.nBlockAlign;

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
    }

    if(!b_probe) {
        msg_Dbg( p_aout, "OpenWaveDevice-ID: %u", i_device_id);
        msg_Dbg( p_aout,"waveformat.Format.cbSize          = %d",
                 waveformat.Format.cbSize);
        msg_Dbg( p_aout,"waveformat.Format.wFormatTag      = %u",
                 waveformat.Format.wFormatTag);
        msg_Dbg( p_aout,"waveformat.Format.nChannels       = %u",
                 waveformat.Format.nChannels);
        msg_Dbg( p_aout,"waveformat.Format.nSamplesPerSec  = %d",
                 (int)waveformat.Format.nSamplesPerSec);
        msg_Dbg( p_aout,"waveformat.Format.nAvgBytesPerSec = %u",
                 (int)waveformat.Format.nAvgBytesPerSec);
        msg_Dbg( p_aout,"waveformat.Format.nBlockAlign     = %d",
                 waveformat.Format.nBlockAlign);
        msg_Dbg( p_aout,"waveformat.Format.wBitsPerSample  = %d",
                 waveformat.Format.wBitsPerSample);
        msg_Dbg( p_aout,"waveformat.Samples.wValidBitsPerSample = %d",
                 waveformat.Samples.wValidBitsPerSample);
        msg_Dbg( p_aout,"waveformat.Samples.wSamplesPerBlock = %d",
                 waveformat.Samples.wSamplesPerBlock);
        msg_Dbg( p_aout,"waveformat.dwChannelMask          = %lu",
                 waveformat.dwChannelMask);
    }

    /* Open the device */
    result = waveOutOpen( &p_aout->sys->h_waveout, i_device_id,
                          (WAVEFORMATEX *)&waveformat,
                          (DWORD_PTR)WaveOutCallback, (DWORD_PTR)p_aout,
                          CALLBACK_FUNCTION | (b_probe?WAVE_FORMAT_QUERY:0) );
    if( result == WAVERR_BADFORMAT )
    {
        msg_Warn( p_aout, "waveOutOpen failed WAVERR_BADFORMAT" );
        return VLC_EGENERIC;
    }
    if( result == MMSYSERR_ALLOCATED )
    {
        msg_Warn( p_aout, "waveOutOpen failed WAVERR_ALLOCATED" );
        return VLC_EGENERIC;
    }
    if( result != MMSYSERR_NOERROR )
    {
        msg_Warn( p_aout, "waveOutOpen failed" );
        return VLC_EGENERIC;
    }

    p_aout->sys->b_chan_reorder =
        aout_CheckChannelReorder( pi_channels_in, pi_channels_out,
                                  waveformat.dwChannelMask, i_nb_channels,
                                  p_aout->sys->pi_chan_table );

    if( p_aout->sys->b_chan_reorder )
    {
        msg_Dbg( p_aout, "channel reordering needed" );
    }

    return VLC_SUCCESS;

#undef waveformat

}

/*****************************************************************************
 * OpenWaveOutPCM: open a PCM waveout sound device
 ****************************************************************************/
static int OpenWaveOutPCM( audio_output_t *p_aout, uint32_t i_device_id,
                           vlc_fourcc_t *i_format,
                           int i_channels, int i_nb_channels, int i_rate,
                           bool b_probe )
{
    bool b_use_float32 = var_CreateGetBool( p_aout, "waveout-float32");

    if( !b_use_float32 || OpenWaveOut( p_aout, i_device_id, VLC_CODEC_FL32,
                                   i_channels, i_nb_channels, i_rate, b_probe )
        != VLC_SUCCESS )
    {
        if ( OpenWaveOut( p_aout, i_device_id, VLC_CODEC_S16L,
                          i_channels, i_nb_channels, i_rate, b_probe )
             != VLC_SUCCESS )
        {
            return VLC_EGENERIC;
        }
        else
        {
            *i_format = VLC_CODEC_S16L;
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
 * PlayWaveOut: play a buffer through the WaveOut device
 *****************************************************************************/
static int PlayWaveOut( audio_output_t *p_aout, HWAVEOUT h_waveout,
                        WAVEHDR *p_waveheader, aout_buffer_t *p_buffer,
                        bool b_spdif)
{
    MMRESULT result;

    /* Prepare the buffer */
    if( p_buffer != NULL )
    {
        p_waveheader->lpData = (LPSTR)p_buffer->p_buffer;
        /*
          copy the buffer to the silence buffer :) so in case we don't
          get the next buffer fast enough (I will repeat this one a time
          for AC3 / DTS and SPDIF this will sound better instead of
          a hickup)
        */
        if(b_spdif)
        {
           vlc_memcpy( p_aout->sys->p_silence_buffer,
                       p_buffer->p_buffer,
                       p_aout->sys->i_buffer_size );
           p_aout->sys->i_repeat_counter = 2;
        }
    } else {
        /* Use silence buffer instead */
        if(p_aout->sys->i_repeat_counter)
        {
           p_aout->sys->i_repeat_counter--;
           if(!p_aout->sys->i_repeat_counter)
           {
               vlc_memset( p_aout->sys->p_silence_buffer,
                           0x00, p_aout->sys->i_buffer_size );
           }
        }
        p_waveheader->lpData = (LPSTR)p_aout->sys->p_silence_buffer;
    }

    p_waveheader->dwUser = p_buffer ? (DWORD_PTR)p_buffer : (DWORD_PTR)1;
    p_waveheader->dwBufferLength = p_aout->sys->i_buffer_size;
    p_waveheader->dwFlags = 0;

    result = waveOutPrepareHeader( h_waveout, p_waveheader, sizeof(WAVEHDR) );
    if( result != MMSYSERR_NOERROR )
    {
        msg_Err( p_aout, "waveOutPrepareHeader failed" );
        return VLC_EGENERIC;
    }

    /* Send the buffer to the waveOut queue */
    result = waveOutWrite( h_waveout, p_waveheader, sizeof(WAVEHDR) );
    if( result != MMSYSERR_NOERROR )
    {
        msg_Err( p_aout, "waveOutWrite failed" );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * WaveOutCallback: what to do once WaveOut has played its sound samples
 *****************************************************************************/
static void CALLBACK WaveOutCallback( HWAVEOUT h_waveout, UINT uMsg,
                                      DWORD_PTR _p_aout,
                                      DWORD_PTR dwParam1, DWORD_PTR dwParam2 )
{
    (void)h_waveout;    (void)dwParam1;    (void)dwParam2;
    audio_output_t *p_aout = (audio_output_t *)_p_aout;
    int i_queued_frames = 0;

    if( uMsg != WOM_DONE ) return;

    if( vlc_atomic_get(&p_aout->sys->abort) ) return;

    /* Find out the current latency */
    for( int i = 0; i < FRAMES_NUM; i++ )
    {
        /* Check if frame buf is available */
        if( !(p_aout->sys->waveheader[i].dwFlags & WHDR_DONE) )
        {
            i_queued_frames++;
        }
    }

    /* Don't wake up the thread too much */
    if( i_queued_frames <= FRAMES_NUM/2 )
        SetEvent( p_aout->sys->event );
}


/****************************************************************************
 * WaveOutClearDoneBuffers: Clear all done marked buffers, and free aout_bufer
 ****************************************************************************
 * return value is the number of still playing buffers in the queue
 ****************************************************************************/
static int WaveOutClearDoneBuffers(aout_sys_t *p_sys)
{
    WAVEHDR *p_waveheader = p_sys->waveheader;
    int i_queued_frames = 0;

    for( int i = 0; i < FRAMES_NUM; i++ )
    {
        if( (p_waveheader[i].dwFlags & WHDR_DONE) &&
            p_waveheader[i].dwUser )
        {
            aout_buffer_t *p_buffer =
                    (aout_buffer_t *)(p_waveheader[i].dwUser);
            /* Unprepare and free the buffers which has just been played */
            waveOutUnprepareHeader( p_sys->h_waveout, &p_waveheader[i],
                                    sizeof(WAVEHDR) );

            if( p_waveheader[i].dwUser != 1 )
                aout_BufferFree( p_buffer );

            p_waveheader[i].dwUser = 0;
        }

        /* Check if frame buf is available */
        if( !(p_waveheader[i].dwFlags & WHDR_DONE) )
        {
            i_queued_frames++;
        }
    }
    return i_queued_frames;
}

/*****************************************************************************
 * WaveOutThread: this thread will capture play notification events.
 *****************************************************************************
 * We use this thread to feed new audio samples to the sound card because
 * we are not authorized to use waveOutWrite() directly in the waveout
 * callback.
 *****************************************************************************/
static void* WaveOutThread( void *data )
{
    audio_output_t *p_aout = data;
    aout_sys_t *p_sys = p_aout->sys;
    aout_buffer_t *p_buffer = NULL;
    WAVEHDR *p_waveheader = p_sys->waveheader;
    int i, i_queued_frames;
    bool b_sleek;
    mtime_t next_date;
    int canc = vlc_savecancel ();

    /* We don't want any resampling when using S/PDIF */
    b_sleek = p_aout->format.i_format == VLC_CODEC_SPDIFL;

    // wait for first call to "play()"
    while( !p_sys->start_date && !vlc_atomic_get(&p_aout->sys->abort) )
           WaitForSingleObject( p_sys->event, INFINITE );
    if( vlc_atomic_get(&p_aout->sys->abort) )
        return NULL;

    msg_Dbg( p_aout, "will start to play in %"PRId64" us",
             (p_sys->start_date - AOUT_MAX_PTS_ADVANCE/4)-mdate());

    // than wait a short time... before grabbing first frames
    mwait( p_sys->start_date - AOUT_MAX_PTS_ADVANCE/4 );

#define waveout_warn(msg) msg_Warn( p_aout, "aout_PacketNext no buffer "\
                           "got next_date=%d ms, "\
                           "%d frames to play, %s",\
                           (int)(next_date/(mtime_t)1000), \
                           i_queued_frames, msg);
    next_date = mdate();

    while( !vlc_atomic_get(&p_aout->sys->abort) )
    {
        /* Cleanup and find out the current latency */
        i_queued_frames = WaveOutClearDoneBuffers( p_sys );

        if( vlc_atomic_get(&p_aout->sys->abort) ) return NULL;

        /* Try to fill in as many frame buffers as possible */
        for( i = 0; i < FRAMES_NUM; i++ )
        {
            /* Check if frame buf is available */
            if( p_waveheader[i].dwFlags & WHDR_DONE )
            {
                // next_date = mdate() + 1000000 * i_queued_frames /
                //  p_aout->format.i_rate * p_aout->i_nb_samples;

                // the realtime has got our back-site:) to come in sync
                if(next_date < mdate())
                   next_date = mdate();


                /* Take into account the latency */
                p_buffer = aout_PacketNext( p_aout, next_date );
                if(!p_buffer)
                {
#if 0
                    msg_Dbg( p_aout, "aout_PacketNext no buffer got "
                             "next_date=%"PRId64" ms, %d frames to play",
                             next_date/1000, i_queued_frames);
#endif
                    // means we are too early to request a new buffer?
                    waveout_warn("waiting...")
                    mwait( next_date - AOUT_MAX_PTS_ADVANCE/4 );
                    next_date = mdate();
                    p_buffer = aout_PacketNext( p_aout, next_date );
                }

                if( !p_buffer && i_queued_frames )
                {
                    /* We aren't late so no need to play a blank sample */
                    break;
                }

                if( p_buffer )
                {
                    mtime_t buffer_length = p_buffer->i_length;
                    next_date = next_date + buffer_length;
                }

                /* Do the channel reordering */
                if( p_buffer && p_sys->b_chan_reorder )
                {
                    aout_ChannelReorder( p_buffer->p_buffer,
                        p_buffer->i_buffer,
                        p_sys->waveformat.Format.nChannels,
                        p_sys->pi_chan_table,
                        p_sys->waveformat.Format.wBitsPerSample );
                }

                PlayWaveOut( p_aout, p_sys->h_waveout,
                             &p_waveheader[i], p_buffer, b_sleek );

                i_queued_frames++;
            }
        }

        if( vlc_atomic_get(&p_aout->sys->abort) ) return NULL;

        /*
          deal with the case that the loop didn't fillup the buffer to the
          max - instead of waiting that half the buffer is played before
          fillup the waveout buffers, wait only for the next sample buffer
          to arrive at the play method...

          this will also avoid, that the last buffer is play until the
          end, and then trying to get more data, so it will also
          work - if the next buffer will arrive some ms before the
          last buffer is finished.
        */
        if(i_queued_frames < FRAMES_NUM)
           WaitForSingleObject( p_sys->new_buffer_event, INFINITE );
        else
           WaitForSingleObject( p_sys->event, INFINITE );

    }

#undef waveout_warn
    vlc_restorecancel (canc);
    return NULL;
}

static int VolumeSet( audio_output_t * p_aout, float volume, bool mute )
{
    if( mute )
        volume = 0.;

    unsigned long i_waveout_vol = volume
        * (0xFFFF * AOUT_VOLUME_DEFAULT / AOUT_VOLUME_MAX);

    if( i_waveout_vol <= 0xFFFF )
        i_waveout_vol |= i_waveout_vol << 16;
    else
        i_waveout_vol = 0xFFFFFFFF;

#ifdef UNDER_CE
    waveOutSetVolume( 0, i_waveout_vol );
#else
    waveOutSetVolume( p_aout->sys->h_waveout, i_waveout_vol );
#endif
    return 0;
}


/*
  reload the configuration drop down list, of the Audio Devices
*/
static int ReloadWaveoutDevices( vlc_object_t *p_this, char const *psz_name,
                                 vlc_value_t newval, vlc_value_t oldval, void *data )
{
    VLC_UNUSED( newval ); VLC_UNUSED( oldval ); VLC_UNUSED( data );

    module_config_t *p_item = config_FindConfig( p_this, psz_name );
    if( !p_item ) return VLC_SUCCESS;

    /* Clear-up the current list */
    if( p_item->i_list )
    {
        int i;

        /* Keep the first entry */
        for( i = 1; i < p_item->i_list; i++ )
        {
            free((char *)(p_item->ppsz_list[i]) );
            free((char *)(p_item->ppsz_list_text[i]) );
        }
        /* TODO: Remove when no more needed */
        p_item->ppsz_list[i] = NULL;
        p_item->ppsz_list_text[i] = NULL;
    }
    p_item->i_list = 1;

    int wave_devices = waveOutGetNumDevs();

    p_item->ppsz_list = xrealloc( p_item->ppsz_list,
                          (wave_devices+2) * sizeof(char *) );
    p_item->ppsz_list_text = xrealloc( p_item->ppsz_list_text,
                          (wave_devices+2) * sizeof(char *) );

    int j=1;
    for(int i=0; i<wave_devices; i++)
    {
        WAVEOUTCAPS caps;
        wchar_t dev_name[MAXPNAMELEN+32];

        if(waveOutGetDevCaps(i, &caps, sizeof(WAVEOUTCAPS))
                                                           != MMSYSERR_NOERROR)
            continue;

        swprintf(dev_name, MAXPNAMELEN + 32, device_name_fmt,
                 caps.szPname, caps.wMid, caps.wPid);
        p_item->ppsz_list[j] = FromWide( dev_name );
        p_item->ppsz_list_text[j] = FromWide( dev_name );
        p_item->i_list++;
        j++;
    }
    p_item->ppsz_list[j] = NULL;
    p_item->ppsz_list_text[j] = NULL;

    /* Signal change to the interface */
    p_item->b_dirty = true;

    return VLC_SUCCESS;
}

/*
  convert devicename to device ID for output
  if device not found return WAVE_MAPPER, so let
  windows decide which preferred audio device
  should be used.
*/
static uint32_t findDeviceID(char *psz_device_name)
{
    if( !psz_device_name )
       return WAVE_MAPPER;

    uint32_t wave_devices = waveOutGetNumDevs();

    for( uint32_t i = 0; i < wave_devices; i++ )
    {
        WAVEOUTCAPS caps;
        wchar_t dev_name[MAXPNAMELEN+32];

        if( waveOutGetDevCaps( i, &caps, sizeof(WAVEOUTCAPS) )
                                                          != MMSYSERR_NOERROR )
            continue;

        swprintf( dev_name, MAXPNAMELEN + 32, device_name_fmt,
                  caps.szPname, caps.wMid, caps.wPid );
        char *u8 = FromWide(dev_name);
        if( !stricmp(u8, psz_device_name) )
        {
            free( u8 );
            return i;
        }
        free( u8 );
    }

    return WAVE_MAPPER;
}
