/*****************************************************************************
 * directx.c: Windows DirectX audio output method
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: directx.c,v 1.12 2003/02/17 22:19:24 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
#include <errno.h>                                                 /* ENOMEM */
#include <fcntl.h>                                       /* open(), O_WRONLY */
#include <string.h>                                            /* strerror() */

#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include "aout_internal.h"

#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>

#define FRAME_SIZE 2048              /* The size is in samples, not in bytes */
#define FRAMES_NUM 4

/* frame buffer status */
#define FRAME_QUEUED 0
#define FRAME_EMPTY 1

/*****************************************************************************
 * DirectSound GUIDs.
 * Defining them here allows us to get rid of the dxguid library during
 * the linking stage.
 *****************************************************************************/
#include <initguid.h>
DEFINE_GUID(IID_IDirectSoundNotify, 0xb0210783, 0x89cd, 0x11d0, 0xaf, 0x8, 0x0, 0xa0, 0xc9, 0x25, 0xcd, 0x16);

/*****************************************************************************
 * Useful macros
 *****************************************************************************/
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

#ifndef KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
DEFINE_GUID( KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, WAVE_FORMAT_IEEE_FLOAT, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
#endif
#ifndef KSDATAFORMAT_SUBTYPE_PCM
DEFINE_GUID( KSDATAFORMAT_SUBTYPE_PCM, WAVE_FORMAT_PCM, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
#endif
#ifndef KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF
DEFINE_GUID( KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF, WAVE_FORMAT_DOLBY_AC3_SPDIF, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
#endif

/*****************************************************************************
 * notification_thread_t: DirectX event thread
 *****************************************************************************/
typedef struct notification_thread_t
{
    VLC_COMMON_MEMBERS

    aout_instance_t * p_aout;
    int i_frame_status[FRAMES_NUM];           /* status of each frame buffer */
    DSBPOSITIONNOTIFY p_events[FRAMES_NUM];      /* play notification events */
    int i_frame_size;                         /* Size in bytes of one frame */

    mtime_t start_date;

} notification_thread_t;

/*****************************************************************************
 * aout_sys_t: directx audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the direct sound specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    HINSTANCE           hdsound_dll;      /* handle of the opened dsound dll */
    LPDIRECTSOUND       p_dsobject;              /* main Direct Sound object */
    LPDIRECTSOUNDBUFFER p_dsbuffer;   /* the sound buffer we use (direct sound
                                       * takes care of mixing all the
                                       * secondary buffers into the primary) */

    LPDIRECTSOUNDNOTIFY p_dsnotify;         /* the position notify interface */
    notification_thread_t *p_notif;                  /* DirectSoundThread id */

    int b_playing;                                         /* playing status */

    int i_frame_size;                         /* Size in bytes of one frame */

    vlc_bool_t b_chan_reorder;              /* do we need channel reordering */
    int *pi_chan_table;
    uint32_t i_channel_mask;
};

static const uint32_t pi_channels_in[] =
    { AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT,
      AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,
      AOUT_CHAN_CENTER, AOUT_CHAN_LFE };
static const uint32_t pi_channels_out[] =
    { SPEAKER_FRONT_LEFT, SPEAKER_FRONT_RIGHT,
      SPEAKER_BACK_LEFT, SPEAKER_BACK_RIGHT,
      SPEAKER_FRONT_CENTER, SPEAKER_LOW_FREQUENCY };
static const uint32_t pi_channels_ordered[] =
    { SPEAKER_FRONT_LEFT, SPEAKER_FRONT_RIGHT, SPEAKER_FRONT_CENTER,
      SPEAKER_LOW_FREQUENCY,
      SPEAKER_BACK_LEFT, SPEAKER_BACK_RIGHT };

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  OpenAudio  ( vlc_object_t * );
static void CloseAudio ( vlc_object_t * );
static void Play       ( aout_instance_t * );

/* local functions */
static void Probe             ( aout_instance_t * );
static int  InitDirectSound   ( aout_instance_t * );
static int  CreateDSBuffer    ( aout_instance_t *, int, int, int, int, int, vlc_bool_t );
static int  CreateDSBufferPCM ( aout_instance_t *, int*, int, int, int, vlc_bool_t );
static void DestroyDSBuffer   ( aout_instance_t * );
static void DirectSoundThread ( notification_thread_t * );
static int  FillBuffer        ( aout_instance_t *, int, aout_buffer_t * );

static void CheckReordering   ( aout_instance_t *, int );
static void InterleaveFloat32 ( float *, int *, int );
static void InterleaveS16     ( int16_t *, int *, int );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("DirectX audio module") );
    set_capability( "audio output", 50 );
    add_shortcut( "directx" );
    set_callbacks( OpenAudio, CloseAudio );
vlc_module_end();

/*****************************************************************************
 * OpenAudio: open the audio device
 *****************************************************************************
 * This function opens and setups Direct Sound.
 *****************************************************************************/
static int OpenAudio( vlc_object_t *p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;
    vlc_value_t val;
    int i;

    msg_Dbg( p_aout, "OpenAudio" );

   /* Allocate structure */
    p_aout->output.p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->output.p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return VLC_EGENERIC;
    }

    /* Initialize some variables */
    p_aout->output.p_sys->p_dsobject = NULL;
    p_aout->output.p_sys->p_dsbuffer = NULL;
    p_aout->output.p_sys->p_dsnotify = NULL;
    p_aout->output.p_sys->p_notif = NULL;
    p_aout->output.p_sys->b_playing = 0;
    p_aout->output.p_sys->pi_chan_table = NULL;

    p_aout->output.pf_play = Play;
    aout_VolumeSoftInit( p_aout );

    /* Initialise DirectSound */
    if( InitDirectSound( p_aout ) )
    {
        msg_Err( p_aout, "cannot initialize DirectSound" );
        goto error;
    }

    /* Now we need to setup our DirectSound play notification structure */
    p_aout->output.p_sys->p_notif =
        vlc_object_create( p_aout, sizeof(notification_thread_t) );
    p_aout->output.p_sys->p_notif->p_aout = p_aout;

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

    /* Then create the notification events */
    for( i = 0; i < FRAMES_NUM; i++ )
        p_aout->output.p_sys->p_notif->p_events[i].hEventNotify =
            CreateEvent( NULL, FALSE, FALSE, NULL );

    /* Open the device */
    if( !strcmp( val.psz_string, N_("A/52 over S/PDIF") ) )
    {
        free( val.psz_string );
        p_aout->output.output.i_format = VLC_FOURCC('s','p','d','i');

        /* Calculate the frame size in bytes */
        p_aout->output.i_nb_samples = A52_FRAME_NB;
        p_aout->output.output.i_bytes_per_frame = AOUT_SPDIF_SIZE;
        p_aout->output.output.i_frame_length = A52_FRAME_NB;
        p_aout->output.p_sys->i_frame_size =
            p_aout->output.output.i_bytes_per_frame;

        if( CreateDSBuffer( p_aout, VLC_FOURCC('s','p','d','i'),
                            p_aout->output.output.i_physical_channels,
                            aout_FormatNbChannels( &p_aout->output.output ),
                            p_aout->output.output.i_rate,
                            p_aout->output.p_sys->i_frame_size, VLC_FALSE )
            != VLC_SUCCESS )
        {
            msg_Err( p_aout, "cannot open waveout audio device" );
            free( p_aout->output.p_sys );
            return VLC_EGENERIC;
        }

        aout_VolumeNoneInit( p_aout );
    }
    else
    {
        if( !strcmp( val.psz_string, N_("5.1") ) )
        {
            p_aout->output.output.i_physical_channels
                = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
                   | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                   | AOUT_CHAN_LFE;
        }
        else if( !strcmp( val.psz_string, N_("2 Front 2 Rear") ) )
        {
            p_aout->output.output.i_physical_channels
                = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                   | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
        }
        else if( !strcmp( val.psz_string, "Mono" ) )
        {
            p_aout->output.output.i_physical_channels = AOUT_CHAN_CENTER;
        }
        else
        {
            p_aout->output.output.i_physical_channels
                = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
        }
        free( val.psz_string );

        if( CreateDSBufferPCM( p_aout, &p_aout->output.output.i_format,
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
        p_aout->output.p_sys->i_frame_size =
            FRAME_SIZE * p_aout->output.output.i_bytes_per_frame;

        aout_VolumeSoftInit( p_aout );
    }

    /* then launch the notification thread */
    msg_Dbg( p_aout, "creating DirectSoundThread" );
    if( vlc_thread_create( p_aout->output.p_sys->p_notif,
                           "DirectSound Notification Thread",
                           DirectSoundThread,
                           VLC_THREAD_PRIORITY_HIGHEST, VLC_FALSE ) )
    {
        msg_Err( p_aout, "cannot create DirectSoundThread" );
        goto error;
    }

    vlc_object_attach( p_aout->output.p_sys->p_notif, p_aout );

    return VLC_SUCCESS;

 error:
    CloseAudio( VLC_OBJECT(p_aout) );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Probe: probe the audio device for available formats and channels
 *****************************************************************************/
static void Probe( aout_instance_t * p_aout )
{
    vlc_value_t val;
    int i_format;
    unsigned int i_physical_channels;
    DWORD ui_speaker_config;

    var_Create( p_aout, "audio-device", VLC_VAR_STRING | VLC_VAR_HASCHOICE );

    /* Test for SPDIF support */
    if ( AOUT_FMT_NON_LINEAR( &p_aout->output.output ) )
    {
        if( CreateDSBuffer( p_aout, VLC_FOURCC('s','p','d','i'),
                            p_aout->output.output.i_physical_channels,
                            aout_FormatNbChannels( &p_aout->output.output ),
                            p_aout->output.output.i_rate,
                            AOUT_SPDIF_SIZE, VLC_TRUE )
            == VLC_SUCCESS )
        {
            msg_Dbg( p_aout, "device supports A/52 over S/PDIF" );
            val.psz_string = N_("A/52 over S/PDIF");
            var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val );
        }
    }

    /* Test for 5.1 support */
    i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                          AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT |
                          AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE;
    if( p_aout->output.output.i_physical_channels == i_physical_channels )
    {
        if( CreateDSBufferPCM( p_aout, &i_format, i_physical_channels, 6,
                               p_aout->output.output.i_rate, VLC_TRUE )
            == VLC_SUCCESS )
        {
            val.psz_string = N_("5.1");
            var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val );
            msg_Dbg( p_aout, "device supports 5.1 channels" );
        }
    }

    /* Test for 2 Front 2 Rear support */
    i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                          AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
    if( ( p_aout->output.output.i_physical_channels & i_physical_channels )
        == i_physical_channels )
    {
        if( CreateDSBufferPCM( p_aout, &i_format, i_physical_channels, 4,
                               p_aout->output.output.i_rate, VLC_TRUE )
            == VLC_SUCCESS )
        {
            val.psz_string = N_("2 Front 2 Rear");
            var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val );
            msg_Dbg( p_aout, "device supports 4 channels" );
        }
    }

    /* Test for stereo support */
    i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    if( CreateDSBufferPCM( p_aout, &i_format, i_physical_channels, 2,
                           p_aout->output.output.i_rate, VLC_TRUE )
        == VLC_SUCCESS )
    {
        val.psz_string = N_("Stereo");
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val );
        msg_Dbg( p_aout, "device supports 2 channels" );
    }

    /* Test for mono support */
    i_physical_channels = AOUT_CHAN_CENTER;
    if( CreateDSBufferPCM( p_aout, &i_format, i_physical_channels, 1,
                           p_aout->output.output.i_rate, VLC_TRUE )
        == VLC_SUCCESS )
    {
        val.psz_string = N_("Mono");
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val );        
        msg_Dbg( p_aout, "device supports 1 channel" );
    }

    /* Check the speaker configuration to determine which channel config should
     * be the default */
    if FAILED( IDirectSound_GetSpeakerConfig( p_aout->output.p_sys->p_dsobject,
                                              &ui_speaker_config ) )
    {
        ui_speaker_config = DSSPEAKER_STEREO;
    }
    switch( DSSPEAKER_CONFIG(ui_speaker_config) )
    {
    case DSSPEAKER_5POINT1:
        val.psz_string = N_("5.1");
        break;
    case DSSPEAKER_QUAD:
        val.psz_string = N_("2 Front 2 Rear");
        break;
    case DSSPEAKER_MONO:
        val.psz_string = N_("Mono");
        break;
    case DSSPEAKER_SURROUND:
    case DSSPEAKER_STEREO:
    default:
        val.psz_string = N_("Stereo");
        break;
    }
    var_Set( p_aout, "audio-device", val );

    var_AddCallback( p_aout, "audio-device", aout_ChannelsRestart, NULL );

    val.b_bool = VLC_TRUE;
    var_Set( p_aout, "intf-change", val );
}

/*****************************************************************************
 * Play: we'll start playing the directsound buffer here because at least here
 *       we know the first buffer has been put in the aout fifo and we also
 *       know its date.
 *****************************************************************************/
static void Play( aout_instance_t *p_aout )
{
    if( !p_aout->output.p_sys->b_playing )
    {
        aout_buffer_t *p_buffer;

        p_aout->output.p_sys->b_playing = 1;

        /* get the playing date of the first aout buffer */
        p_aout->output.p_sys->p_notif->start_date =
            aout_FifoFirstDate( p_aout, &p_aout->output.fifo );

        /* fill in the first samples */
        p_buffer = aout_FifoPop( p_aout, &p_aout->output.fifo );
        FillBuffer( p_aout, 0, p_buffer );

        /* wake up the audio output thread */
        SetEvent( p_aout->output.p_sys->p_notif->p_events[0].hEventNotify );
    }
}

/*****************************************************************************
 * CloseAudio: close the audio device
 *****************************************************************************/
static void CloseAudio( vlc_object_t *p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;

    msg_Dbg( p_aout, "CloseAudio" );

    /* kill the position notification thread, if any */
    if( p_aout->output.p_sys->p_notif )
    {
        vlc_object_detach( p_aout->output.p_sys->p_notif );
        if( p_aout->output.p_sys->p_notif->b_thread )
        {
            p_aout->output.p_sys->p_notif->b_die = 1;

            if( !p_aout->output.p_sys->b_playing )
                /* wake up the audio thread */
                SetEvent(
                    p_aout->output.p_sys->p_notif->p_events[0].hEventNotify );

            vlc_thread_join( p_aout->output.p_sys->p_notif );
        }
        vlc_object_destroy( p_aout->output.p_sys->p_notif );
    }

    /* release the secondary buffer */
    DestroyDSBuffer( p_aout );

    /* finally release the DirectSound object */
    if( p_aout->output.p_sys->p_dsobject )
        IDirectSound_Release( p_aout->output.p_sys->p_dsobject );
    
    /* free DSOUND.DLL */
    if( p_aout->output.p_sys->hdsound_dll )
       FreeLibrary( p_aout->output.p_sys->hdsound_dll );

    if( p_aout->output.p_sys->pi_chan_table )
        free( p_aout->output.p_sys->pi_chan_table );

    free( p_aout->output.p_sys );
}

/*****************************************************************************
 * InitDirectSound: handle all the gory details of DirectSound initialisation
 *****************************************************************************/
static int InitDirectSound( aout_instance_t *p_aout )
{
    HRESULT (WINAPI *OurDirectSoundCreate)(LPGUID, LPDIRECTSOUND *, LPUNKNOWN);

    p_aout->output.p_sys->hdsound_dll = LoadLibrary("DSOUND.DLL");
    if( p_aout->output.p_sys->hdsound_dll == NULL )
    {
        msg_Warn( p_aout, "cannot open DSOUND.DLL" );
        goto error;
    }

    OurDirectSoundCreate = (void *)GetProcAddress(
                                           p_aout->output.p_sys->hdsound_dll,
                                           "DirectSoundCreate" );
    if( OurDirectSoundCreate == NULL )
    {
        msg_Warn( p_aout, "GetProcAddress FAILED" );
        goto error;
    }

    /* Create the direct sound object */
    if FAILED( OurDirectSoundCreate( NULL, &p_aout->output.p_sys->p_dsobject,
                                     NULL ) )
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
    if( IDirectSound_SetCooperativeLevel( p_aout->output.p_sys->p_dsobject,
                                          GetDesktopWindow(),
                                          DSSCL_EXCLUSIVE) )
    {
        msg_Warn( p_aout, "cannot set direct sound cooperative level" );
    }

    return VLC_SUCCESS;

 error:
    p_aout->output.p_sys->p_dsobject = NULL;
    if( p_aout->output.p_sys->hdsound_dll )
    {
        FreeLibrary( p_aout->output.p_sys->hdsound_dll );
        p_aout->output.p_sys->hdsound_dll = NULL;
    }
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
static int CreateDSBuffer( aout_instance_t *p_aout, int i_format,
                           int i_channels, int i_nb_channels, int i_rate,
                           int i_bytes_per_frame, vlc_bool_t b_probe )
{
    WAVEFORMATEXTENSIBLE waveformat;
    DSBUFFERDESC         dsbdesc;
    unsigned int         i;

    /* First set the sound buffer format */
    waveformat.dwChannelMask = 0;
    for( i = 0; i < sizeof(pi_channels_in)/sizeof(uint32_t); i++ )
    {
        if( i_channels & pi_channels_in[i] )
            waveformat.dwChannelMask |= pi_channels_out[i];
    }

    switch( i_format )
    {
    case VLC_FOURCC('s','p','d','i'):
        i_nb_channels = 2;
        waveformat.Format.wBitsPerSample = 16;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_DOLBY_AC3_SPDIF;
        waveformat.SubFormat = KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF;
        break;

    case VLC_FOURCC('f','l','3','2'):
        waveformat.Format.wBitsPerSample = sizeof(float) * 8;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        waveformat.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        break;

    case VLC_FOURCC('s','1','6','l'):
        waveformat.Format.wBitsPerSample = 16;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_PCM;
        waveformat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
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


    /* Then fill in the direct sound descriptor */
    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2/* Better position accuracy */
                    | DSBCAPS_CTRLPOSITIONNOTIFY     /* We need notification */
                    | DSBCAPS_GLOBALFOCUS;      /* Allows background playing */
    dsbdesc.dwBufferBytes = FRAMES_NUM * i_bytes_per_frame;   /* buffer size */
    dsbdesc.lpwfxFormat = (WAVEFORMATEX *)&waveformat;

    if FAILED( IDirectSound_CreateSoundBuffer(
                   p_aout->output.p_sys->p_dsobject, &dsbdesc,
                   &p_aout->output.p_sys->p_dsbuffer, NULL) )
    {
        goto error;
    }

    /* Stop here if we were just probing */
    if( b_probe )
    {
        IDirectSoundBuffer_Release( p_aout->output.p_sys->p_dsbuffer );
        p_aout->output.p_sys->p_dsbuffer = NULL;
        return VLC_SUCCESS;
    }

    /* Backup the size of a frame */
    p_aout->output.p_sys->p_notif->i_frame_size = i_bytes_per_frame;

    /* Now the secondary buffer is created, we need to setup its position
     * notification */
    for( i = 0; i < FRAMES_NUM; i++ )
    {
        p_aout->output.p_sys->p_notif->p_events[i].dwOffset = i *
            p_aout->output.p_sys->p_notif->i_frame_size;

        p_aout->output.p_sys->p_notif->i_frame_status[i] = FRAME_EMPTY;
    }

    /* Get the IDirectSoundNotify interface */
    if FAILED( IDirectSoundBuffer_QueryInterface(
                                p_aout->output.p_sys->p_dsbuffer,
                                &IID_IDirectSoundNotify,
                                (LPVOID *)&p_aout->output.p_sys->p_dsnotify ) )
    {
        msg_Err( p_aout, "cannot get IDirectSoundNotify interface" );
        goto error;
    }

    if FAILED( IDirectSoundNotify_SetNotificationPositions(
                                    p_aout->output.p_sys->p_dsnotify,
                                    FRAMES_NUM,
                                    p_aout->output.p_sys->p_notif->p_events ) )
    {
        msg_Err( p_aout, "cannot set position notification" );
        goto error;
    }

    p_aout->output.p_sys->i_channel_mask = waveformat.dwChannelMask;
    CheckReordering( p_aout, i_nb_channels );

    return VLC_SUCCESS;

 error:
    if( p_aout->output.p_sys->p_dsbuffer )
    {
        IDirectSoundBuffer_Release( p_aout->output.p_sys->p_dsbuffer );
        p_aout->output.p_sys->p_dsbuffer = NULL;
    }
    if( p_aout->output.p_sys->p_dsnotify )
    {
        IDirectSoundBuffer_Release( p_aout->output.p_sys->p_dsbuffer );
        p_aout->output.p_sys->p_dsnotify = NULL;
    }
    return VLC_EGENERIC;
}

/*****************************************************************************
 * CreateDSBufferPCM: creates a PCM direct sound buffer.
 *****************************************************************************
 * We first try to create a WAVE_FORMAT_IEEE_FLOAT buffer if supported by
 * the hardware, otherwise we create a WAVE_FORMAT_PCM buffer.
 ****************************************************************************/
static int CreateDSBufferPCM( aout_instance_t *p_aout, int *i_format,
                              int i_channels, int i_nb_channels, int i_rate,
                              vlc_bool_t b_probe )
{
    if( CreateDSBuffer( p_aout, VLC_FOURCC('f','l','3','2'),
                        i_channels, i_nb_channels, i_rate,
                        FRAME_SIZE * 4 * i_nb_channels, b_probe )
        != VLC_SUCCESS )
    {
        if ( CreateDSBuffer( p_aout, VLC_FOURCC('s','1','6','l'),
                             i_channels, i_nb_channels, i_rate,
                             FRAME_SIZE * 2 * i_nb_channels, b_probe )
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
 * DestroyDSBuffer
 *****************************************************************************
 * This function destroys the secondary buffer.
 *****************************************************************************/
static void DestroyDSBuffer( aout_instance_t *p_aout )
{
    if( p_aout->output.p_sys->p_dsnotify )
    {
        IDirectSoundNotify_Release( p_aout->output.p_sys->p_dsnotify );
        p_aout->output.p_sys->p_dsnotify = NULL;
    }

    if( p_aout->output.p_sys->p_dsbuffer )
    {
        IDirectSoundBuffer_Release( p_aout->output.p_sys->p_dsbuffer );
        p_aout->output.p_sys->p_dsbuffer = NULL;
    }
}

/*****************************************************************************
 * FillBuffer: Fill in one of the direct sound frame buffers.
 *****************************************************************************
 * Returns VLC_SUCCESS on success.
 *****************************************************************************/
static int FillBuffer( aout_instance_t *p_aout, int i_frame,
                       aout_buffer_t *p_buffer )
{
    notification_thread_t *p_notif = p_aout->output.p_sys->p_notif;
    void *p_write_position, *p_wrap_around;
    long l_bytes1, l_bytes2;
    HRESULT dsresult;

    /* Before copying anything, we have to lock the buffer */
    dsresult = IDirectSoundBuffer_Lock(
                p_aout->output.p_sys->p_dsbuffer,               /* DS buffer */
                i_frame * p_notif->i_frame_size,             /* Start offset */
                p_notif->i_frame_size,                    /* Number of bytes */
                &p_write_position,                  /* Address of lock start */
                &l_bytes1,       /* Count of bytes locked before wrap around */
                &p_wrap_around,            /* Buffer adress (if wrap around) */
                &l_bytes2,               /* Count of bytes after wrap around */
                0 );                                                /* Flags */
    if( dsresult == DSERR_BUFFERLOST )
    {
        IDirectSoundBuffer_Restore( p_aout->output.p_sys->p_dsbuffer );
        dsresult = IDirectSoundBuffer_Lock(
                               p_aout->output.p_sys->p_dsbuffer,
                               i_frame * p_notif->i_frame_size,
                               p_notif->i_frame_size,
                               &p_write_position,
                               &l_bytes1,
                               &p_wrap_around,
                               &l_bytes2,
                               0 );
    }
    if( dsresult != DS_OK )
    {
        msg_Warn( p_notif, "cannot lock buffer" );
        if( p_buffer ) aout_BufferFree( p_buffer );
        return VLC_EGENERIC;
    }

    if ( p_buffer != NULL )
    {
        p_aout->p_vlc->pf_memcpy( p_write_position, p_buffer->p_buffer,
                                  l_bytes1 );
        aout_BufferFree( p_buffer );
    }
    else
        memset( p_write_position, 0, l_bytes1 );

    /* Now the data has been copied, unlock the buffer */
    IDirectSoundBuffer_Unlock( p_aout->output.p_sys->p_dsbuffer,
                               p_write_position, l_bytes1,
                               p_wrap_around, l_bytes2 );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DirectSoundThread: this thread will capture play notification events. 
 *****************************************************************************
 * We use this thread to emulate a callback mechanism. The thread probes for
 * event notification and fills up the DS secondary buffer when needed.
 *****************************************************************************/
static void DirectSoundThread( notification_thread_t *p_notif )
{
    HANDLE  notification_events[FRAMES_NUM];
    HRESULT dsresult;
    aout_instance_t *p_aout = p_notif->p_aout;
    int i, i_which_frame, i_last_frame, i_next_frame;
    mtime_t mtime;
    vlc_bool_t b_sleek;

    for( i = 0; i < FRAMES_NUM; i++ )
        notification_events[i] = p_notif->p_events[i].hEventNotify;

    /* We don't want any resampling when using S/PDIF output */
    b_sleek = p_aout->output.output.i_format == VLC_FOURCC('s','p','d','i');

    /* Tell the main thread that we are ready */
    vlc_thread_ready( p_notif );

    msg_Dbg( p_notif, "DirectSoundThread ready" );

    /* Wait here until Play() is called */
    WaitForSingleObject( notification_events[0], INFINITE );

    if( !p_notif->b_die )
    {
        mwait( p_notif->start_date - AOUT_PTS_TOLERANCE / 2 );

        /* start playing the buffer */
        dsresult = IDirectSoundBuffer_Play( p_aout->output.p_sys->p_dsbuffer,
                                        0,                         /* Unused */
                                        0,                         /* Unused */
                                        DSBPLAY_LOOPING );          /* Flags */
        if( dsresult == DSERR_BUFFERLOST )
        {
            IDirectSoundBuffer_Restore( p_aout->output.p_sys->p_dsbuffer );
            dsresult = IDirectSoundBuffer_Play(
                                            p_aout->output.p_sys->p_dsbuffer,
                                            0,                     /* Unused */
                                            0,                     /* Unused */
                                            DSBPLAY_LOOPING );      /* Flags */
        }
        if( dsresult != DS_OK )
        {
            msg_Err( p_aout, "cannot start playing buffer" );
        }
    }

    while( !p_notif->b_die )
    {
        aout_buffer_t *p_buffer;
        long l_latency;

        /* wait for the position notification */
        i_which_frame = WaitForMultipleObjects( FRAMES_NUM,
                                                notification_events, 0,
                                                INFINITE ) - WAIT_OBJECT_0;

        if( p_notif->b_die )
            break;

        mtime = mdate();

        /* We take into account the current latency */
        if SUCCEEDED( IDirectSoundBuffer_GetCurrentPosition(
                        p_aout->output.p_sys->p_dsbuffer,
                        &l_latency, NULL ) )
        {
            if( l_latency > (i_which_frame * FRAME_SIZE)
                  && l_latency < ((i_which_frame+1) * FRAME_SIZE) )
            {
                l_latency = - ( l_latency /
                                p_aout->output.output.i_bytes_per_frame %
                                FRAME_SIZE );
            }
            else
            {
                l_latency = FRAME_SIZE - ( l_latency /
                                      p_aout->output.output.i_bytes_per_frame %
                                      FRAME_SIZE );
            }
        }
        else
        {
            l_latency = 0;
        }

        /* Mark last frame as empty */
        i_last_frame = (i_which_frame + FRAMES_NUM -1) % FRAMES_NUM;
        i_next_frame = (i_which_frame + 1) % FRAMES_NUM;
        p_notif->i_frame_status[i_last_frame] = FRAME_EMPTY;

        /* Try to fill in as many frame buffers as possible */
        for( i = i_next_frame; (i % FRAMES_NUM) != i_which_frame; i++ )
        {

            /* Check if frame buf is already filled */
            if( p_notif->i_frame_status[i % FRAMES_NUM] == FRAME_QUEUED )
                continue;

            p_buffer = aout_OutputNextBuffer( p_aout,
                mtime + 1000000 / p_aout->output.output.i_rate *
                ((i - i_next_frame + 1) * FRAME_SIZE + l_latency), b_sleek );

            /* If there is no audio data available and we have some buffered
             * already, then just wait for the next time */
            if( !p_buffer && (i != i_next_frame) )
            {
                //msg_Err( p_aout, "only %i frame buffers filled!",
                //         i - i_next_frame );
                break;
            }

            if( FillBuffer( p_aout, (i%FRAMES_NUM), p_buffer )
                != VLC_SUCCESS )
                break;

            /* Mark the frame buffer as QUEUED */
            p_notif->i_frame_status[i%FRAMES_NUM] = FRAME_QUEUED;
        }

    }

    /* make sure the buffer isn't playing */
    IDirectSoundBuffer_Stop( p_aout->output.p_sys->p_dsbuffer );

    /* free the events */
    for( i = 0; i < FRAMES_NUM; i++ )
        CloseHandle( notification_events[i] );

    msg_Dbg( p_notif, "DirectSoundThread exiting" );
}

/*****************************************************************************
 * CheckReordering: Check if we need to do some channel re-ordering (the ac3
 *                  channel order is different from the one chosen by
 *                  Microsoft).
 *****************************************************************************/
static void CheckReordering( aout_instance_t *p_aout, int i_nb_channels )
{
    int i, j, k, l;

#define i_channel_mask p_aout->output.p_sys->i_channel_mask
#define pi_chan_table p_aout->output.p_sys->pi_chan_table

    p_aout->output.p_sys->b_chan_reorder = VLC_FALSE;

    pi_chan_table = malloc( i_nb_channels * sizeof(int) );
    if( !pi_chan_table )
    {
        return;
    }

    for( i = 0, j = 0;
         i < (int)(sizeof(pi_channels_out)/sizeof(uint32_t)); i++ )
    {
        if( i_channel_mask & pi_channels_out[i] )
        {
            for( k = 0, l = 0;
                 pi_channels_out[i] != pi_channels_ordered[k]; k++ )
            {
                if( i_channel_mask & pi_channels_ordered[k] )
                {
                    l++;
                }
            }

            pi_chan_table[j] = l;

            j++;
        }
    }

    for( i = 0; i < i_nb_channels; i++ )
    {
        if( pi_chan_table[i] != i )
        {
            p_aout->output.p_sys->b_chan_reorder = VLC_TRUE;
        }
    }

    if( p_aout->output.p_sys->b_chan_reorder )
    {
        msg_Dbg( p_aout, "channel reordering needed" );
    }

#undef pi_chan_table
#undef waveformat
}

/*****************************************************************************
 * InterleaveFloat32/S16: change the channel order to the Microsoft one.
 *****************************************************************************/
static void InterleaveFloat32( float *p_buf, int *pi_chan_table,
                               int i_nb_channels )
{
    int i, j;
    float p_tmp[10];

    for( i = 0; i < FRAME_SIZE; i++ )
    {
        for( j = 0; j < i_nb_channels; j++ )
        {
            p_tmp[pi_chan_table[j]] = p_buf[i*i_nb_channels + j];
        }

        memcpy( &p_buf[i*i_nb_channels], p_tmp,
                i_nb_channels * sizeof(float) );
    }
}

static void InterleaveS16( int16_t *p_buf, int *pi_chan_table,
                           int i_nb_channels )
{
    int i, j;
    int16_t p_tmp[10];

    for( i = 0; i < FRAME_SIZE; i++ )
    {
        for( j = 0; j < i_nb_channels; j++ )
        {
            p_tmp[pi_chan_table[j]] = p_buf[i*i_nb_channels + j];
        }

        memcpy( &p_buf[i*i_nb_channels], p_tmp,
                i_nb_channels * sizeof(int16_t) );
    }
}
