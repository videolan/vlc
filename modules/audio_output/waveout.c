/*****************************************************************************
 * waveout.c : Windows waveOut plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <string.h>                                            /* strerror() */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include "aout_internal.h"

#include <windows.h>
#include <mmsystem.h>

#define FRAME_SIZE 4096              /* The size is in samples, not in bytes */
#define FRAMES_NUM 8

/*****************************************************************************
 * Useful macros
 *****************************************************************************/
#ifdef UNDER_CE
#   define DWORD_PTR DWORD
#   ifdef waveOutGetDevCaps
#       undef waveOutGetDevCaps
        MMRESULT WINAPI waveOutGetDevCaps(UINT, LPWAVEOUTCAPS, UINT);
#   endif
#endif

#ifndef WAVE_FORMAT_IEEE_FLOAT
#   define WAVE_FORMAT_IEEE_FLOAT 0x0003
#endif

#ifndef WAVE_FORMAT_DOLBY_AC3_SPDIF
#   define WAVE_FORMAT_DOLBY_AC3_SPDIF 0x0092
#endif

#ifndef WAVE_FORMAT_EXTENSIBLE
#define  WAVE_FORMAT_EXTENSIBLE   0xFFFE
#endif

#ifndef SPEAKER_FRONT_LEFT
#   define SPEAKER_FRONT_LEFT             0x1
#   define SPEAKER_FRONT_RIGHT            0x2
#   define SPEAKER_FRONT_CENTER           0x4
#   define SPEAKER_LOW_FREQUENCY          0x8
#   define SPEAKER_BACK_LEFT              0x10
#   define SPEAKER_BACK_RIGHT             0x20
#   define SPEAKER_FRONT_LEFT_OF_CENTER   0x40
#   define SPEAKER_FRONT_RIGHT_OF_CENTER  0x80
#   define SPEAKER_BACK_CENTER            0x100
#   define SPEAKER_SIDE_LEFT              0x200
#   define SPEAKER_SIDE_RIGHT             0x400
#   define SPEAKER_TOP_CENTER             0x800
#   define SPEAKER_TOP_FRONT_LEFT         0x1000
#   define SPEAKER_TOP_FRONT_CENTER       0x2000
#   define SPEAKER_TOP_FRONT_RIGHT        0x4000
#   define SPEAKER_TOP_BACK_LEFT          0x8000
#   define SPEAKER_TOP_BACK_CENTER        0x10000
#   define SPEAKER_TOP_BACK_RIGHT         0x20000
#   define SPEAKER_RESERVED               0x80000000
#endif

#ifndef _WAVEFORMATEXTENSIBLE_
typedef struct {
    WAVEFORMATEX    Format;
    union {
        WORD wValidBitsPerSample;       /* bits of precision  */
        WORD wSamplesPerBlock;          /* valid if wBitsPerSample==0 */
        WORD wReserved;                 /* If neither applies, set to zero. */
    } Samples;
    DWORD           dwChannelMask;      /* which channels are */
                                        /* present in stream  */
    GUID            SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;
#endif

static const GUID __KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {WAVE_FORMAT_IEEE_FLOAT, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static const GUID __KSDATAFORMAT_SUBTYPE_PCM = {WAVE_FORMAT_PCM, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static const GUID __KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF = {WAVE_FORMAT_DOLBY_AC3_SPDIF, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );
static void Play         ( aout_instance_t * );

/*****************************************************************************
 * notification_thread_t: waveOut event thread
 *****************************************************************************/
typedef struct notification_thread_t
{
    VLC_COMMON_MEMBERS
    aout_instance_t *p_aout;

} notification_thread_t;

/* local functions */
static void Probe        ( aout_instance_t * );
static int OpenWaveOut   ( aout_instance_t *, int, int, int, int, vlc_bool_t );
static int OpenWaveOutPCM( aout_instance_t *, int*, int, int, int, vlc_bool_t );
static int PlayWaveOut   ( aout_instance_t *, HWAVEOUT, WAVEHDR *,
                           aout_buffer_t * );

static void CALLBACK WaveOutCallback ( HWAVEOUT, UINT, DWORD, DWORD, DWORD );
static void WaveOutThread( notification_thread_t * );

static int VolumeInfos( aout_instance_t *, audio_volume_t * );
static int VolumeGet( aout_instance_t *, audio_volume_t * );
static int VolumeSet( aout_instance_t *, audio_volume_t );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FLOAT_TEXT N_("Use float32 output")
#define FLOAT_LONGTEXT N_( \
    "The option allows you to enable or disable the high-quality float32 " \
    "audio output mode (which is not well supported by some soundcards)." )

vlc_module_begin();
    set_shortname( "WaveOut" );
    set_description( _("Win32 waveOut extension output") );
    set_capability( "audio output", 50 );
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_AOUT );
    add_bool( "waveout-float32", 1, 0, FLOAT_TEXT, FLOAT_LONGTEXT, VLC_TRUE );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * aout_sys_t: waveOut audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the waveOut specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    HWAVEOUT h_waveout;                        /* handle to waveout instance */

    WAVEFORMATEXTENSIBLE waveformat;                         /* audio format */

    WAVEHDR waveheader[FRAMES_NUM];

    notification_thread_t *p_notif;                      /* WaveOutThread id */
    HANDLE event;

    int i_buffer_size;

    byte_t *p_silence_buffer;               /* buffer we use to play silence */

    vlc_bool_t b_chan_reorder;              /* do we need channel reordering */
    int pi_chan_table[AOUT_CHAN_MAX];
};

static const uint32_t pi_channels_src[] =
    { AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT,
      AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT,
      AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,
      AOUT_CHAN_CENTER, AOUT_CHAN_LFE, 0 };
static const uint32_t pi_channels_in[] =
    { SPEAKER_FRONT_LEFT, SPEAKER_FRONT_RIGHT,
      SPEAKER_SIDE_LEFT, SPEAKER_SIDE_RIGHT,
      SPEAKER_BACK_LEFT, SPEAKER_BACK_RIGHT,
      SPEAKER_FRONT_CENTER, SPEAKER_LOW_FREQUENCY, 0 };
static const uint32_t pi_channels_out[] =
    { SPEAKER_FRONT_LEFT, SPEAKER_FRONT_RIGHT,
      SPEAKER_FRONT_CENTER, SPEAKER_LOW_FREQUENCY,
      SPEAKER_BACK_LEFT, SPEAKER_BACK_RIGHT,
      SPEAKER_SIDE_LEFT, SPEAKER_SIDE_RIGHT, 0 };

/*****************************************************************************
 * Open: open the audio device
 *****************************************************************************
 * This function opens and setups Win32 waveOut
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    vlc_value_t val;
    int i;

    /* Allocate structure */
    p_aout->output.p_sys = malloc( sizeof( aout_sys_t ) );

    if( p_aout->output.p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return VLC_EGENERIC;
    }

    p_aout->output.pf_play = Play;
    p_aout->b_die = VLC_FALSE;

    if( var_Type( p_aout, "audio-device" ) == 0 )
    {
        Probe( p_aout );
    }

    if( var_Get( p_aout, "audio-device", &val ) < 0 )
    {
        /* Probe() has failed. */
        free( p_aout->output.p_sys );
        return VLC_EGENERIC;
    }

    var_Create( p_aout, "waveout-float32", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* Open the device */
    if( val.i_int == AOUT_VAR_SPDIF )
    {
        p_aout->output.output.i_format = VLC_FOURCC('s','p','d','i');

        if( OpenWaveOut( p_aout, VLC_FOURCC('s','p','d','i'),
                         p_aout->output.output.i_physical_channels,
                         aout_FormatNbChannels( &p_aout->output.output ),
                         p_aout->output.output.i_rate, VLC_FALSE )
            != VLC_SUCCESS )
        {
            msg_Err( p_aout, "cannot open waveout audio device" );
            free( p_aout->output.p_sys );
            return VLC_EGENERIC;
        }

        /* Calculate the frame size in bytes */
        p_aout->output.i_nb_samples = A52_FRAME_NB;
        p_aout->output.output.i_bytes_per_frame = AOUT_SPDIF_SIZE;
        p_aout->output.output.i_frame_length = A52_FRAME_NB;
        p_aout->output.p_sys->i_buffer_size =
            p_aout->output.output.i_bytes_per_frame;

        aout_VolumeNoneInit( p_aout );
    }
    else
    {
        WAVEOUTCAPS wocaps;

        if( val.i_int == AOUT_VAR_5_1 )
        {
            p_aout->output.output.i_physical_channels
                = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
                   | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                   | AOUT_CHAN_LFE;
        }
        else if( val.i_int == AOUT_VAR_2F2R )
        {
            p_aout->output.output.i_physical_channels
                = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                   | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
        }
        else if( val.i_int == AOUT_VAR_MONO )
        {
            p_aout->output.output.i_physical_channels = AOUT_CHAN_CENTER;
        }
        else
        {
            p_aout->output.output.i_physical_channels
                = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
        }

        if( OpenWaveOutPCM( p_aout, &p_aout->output.output.i_format,
                            p_aout->output.output.i_physical_channels,
                            aout_FormatNbChannels( &p_aout->output.output ),
                            p_aout->output.output.i_rate, VLC_FALSE )
            != VLC_SUCCESS )
        {
            msg_Err( p_aout, "cannot open waveout audio device" );
            free( p_aout->output.p_sys );
            return VLC_EGENERIC;
        }

        /* Calculate the frame size in bytes */
        p_aout->output.i_nb_samples = FRAME_SIZE;
        aout_FormatPrepare( &p_aout->output.output );
        p_aout->output.p_sys->i_buffer_size = FRAME_SIZE *
            p_aout->output.output.i_bytes_per_frame;

        aout_VolumeSoftInit( p_aout );

        /* Check for hardware volume support */
        if( waveOutGetDevCaps( (UINT_PTR)p_aout->output.p_sys->h_waveout,
                               &wocaps, sizeof(wocaps) ) == MMSYSERR_NOERROR &&
            wocaps.dwSupport & WAVECAPS_VOLUME )
        {
            DWORD i_dummy;
            if( waveOutGetVolume( p_aout->output.p_sys->h_waveout, &i_dummy )
                == MMSYSERR_NOERROR )
            {
                p_aout->output.pf_volume_infos = VolumeInfos;
                p_aout->output.pf_volume_get = VolumeGet;
                p_aout->output.pf_volume_set = VolumeSet;
            }
        }
    }


    waveOutReset( p_aout->output.p_sys->h_waveout );

    /* Allocate silence buffer */
    p_aout->output.p_sys->p_silence_buffer =
        malloc( p_aout->output.p_sys->i_buffer_size );
    if( p_aout->output.p_sys->p_silence_buffer == NULL )
    {
        free( p_aout->output.p_sys );
        msg_Err( p_aout, "out of memory" );
        return 1;
    }

    /* Zero the buffer. WinCE doesn't have calloc(). */
    memset( p_aout->output.p_sys->p_silence_buffer, 0,
            p_aout->output.p_sys->i_buffer_size );

    /* Now we need to setup our waveOut play notification structure */
    p_aout->output.p_sys->p_notif =
        vlc_object_create( p_aout, sizeof(notification_thread_t) );
    p_aout->output.p_sys->p_notif->p_aout = p_aout;
    p_aout->output.p_sys->event = CreateEvent( NULL, FALSE, FALSE, NULL );

    /* Then launch the notification thread */
    if( vlc_thread_create( p_aout->output.p_sys->p_notif,
                           "waveOut Notification Thread", WaveOutThread,
                           VLC_THREAD_PRIORITY_HIGHEST, VLC_FALSE ) )
    {
        msg_Err( p_aout, "cannot create WaveOutThread" );
    }

    /* We need to kick off the playback in order to have the callback properly
     * working */
    for( i = 0; i < FRAMES_NUM; i++ )
    {
        p_aout->output.p_sys->waveheader[i].dwFlags = WHDR_DONE;
        p_aout->output.p_sys->waveheader[i].dwUser = 0;
    }
    PlayWaveOut( p_aout, p_aout->output.p_sys->h_waveout,
                 &p_aout->output.p_sys->waveheader[0], NULL );

    return 0;
}

/*****************************************************************************
 * Probe: probe the audio device for available formats and channels
 *****************************************************************************/
static void Probe( aout_instance_t * p_aout )
{
    vlc_value_t val, text;
    int i_format;
    unsigned int i_physical_channels;

    var_Create( p_aout, "audio-device", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Audio Device");
    var_Change( p_aout, "audio-device", VLC_VAR_SETTEXT, &text, NULL );

    /* Test for 5.1 support */
    i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                          AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT |
                          AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE;
    if( p_aout->output.output.i_physical_channels == i_physical_channels )
    {
        if( OpenWaveOutPCM( p_aout, &i_format,
                            i_physical_channels, 6,
                            p_aout->output.output.i_rate, VLC_TRUE )
            == VLC_SUCCESS )
        {
            val.i_int = AOUT_VAR_5_1;
            text.psz_string = N_("5.1");
            var_Change( p_aout, "audio-device",
                        VLC_VAR_ADDCHOICE, &val, &text );
            msg_Dbg( p_aout, "device supports 5.1 channels" );
        }
    }

    /* Test for 2 Front 2 Rear support */
    i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                          AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
    if( ( p_aout->output.output.i_physical_channels & i_physical_channels )
        == i_physical_channels )
    {
        if( OpenWaveOutPCM( p_aout, &i_format,
                            i_physical_channels, 4,
                            p_aout->output.output.i_rate, VLC_TRUE )
            == VLC_SUCCESS )
        {
            val.i_int = AOUT_VAR_2F2R;
            text.psz_string = N_("2 Front 2 Rear");
            var_Change( p_aout, "audio-device",
                        VLC_VAR_ADDCHOICE, &val, &text );
            msg_Dbg( p_aout, "device supports 4 channels" );
        }
    }

    /* Test for stereo support */
    i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    if( OpenWaveOutPCM( p_aout, &i_format,
                        i_physical_channels, 2,
                        p_aout->output.output.i_rate, VLC_TRUE )
        == VLC_SUCCESS )
    {
        val.i_int = AOUT_VAR_STEREO;
        text.psz_string = N_("Stereo");
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text );
        msg_Dbg( p_aout, "device supports 2 channels" );
    }

    /* Test for mono support */
    i_physical_channels = AOUT_CHAN_CENTER;
    if( OpenWaveOutPCM( p_aout, &i_format,
                        i_physical_channels, 1,
                        p_aout->output.output.i_rate, VLC_TRUE )
        == VLC_SUCCESS )
    {
        val.i_int = AOUT_VAR_MONO;
        text.psz_string = N_("Mono");
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text );
        msg_Dbg( p_aout, "device supports 1 channel" );
    }

    /* Test for SPDIF support */
    if ( AOUT_FMT_NON_LINEAR( &p_aout->output.output ) )
    {
        if( OpenWaveOut( p_aout, VLC_FOURCC('s','p','d','i'),
                         p_aout->output.output.i_physical_channels,
                         aout_FormatNbChannels( &p_aout->output.output ),
                         p_aout->output.output.i_rate, VLC_TRUE )
            == VLC_SUCCESS )
        {
            msg_Dbg( p_aout, "device supports A/52 over S/PDIF" );
            val.i_int = AOUT_VAR_SPDIF;
            text.psz_string = N_("A/52 over S/PDIF");
            var_Change( p_aout, "audio-device",
                        VLC_VAR_ADDCHOICE, &val, &text );
            if( config_GetInt( p_aout, "spdif" ) )
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

    val.b_bool = VLC_TRUE;
    var_Set( p_aout, "intf-change", val );
}

/*****************************************************************************
 * Play: play a sound buffer
 *****************************************************************************
 * This doesn't actually play the buffer. This just stores the buffer so it
 * can be played by the callback thread.
 *****************************************************************************/
static void Play( aout_instance_t *_p_aout )
{
}

/*****************************************************************************
 * Close: close the audio device
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    aout_instance_t *p_aout = (aout_instance_t *)p_this;
    aout_sys_t *p_sys = p_aout->output.p_sys;

    /* Before calling waveOutClose we must reset the device */
    p_aout->b_die = VLC_TRUE;

    waveOutReset( p_sys->h_waveout );

    /* wake up the audio thread */
    SetEvent( p_sys->event );
    vlc_thread_join( p_sys->p_notif );
    vlc_object_destroy( p_sys->p_notif );
    CloseHandle( p_sys->event );

    /* Close the device */
    if( waveOutClose( p_sys->h_waveout ) != MMSYSERR_NOERROR )
    {
        msg_Err( p_aout, "waveOutClose failed" );
    }

    free( p_sys->p_silence_buffer );
    free( p_sys );
}

/*****************************************************************************
 * OpenWaveOut: open the waveout sound device
 ****************************************************************************/
static int OpenWaveOut( aout_instance_t *p_aout, int i_format,
                        int i_channels, int i_nb_channels, int i_rate,
                        vlc_bool_t b_probe )
{
    MMRESULT result;
    unsigned int i;

    /* Set sound format */

#define waveformat p_aout->output.p_sys->waveformat

    waveformat.dwChannelMask = 0;
    for( i = 0; i < sizeof(pi_channels_src)/sizeof(uint32_t); i++ )
    {
        if( i_channels & pi_channels_src[i] )
            waveformat.dwChannelMask |= pi_channels_in[i];
    }

    switch( i_format )
    {
    case VLC_FOURCC('s','p','d','i'):
        i_nb_channels = 2;
        /* To prevent channel re-ordering */
        waveformat.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
        waveformat.Format.wBitsPerSample = 16;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_DOLBY_AC3_SPDIF;
        waveformat.SubFormat = __KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF;
        break;

    case VLC_FOURCC('f','l','3','2'):
        waveformat.Format.wBitsPerSample = sizeof(float) * 8;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        waveformat.SubFormat = __KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        break;

    case VLC_FOURCC('s','1','6','l'):
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

    /* Open the device */
    result = waveOutOpen( &p_aout->output.p_sys->h_waveout, WAVE_MAPPER,
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

    p_aout->output.p_sys->b_chan_reorder =
        aout_CheckChannelReorder( pi_channels_in, pi_channels_out,
                                  waveformat.dwChannelMask, i_nb_channels,
                                  p_aout->output.p_sys->pi_chan_table );

    if( p_aout->output.p_sys->b_chan_reorder )
    {
        msg_Dbg( p_aout, "channel reordering needed" );
    }

    return VLC_SUCCESS;

#undef waveformat

}

/*****************************************************************************
 * OpenWaveOutPCM: open a PCM waveout sound device
 ****************************************************************************/
static int OpenWaveOutPCM( aout_instance_t *p_aout, int *i_format,
                           int i_channels, int i_nb_channels, int i_rate,
                           vlc_bool_t b_probe )
{
    vlc_value_t val;

    var_Get( p_aout, "waveout-float32", &val );

    if( !val.b_bool || OpenWaveOut( p_aout, VLC_FOURCC('f','l','3','2'),
                                   i_channels, i_nb_channels, i_rate, b_probe )
        != VLC_SUCCESS )
    {
        if ( OpenWaveOut( p_aout, VLC_FOURCC('s','1','6','l'),
                          i_channels, i_nb_channels, i_rate, b_probe )
             != VLC_SUCCESS )
        {
            return VLC_EGENERIC;
        }
        else
        {
            *i_format = VLC_FOURCC('s','1','6','l');
            return VLC_SUCCESS;
        }
    }
    else
    {
        *i_format = VLC_FOURCC('f','l','3','2');
        return VLC_SUCCESS;
    }
}

/*****************************************************************************
 * PlayWaveOut: play a buffer through the WaveOut device
 *****************************************************************************/
static int PlayWaveOut( aout_instance_t *p_aout, HWAVEOUT h_waveout,
                        WAVEHDR *p_waveheader, aout_buffer_t *p_buffer )
{
    MMRESULT result;

    /* Prepare the buffer */
    if( p_buffer != NULL )
        p_waveheader->lpData = p_buffer->p_buffer;
    else
        /* Use silence buffer instead */
        p_waveheader->lpData = p_aout->output.p_sys->p_silence_buffer;

    p_waveheader->dwUser = p_buffer ? (DWORD_PTR)p_buffer : (DWORD_PTR)1;
    p_waveheader->dwBufferLength = p_aout->output.p_sys->i_buffer_size;
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
                                      DWORD _p_aout,
                                      DWORD dwParam1, DWORD dwParam2 )
{
    aout_instance_t *p_aout = (aout_instance_t *)_p_aout;
    int i, i_queued_frames = 0;

    if( uMsg != WOM_DONE ) return;

    if( p_aout->b_die ) return;

    /* Find out the current latency */
    for( i = 0; i < FRAMES_NUM; i++ )
    {
        /* Check if frame buf is available */
        if( !(p_aout->output.p_sys->waveheader[i].dwFlags & WHDR_DONE) )
        {
            i_queued_frames++;
        }
    }

    /* Don't wake up the thread too much */
    if( i_queued_frames < FRAMES_NUM / 2 )
        SetEvent( p_aout->output.p_sys->event );
}

/*****************************************************************************
 * WaveOutThread: this thread will capture play notification events.
 *****************************************************************************
 * We use this thread to feed new audio samples to the sound card because
 * we are not authorized to use waveOutWrite() directly in the waveout
 * callback.
 *****************************************************************************/
static void WaveOutThread( notification_thread_t *p_notif )
{
    aout_instance_t *p_aout = p_notif->p_aout;
    aout_sys_t *p_sys = p_aout->output.p_sys;
    aout_buffer_t *p_buffer = NULL;
    WAVEHDR *p_waveheader = p_sys->waveheader;
    int i, i_queued_frames;
    vlc_bool_t b_sleek;

    /* We don't want any resampling when using S/PDIF */
    b_sleek = p_aout->output.output.i_format == VLC_FOURCC('s','p','d','i');

    while( 1 )
    {
        WaitForSingleObject( p_sys->event, INFINITE );

        /* Cleanup and find out the current latency */
        i_queued_frames = 0;
        for( i = 0; i < FRAMES_NUM; i++ )
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

        if( p_aout->b_die ) return;

        /* Try to fill in as many frame buffers as possible */
        for( i = 0; i < FRAMES_NUM; i++ )
        {
            /* Check if frame buf is available */
            if( p_waveheader[i].dwFlags & WHDR_DONE )
            {
                /* Take into account the latency */
                p_buffer = aout_OutputNextBuffer( p_aout,
                    mdate() + 1000000 * i_queued_frames /
                    p_aout->output.output.i_rate * p_aout->output.i_nb_samples,
                    b_sleek );

                if( !p_buffer && i_queued_frames )
                {
                    /* We aren't late so no need to play a blank sample */
                    break;
                }

                /* Do the channel reordering */
                if( p_buffer && p_sys->b_chan_reorder )
                {
                    aout_ChannelReorder( p_buffer->p_buffer,
                        p_buffer->i_nb_bytes,
                        p_sys->waveformat.Format.nChannels,
                        p_sys->pi_chan_table,
                        p_sys->waveformat.Format.wBitsPerSample );
                }

                PlayWaveOut( p_aout, p_sys->h_waveout,
                             &p_waveheader[i], p_buffer );

                i_queued_frames++;
            }
        }
    }
}

static int VolumeInfos( aout_instance_t * p_aout, audio_volume_t * pi_soft )
{
    *pi_soft = AOUT_VOLUME_MAX / 2;
    return 0;
}

static int VolumeGet( aout_instance_t * p_aout, audio_volume_t * pi_volume )
{
    DWORD i_waveout_vol;

#ifdef UNDER_CE
    waveOutGetVolume( 0, &i_waveout_vol );
#else
    waveOutGetVolume( p_aout->output.p_sys->h_waveout, &i_waveout_vol );
#endif

    i_waveout_vol &= 0xFFFF;
    *pi_volume = p_aout->output.i_volume =
        (i_waveout_vol * AOUT_VOLUME_MAX + 0xFFFF /*rounding*/) / 2 / 0xFFFF;
    return 0;
}

static int VolumeSet( aout_instance_t * p_aout, audio_volume_t i_volume )
{
    unsigned long i_waveout_vol = i_volume * 0xFFFF * 2 / AOUT_VOLUME_MAX;
    i_waveout_vol |= (i_waveout_vol << 16);

#ifdef UNDER_CE
    waveOutSetVolume( 0, i_waveout_vol );
#else
    waveOutSetVolume( p_aout->output.p_sys->h_waveout, i_waveout_vol );
#endif

    p_aout->output.i_volume = i_volume;
    return 0;
}
