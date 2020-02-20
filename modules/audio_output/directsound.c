/*****************************************************************************
 * directsound.c: DirectSound audio output plugin for VLC
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
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

#include <assert.h>
#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_charset.h>

#include "audio_output/windows_audio_common.h"
#include "audio_output/mmdevice.h"
#include <mmdeviceapi.h>

#define DS_BUF_SIZE (6*1024*1024)

static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );
static HRESULT StreamStart( aout_stream_t *, audio_sample_format_t *,
                            const GUID * );
static void * PlayedDataEraser( void * );
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
    add_shortcut( "directx", "aout_directx", "directsound", "dsound" )

    add_string( "directx-audio-device", NULL,
             DEVICE_TEXT, DEVICE_LONGTEXT, false )
    add_obsolete_string( "directx-audio-device-name")
    add_bool( "directx-audio-float32", true, FLOAT_TEXT,
              FLOAT_LONGTEXT, true )
    add_string( "directx-audio-speaker", "Windows default",
                 SPEAKER_TEXT, SPEAKER_LONGTEXT, true )
        change_string_list( speaker_list, speaker_list )
    add_float( "directx-volume", 1.0f,
                 VOLUME_TEXT, VOLUME_LONGTEXT, true )
        change_float_range( 0.f, 2.f )

    set_callbacks( Open, Close )

    add_submodule()
        set_capability( "aout stream", 30 )
        set_callback( StreamStart )
vlc_module_end ()

typedef struct aout_stream_sys
{
    LPDIRECTSOUND       p_dsobject; /*< main Direct Sound object */
    LPDIRECTSOUNDBUFFER p_dsbuffer; /*< the sound buffer we use (direct sound
                                        takes care of mixing all the secondary
                                        buffers into the primary) */
    LPDIRECTSOUNDNOTIFY p_notify;

    int      i_bytes_per_sample;    /*< Size in bytes of one frame */
    int      i_rate;                /*< Sample rate */

    uint8_t  chans_to_reorder;      /*< Do we need channel reordering? */
    uint8_t  chan_table[AOUT_CHAN_MAX];
    uint32_t i_channel_mask;
    vlc_fourcc_t format;

    size_t  i_write;
    size_t  i_last_read;
    int64_t i_data;

    bool b_playing;
    vlc_mutex_t lock;
    vlc_cond_t cond;
    vlc_thread_t eraser_thread;
} aout_stream_sys_t;

/**
 * DirectSound audio output method descriptor
 *
 * This structure is part of the audio output thread descriptor.
 * It describes the direct sound specific properties of an audio device.
 */
typedef struct
{
    aout_stream_sys_t s;
    struct
    {
        float         volume;
        LONG          mb;
        bool          mute;
    } volume;
} aout_sys_t;

static HRESULT Flush( aout_stream_sys_t *sys );
static HRESULT TimeGet( aout_stream_sys_t *sys, vlc_tick_t *delay )
{
    DWORD read, status;
    HRESULT hr;
    ssize_t size;

    hr = IDirectSoundBuffer_GetStatus( sys->p_dsbuffer, &status );
    if( hr != DS_OK )
        return hr;
    if( !(status & DSBSTATUS_PLAYING) )
        return DSERR_INVALIDCALL ;

    hr = IDirectSoundBuffer_GetCurrentPosition( sys->p_dsbuffer, &read, NULL );
    if( hr != DS_OK )
        return hr;

    size = (ssize_t)read - sys->i_last_read;

    /* GetCurrentPosition cannot be trusted if the return doesn't change
     * Just return an error */
    if( size ==  0 )
        return DSERR_GENERIC ;
    else if( size < 0 )
      size += DS_BUF_SIZE;

    sys->i_data -= size;
    sys->i_last_read = read;

    if( sys->i_data < 0 )
        /* underrun */
        Flush(sys);

    *delay = vlc_tick_from_samples( sys->i_data / sys->i_bytes_per_sample, sys->i_rate );

    return DS_OK;
}

static HRESULT StreamTimeGet( aout_stream_t *s, vlc_tick_t *delay )
{
    return TimeGet( s->sys, delay );
}

static int OutputTimeGet( audio_output_t *aout, vlc_tick_t *delay )
{
    aout_sys_t *sys = aout->sys;
    return (TimeGet( &sys->s, delay ) == DS_OK) ? 0 : -1;
}

/**
 * Fills in one of the DirectSound frame buffers.
 *
 * @return VLC_SUCCESS on success.
 */
static HRESULT FillBuffer( vlc_object_t *obj, aout_stream_sys_t *p_sys,
                           block_t *p_buffer )
{
    size_t towrite = (p_buffer != NULL) ? p_buffer->i_buffer : DS_BUF_SIZE;
    void *p_write_position, *p_wrap_around;
    unsigned long l_bytes1, l_bytes2;
    HRESULT dsresult;

    vlc_mutex_lock( &p_sys->lock );

    /* Before copying anything, we have to lock the buffer */
    dsresult = IDirectSoundBuffer_Lock(
           p_sys->p_dsbuffer,    /* DS buffer */
           p_sys->i_write,       /* Start offset */
           towrite,                /* Number of bytes */
           &p_write_position,    /* Address of lock start */
           &l_bytes1,            /* Count of bytes locked before wrap around */
           &p_wrap_around,       /* Buffer address (if wrap around) */
           &l_bytes2,            /* Count of bytes after wrap around */
           0 );                  /* Flags: DSBLOCK_FROMWRITECURSOR is buggy */
    if( dsresult == DSERR_BUFFERLOST )
    {
        IDirectSoundBuffer_Restore( p_sys->p_dsbuffer );
        dsresult = IDirectSoundBuffer_Lock(
                               p_sys->p_dsbuffer,
                               p_sys->i_write,
                               towrite,
                               &p_write_position,
                               &l_bytes1,
                               &p_wrap_around,
                               &l_bytes2,
                               0 );
    }
    if( dsresult != DS_OK )
    {
        msg_Warn( obj, "cannot lock buffer" );
        if( p_buffer != NULL )
            block_Release( p_buffer );
        vlc_mutex_unlock( &p_sys->lock );
        return dsresult;
    }

    if( p_buffer == NULL )
    {
        memset( p_write_position, 0, l_bytes1 );
        memset( p_wrap_around, 0, l_bytes2 );
    }
    else
    {
        if( p_sys->chans_to_reorder ) /* Do the channel reordering here */
            aout_ChannelReorder( p_buffer->p_buffer, p_buffer->i_buffer,
                                 p_sys->chans_to_reorder, p_sys->chan_table,
                                 p_sys->format );

        memcpy( p_write_position, p_buffer->p_buffer, l_bytes1 );
        if( p_wrap_around && l_bytes2 )
            memcpy( p_wrap_around, p_buffer->p_buffer + l_bytes1, l_bytes2 );

        if( unlikely( ( l_bytes1 + l_bytes2 ) < p_buffer->i_buffer ) )
            msg_Err( obj, "Buffer overrun");

        block_Release( p_buffer );
    }

    /* Now the data has been copied, unlock the buffer */
    IDirectSoundBuffer_Unlock( p_sys->p_dsbuffer, p_write_position, l_bytes1,
                               p_wrap_around, l_bytes2 );

    p_sys->i_write += towrite;
    p_sys->i_write %= DS_BUF_SIZE;
    p_sys->i_data += towrite;
    vlc_mutex_unlock( &p_sys->lock );

    return DS_OK;
}

static HRESULT Play( vlc_object_t *obj, aout_stream_sys_t *sys,
                     block_t *p_buffer )
{
    HRESULT dsresult;
    dsresult = FillBuffer( obj, sys, p_buffer );
    if( dsresult != DS_OK )
        return dsresult;

    /* start playing the buffer */
    dsresult = IDirectSoundBuffer_Play( sys->p_dsbuffer, 0, 0,
                                        DSBPLAY_LOOPING );
    if( dsresult == DSERR_BUFFERLOST )
    {
        IDirectSoundBuffer_Restore( sys->p_dsbuffer );
        dsresult = IDirectSoundBuffer_Play( sys->p_dsbuffer,
                                            0, 0, DSBPLAY_LOOPING );
    }
    if( dsresult != DS_OK )
        msg_Err( obj, "cannot start playing buffer: (hr=0x%lX)", dsresult );
    else
    {
        vlc_mutex_lock( &sys->lock );
        sys->b_playing = true;
        vlc_cond_signal(&sys->cond);
        vlc_mutex_unlock( &sys->lock );

    }
    return dsresult;
}

static HRESULT StreamPlay( aout_stream_t *s, block_t *block, vlc_tick_t date )
{
    (void) date;
    return Play( VLC_OBJECT(s), s->sys, block );
}

static void OutputPlay( audio_output_t *aout, block_t *block, vlc_tick_t date )
{
    aout_sys_t *sys = aout->sys;
    Play( VLC_OBJECT(aout), &sys->s, block );
    (void) date;
}

static HRESULT Pause( aout_stream_sys_t *sys, bool pause )
{
    HRESULT hr;

    if( pause )
        hr = IDirectSoundBuffer_Stop( sys->p_dsbuffer );
    else
        hr = IDirectSoundBuffer_Play( sys->p_dsbuffer, 0, 0, DSBPLAY_LOOPING );
    if( hr == DS_OK )
    {
        vlc_mutex_lock( &sys->lock );
        sys->b_playing = !pause;
        if( sys->b_playing )
            vlc_cond_signal( &sys->cond );
        vlc_mutex_unlock( &sys->lock );
    }
    return hr;
}

static HRESULT StreamPause( aout_stream_t *s, bool pause )
{
    return Pause( s->sys, pause );
}

static void OutputPause( audio_output_t *aout, bool pause, vlc_tick_t date )
{
    aout_sys_t *sys = aout->sys;
    Pause( &sys->s, pause );
    (void) date;
}

static HRESULT Flush( aout_stream_sys_t *sys )
{
    HRESULT ret = IDirectSoundBuffer_Stop( sys->p_dsbuffer );
    if( ret == DS_OK )
    {
        vlc_mutex_lock(&sys->lock);
        sys->i_data = 0;
        sys->i_last_read = sys->i_write;
        IDirectSoundBuffer_SetCurrentPosition( sys->p_dsbuffer, sys->i_write);
        sys->b_playing = false;
        vlc_mutex_unlock(&sys->lock);
    }
    return ret;
}

static HRESULT StreamFlush( aout_stream_t *s )
{
    return Flush( s->sys );
}

static void OutputFlush( audio_output_t *aout )
{
    aout_sys_t *sys = aout->sys;
    Flush( &sys->s );
}

/**
 * Creates a DirectSound buffer of the required format.
 *
 * This function creates the buffer we'll use to play audio.
 * In DirectSound there are two kinds of buffers:
 * - the primary buffer: which is the actual buffer that the soundcard plays
 * - the secondary buffer(s): these buffers are the one actually used by
 *    applications and DirectSound takes care of mixing them into the primary.
 *
 * Once you create a secondary buffer, you cannot change its format anymore so
 * you have to release the current one and create another.
 */
static HRESULT CreateDSBuffer( vlc_object_t *obj, aout_stream_sys_t *sys,
                               int i_format, int i_channels, int i_nb_channels,
                               int i_rate, bool b_probe )
{
    WAVEFORMATEXTENSIBLE waveformat;
    DSBUFFERDESC         dsbdesc;
    HRESULT              hr;

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

    sys->i_bytes_per_sample = waveformat.Format.nBlockAlign;
    sys->format = i_format;

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

    /* CreateSoundBuffer doesn't allow volume control for non-PCM buffers */
    if ( i_format == VLC_CODEC_SPDIFL )
        dsbdesc.dwFlags &= ~DSBCAPS_CTRLVOLUME;

    hr = IDirectSound_CreateSoundBuffer( sys->p_dsobject, &dsbdesc,
                                         &sys->p_dsbuffer, NULL );
    if( FAILED(hr) )
    {
        if( !(dsbdesc.dwFlags & DSBCAPS_LOCHARDWARE) )
            return hr;

        /* Try without DSBCAPS_LOCHARDWARE */
        dsbdesc.dwFlags &= ~DSBCAPS_LOCHARDWARE;
        hr = IDirectSound_CreateSoundBuffer( sys->p_dsobject, &dsbdesc,
                                             &sys->p_dsbuffer, NULL );
        if( FAILED(hr) )
            return hr;
        if( !b_probe )
            msg_Dbg( obj, "couldn't use hardware sound buffer" );
    }

    /* Stop here if we were just probing */
    if( b_probe )
    {
        IDirectSoundBuffer_Release( sys->p_dsbuffer );
        sys->p_dsbuffer = NULL;
        return DS_OK;
    }

    sys->i_rate = i_rate;
    sys->i_channel_mask = waveformat.dwChannelMask;
    sys->chans_to_reorder =
        aout_CheckChannelReorder( pi_channels_in, pi_channels_out,
                                  waveformat.dwChannelMask, sys->chan_table );
    if( sys->chans_to_reorder )
        msg_Dbg( obj, "channel reordering needed" );

    hr = IDirectSoundBuffer_QueryInterface( sys->p_dsbuffer,
                                            &IID_IDirectSoundNotify,
                                            (void **) &sys->p_notify );
    if( hr != DS_OK )
    {
        msg_Err( obj, "Couldn't query IDirectSoundNotify" );
        sys->p_notify = NULL;
    }

    FillBuffer( obj, sys, NULL );
    return DS_OK;
}

/**
 * Creates a PCM DirectSound buffer.
 *
 * We first try to create a WAVE_FORMAT_IEEE_FLOAT buffer if supported by
 * the hardware, otherwise we create a WAVE_FORMAT_PCM buffer.
 */
static HRESULT CreateDSBufferPCM( vlc_object_t *obj, aout_stream_sys_t *sys,
                                  vlc_fourcc_t *i_format, int i_channels,
                                  int i_rate, bool b_probe )
{
    HRESULT hr;
    unsigned i_nb_channels = vlc_popcount( i_channels );

    if( var_GetBool( obj, "directx-audio-float32" ) )
    {
        hr = CreateDSBuffer( obj, sys, VLC_CODEC_FL32, i_channels,
                             i_nb_channels, i_rate, b_probe );
        if( hr == DS_OK )
        {
            *i_format = VLC_CODEC_FL32;
            return DS_OK;
        }
    }

    hr = CreateDSBuffer( obj, sys, VLC_CODEC_S16N, i_channels, i_nb_channels,
                         i_rate, b_probe );
    if( hr == DS_OK )
    {
        *i_format = VLC_CODEC_S16N;
        return DS_OK;
    }

    return hr;
}

/**
 * Closes the audio device.
 */
static HRESULT Stop( aout_stream_sys_t *p_sys )
{
    vlc_mutex_lock( &p_sys->lock );
    p_sys->b_playing =  true;
    vlc_cond_signal( &p_sys->cond );
    vlc_mutex_unlock( &p_sys->lock );
    vlc_cancel( p_sys->eraser_thread );
    vlc_join( p_sys->eraser_thread, NULL );

    if( p_sys->p_notify != NULL )
    {
        IDirectSoundNotify_Release(p_sys->p_notify );
        p_sys->p_notify = NULL;
    }
    if( p_sys->p_dsbuffer != NULL )
    {
        IDirectSoundBuffer_Stop( p_sys->p_dsbuffer );
        IDirectSoundBuffer_Release( p_sys->p_dsbuffer );
        p_sys->p_dsbuffer = NULL;
    }
    if( p_sys->p_dsobject != NULL )
    {
        IDirectSound_Release( p_sys->p_dsobject );
        p_sys->p_dsobject = NULL;
    }
    return DS_OK;
}

static void StreamStop( aout_stream_t *s )
{
    Stop( s->sys );
    free( s->sys );
}

static void OutputStop( audio_output_t *aout )
{
    msg_Dbg( aout, "closing audio device" );
    aout_sys_t *sys = aout->sys;
    Stop( &sys->s );
}

static HRESULT Start( vlc_object_t *obj, aout_stream_sys_t *sys,
                      audio_sample_format_t *restrict pfmt )
{
    if( aout_FormatNbChannels( pfmt ) == 0 )
        return E_FAIL;

#if !VLC_WINSTORE_APP
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
    if( IDirectSound_SetCooperativeLevel( sys->p_dsobject, GetDesktopWindow(),
                                          DSSCL_EXCLUSIVE) )
        msg_Warn( obj, "cannot set direct sound cooperative level" );
#endif

    if( AOUT_FMT_HDMI( pfmt ) )
        return E_FAIL;

    audio_sample_format_t fmt = *pfmt;
    const char *const *ppsz_compare = speaker_list;
    char *psz_speaker;
    int i = 0;
    HRESULT hr = DSERR_UNSUPPORTED;

    /* Retrieve config values */
    var_Create( obj, "directx-audio-float32",
                VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    psz_speaker = var_CreateGetString( obj, "directx-audio-speaker" );

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
        msg_Err( obj, "(%s) isn't valid speaker setup option", psz_speaker );
        msg_Err( obj, "Defaulting to Windows default speaker config");
        i = 0;
    }
    free( psz_speaker );

    vlc_mutex_init(&sys->lock);
    vlc_cond_init(&sys->cond);

    if( AOUT_FMT_SPDIF( &fmt ) )
    {
        if( var_InheritBool( obj, "spdif" ) )
            hr = CreateDSBuffer( obj, sys, VLC_CODEC_SPDIFL,
                                 fmt.i_physical_channels,
                                 aout_FormatNbChannels(&fmt), fmt.i_rate, false );

        if( hr == DS_OK )
        {
            msg_Dbg( obj, "using A/52 pass-through over S/PDIF" );
            fmt.i_format = VLC_CODEC_SPDIFL;

            /* Calculate the frame size in bytes */
            fmt.i_bytes_per_frame = AOUT_SPDIF_SIZE;
            fmt.i_frame_length = A52_FRAME_NB;
        }
        else
            return E_FAIL;
    }

    if( hr != DS_OK )
    {
        if( i == 0 )
        {
            DWORD ui_speaker_config;
            int i_channels = 2; /* Default to stereo */
            int i_orig_channels = aout_FormatNbChannels( &fmt );

            /* Check the speaker configuration to determine which channel
             * config should be the default */
            hr = IDirectSound_GetSpeakerConfig( sys->p_dsobject,
                                                &ui_speaker_config );
            if( FAILED(hr) )
            {
                ui_speaker_config = DSSPEAKER_STEREO;
                msg_Dbg( obj, "GetSpeakerConfig failed" );
            }

            const char *name = "Unknown";
            switch( DSSPEAKER_CONFIG(ui_speaker_config) )
            {
                case DSSPEAKER_7POINT1:
                case DSSPEAKER_7POINT1_SURROUND:
                    name = "7.1";
                    i_channels = 8;
                    break;
                case DSSPEAKER_5POINT1:
                case DSSPEAKER_5POINT1_SURROUND:
                    name = "5.1";
                    i_channels = 6;
                    break;
                case DSSPEAKER_QUAD:
                    name = "Quad";
                    i_channels = 4;
                    break;
#if 0 /* Lots of people just get their settings wrong and complain that
       * this is a problem with VLC so just don't ever set mono by default. */
                case DSSPEAKER_MONO:
                    name = "Mono";
                    i_channels = 1;
                    break;
#endif
                case DSSPEAKER_SURROUND:
                    name = "Surround";
                    i_channels = 4;
                    break;
                case DSSPEAKER_STEREO:
                    name = "Stereo";
                    i_channels = 2;
                    break;
            }

            if( i_channels >= i_orig_channels )
                i_channels = i_orig_channels;

            msg_Dbg( obj, "%s speaker config: %s and stream has "
                     "%d channels, using %d channels", "Windows", name,
                     i_orig_channels, i_channels );

            switch( i_channels )
            {
                case 8:
                    fmt.i_physical_channels = AOUT_CHANS_7_1;
                    break;
                case 7:
                case 6:
                    fmt.i_physical_channels = AOUT_CHANS_5_1;
                    break;
                case 5:
                case 4:
                    fmt.i_physical_channels = AOUT_CHANS_4_0;
                    break;
                default:
                    fmt.i_physical_channels = AOUT_CHANS_2_0;
                    break;
            }
        }
        else
        {   /* Overriden speaker configuration */
            const char *name = "Non-existant";
            switch( i )
            {
                case 1: /* Mono */
                    name = "Mono";
                    fmt.i_physical_channels = AOUT_CHAN_CENTER;
                    break;
                case 2: /* Stereo */
                    name = "Stereo";
                    fmt.i_physical_channels = AOUT_CHANS_2_0;
                    break;
                case 3: /* Quad */
                    name = "Quad";
                    fmt.i_physical_channels = AOUT_CHANS_4_0;
                    break;
                case 4: /* 5.1 */
                    name = "5.1";
                    fmt.i_physical_channels = AOUT_CHANS_5_1;
                    break;
                case 5: /* 7.1 */
                    name = "7.1";
                    fmt.i_physical_channels = AOUT_CHANS_7_1;
                    break;
            }
            msg_Dbg( obj, "%s speaker config: %s", "VLC", name );
        }

        /* Open the device */
        aout_FormatPrepare( &fmt );

        hr = CreateDSBufferPCM( obj, sys, &fmt.i_format,
                                fmt.i_physical_channels, fmt.i_rate, false );
        if( hr != DS_OK )
        {
            msg_Err( obj, "cannot open directx audio device" );
            goto error;
        }
    }

    int ret = vlc_clone(&sys->eraser_thread, PlayedDataEraser, (void*) obj,
                        VLC_THREAD_PRIORITY_LOW);
    if( unlikely( ret ) )
    {
        if( ret != ENOMEM )
            msg_Err( obj, "Couldn't start eraser thread" );

        hr = E_FAIL;
        goto error;
    }

    fmt.channel_type = AUDIO_CHANNEL_TYPE_BITMAP;

    *pfmt = fmt;
    sys->b_playing = false;
    sys->i_write = 0;
    sys->i_last_read =  0;
    sys->i_data = 0;

    return DS_OK;

error:
    if( sys->p_notify != NULL )
    {
        IDirectSoundNotify_Release( sys->p_notify );
        sys->p_notify = NULL;
    }
    if( sys->p_dsbuffer != NULL )
    {
        IDirectSoundBuffer_Release( sys->p_dsbuffer );
        sys->p_dsbuffer = NULL;
    }
    IDirectSound_Release( sys->p_dsobject );
    sys->p_dsobject = NULL;
    return hr;
}

static HRESULT StreamStart( aout_stream_t *s,
                            audio_sample_format_t *restrict fmt,
                            const GUID *sid )
{
    aout_stream_sys_t *sys = calloc( 1, sizeof( *sys ) );
    if( unlikely(sys == NULL) )
        return E_OUTOFMEMORY;

    void *pv;
    HRESULT hr;
    if( sid )
    {
        DIRECTX_AUDIO_ACTIVATION_PARAMS params = {
            .cbDirectXAudioActivationParams = sizeof( params ),
            .guidAudioSession = *sid,
            .dwAudioStreamFlags = 0,
        };
        PROPVARIANT prop;

        PropVariantInit( &prop );
        prop.vt = VT_BLOB;
        prop.blob.cbSize = sizeof( params );
        prop.blob.pBlobData = (BYTE *)&params;

        hr = aout_stream_Activate( s, &IID_IDirectSound, &prop, &pv );
    }
    else
        hr = aout_stream_Activate( s, &IID_IDirectSound, NULL, &pv );
    if( FAILED(hr) )
        goto error;

    sys->p_dsobject = pv;

    hr = Start( VLC_OBJECT(s), sys, fmt );
    if( FAILED(hr) )
        goto error;

    s->sys = sys;
    s->time_get = StreamTimeGet;
    s->play = StreamPlay;
    s->pause = StreamPause;
    s->flush = StreamFlush;
    s->stop = StreamStop;
    return S_OK;
error:
    free( sys );
    return hr;
}

/**
 * Handles all the gory details of DirectSound initialization.
 */
static int InitDirectSound( audio_output_t *p_aout )
{
    aout_sys_t *sys = p_aout->sys;
    GUID guid, *p_guid = NULL;

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
    if FAILED( DirectSoundCreate( p_guid, &sys->s.p_dsobject, NULL ) )
    {
        msg_Warn( p_aout, "cannot create a direct sound device" );
        goto error;
    }

    return VLC_SUCCESS;

error:
    sys->s.p_dsobject = NULL;
    return VLC_EGENERIC;

}

static int VolumeSet( audio_output_t *p_aout, float volume )
{
    aout_sys_t *sys = p_aout->sys;
    int ret = 0;

    /* Directsound doesn't support amplification, so we use software
       gain if we need it and only for this */
    float gain = volume > 1.f ? volume * volume * volume : 1.f;
    aout_GainRequest( p_aout, gain );

    /* millibels from linear amplification */
    LONG mb = lroundf( 6000.f * log10f( __MIN( volume, 1.f ) ));

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
    if( !sys->volume.mute && sys->s.p_dsbuffer != NULL &&
        IDirectSoundBuffer_SetVolume( sys->s.p_dsbuffer, mb ) != DS_OK )
        return -1;
    /* Convert back to UI volume */
    aout_VolumeReport( p_aout, volume );

    if( var_InheritBool( p_aout, "volume-save" ) )
        config_PutFloat( "directx-volume", volume );
    return ret;
}

static int MuteSet( audio_output_t *p_aout, bool mute )
{
    HRESULT res = DS_OK;
    aout_sys_t *sys = p_aout->sys;

    sys->volume.mute = mute;

    if( sys->s.p_dsbuffer != NULL )
        res = IDirectSoundBuffer_SetVolume( sys->s.p_dsbuffer,
                                            mute? DSBVOLUME_MIN : sys->volume.mb );

    aout_MuteReport( p_aout, mute );
    return (res != DS_OK);
}

static int OutputStart( audio_output_t *p_aout,
                        audio_sample_format_t *restrict fmt )
{
    msg_Dbg( p_aout, "Opening DirectSound Audio Output" );

    /* Initialise DirectSound */
    if( InitDirectSound( p_aout ) )
    {
        msg_Err( p_aout, "cannot initialize DirectSound" );
        return -1;
    }

    aout_sys_t *sys = p_aout->sys;
    HRESULT hr = Start( VLC_OBJECT(p_aout), &sys->s, fmt );
    if( FAILED(hr) )
        return -1;

    /* Force volume update */
    VolumeSet( p_aout, sys->volume.volume );
    MuteSet( p_aout, sys->volume.mute );

    /* then launch the notification thread */
    p_aout->time_get = OutputTimeGet;
    p_aout->play = OutputPlay;
    p_aout->pause = OutputPause;
    p_aout->flush = OutputFlush;

    return 0;
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

    if( StringFromGUID2( guid, buf, 48 ) <= 0 )
        return true;

    list->count++;
    list->ids = realloc_or_free( list->ids, list->count * sizeof(char *) );
    if( list->ids == NULL )
        return false;
    list->names = realloc_or_free( list->names, list->count * sizeof(char *) );
    if( list->names == NULL )
    {
        free( list->ids );
        return false;
    }
    list->ids[list->count - 1] = FromWide( buf );
    list->names[list->count - 1] = FromWide( desc );

    (void) mod;
    return true;
}

/**
 * Stores the list of devices in preferences
 */
static int ReloadDirectXDevices( char const *psz_name,
                                 char ***values, char ***descs )
{
    ds_list_t list = {
        .count = 1,
        .ids = xmalloc(sizeof (char *)),
        .names = xmalloc(sizeof (char *)),
    };
    list.ids[0] = xstrdup("");
    list.names[0] = xstrdup(_("Default"));

    (void) psz_name;

    DirectSoundEnumerate( DeviceEnumCallback, &list );

    *values = list.ids;
    *descs = list.names;
    return list.count;
}

VLC_CONFIG_STRING_ENUM(ReloadDirectXDevices)

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

    aout->sys = sys;
    aout->start = OutputStart;
    aout->stop = OutputStop;
    aout->volume_set = VolumeSet;
    aout->mute_set = MuteSet;
    aout->device_select = DeviceSelect;

    /* Volume */
    sys->volume.volume = var_InheritFloat(aout, "directx-volume");
    aout_VolumeReport(aout, sys->volume.volume );
    MuteSet(aout, var_InheritBool(aout, "mute"));

    /* DirectSound does not support hot-plug events (unless with WASAPI) */
    char **ids, **names;
    int count = ReloadDirectXDevices(NULL, &ids, &names);
    msg_Dbg(obj, "found %d devices", count);
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

    char *dev = var_CreateGetNonEmptyString(aout, "directx-audio-device");
    aout_DeviceReport(aout, dev);
    free(dev);

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    var_Destroy(aout, "directx-audio-device");
    free(sys);
}

static void * PlayedDataEraser( void * data )
{
    const audio_output_t *aout = (audio_output_t *) data;
    aout_sys_t *aout_sys = aout->sys;
    aout_stream_sys_t *p_sys = &aout_sys->s;
    void *p_write_position, *p_wrap_around;
    unsigned long l_bytes1, l_bytes2;
    DWORD i_read;
    int64_t toerase, tosleep;
    vlc_tick_t ticksleep;
    HRESULT dsresult;

    for(;;)
    {
        int canc = vlc_savecancel();
        vlc_mutex_lock( &p_sys->lock );

        while( !p_sys->b_playing )
           vlc_cond_wait( &p_sys->cond, &p_sys->lock );

        toerase = 0;
        tosleep = 0;
        ticksleep = VLC_TICK_FROM_MS(20);

        dsresult = IDirectSoundBuffer_GetCurrentPosition( p_sys->p_dsbuffer,
                                                          &i_read, NULL );
        if( dsresult == DS_OK )
        {
            int64_t max = (int64_t) i_read - (int64_t) p_sys->i_write;
            tosleep = -max;
            if( max <= 0 )
                max += DS_BUF_SIZE;
            else
                tosleep += DS_BUF_SIZE;
            toerase = max;
            ticksleep = vlc_tick_from_sec( tosleep / p_sys->i_bytes_per_sample ) / p_sys->i_rate;
        }

        ticksleep = __MAX( ticksleep, VLC_TICK_FROM_MS(20) );
        dsresult = IDirectSoundBuffer_Lock( p_sys->p_dsbuffer,
                                            p_sys->i_write,
                                            toerase,
                                            &p_write_position,
                                            &l_bytes1,
                                            &p_wrap_around,
                                            &l_bytes2,
                                            0 );
        if( dsresult == DSERR_BUFFERLOST )
        {
            IDirectSoundBuffer_Restore( p_sys->p_dsbuffer );
            dsresult = IDirectSoundBuffer_Lock( p_sys->p_dsbuffer,
                                                p_sys->i_write,
                                                toerase,
                                                &p_write_position,
                                                &l_bytes1,
                                                &p_wrap_around,
                                                &l_bytes2,
                                                0 );
        }
        if( dsresult != DS_OK )
            goto wait;

        memset( p_write_position, 0, l_bytes1 );
        memset( p_wrap_around, 0, l_bytes2 );

        IDirectSoundBuffer_Unlock( p_sys->p_dsbuffer, p_write_position, l_bytes1,
                                   p_wrap_around, l_bytes2 );
wait:
        vlc_mutex_unlock(&p_sys->lock);
        vlc_restorecancel(canc);
        vlc_tick_sleep(ticksleep);
    }
    return NULL;
}
