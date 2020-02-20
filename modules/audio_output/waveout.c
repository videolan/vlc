/*****************************************************************************
 * waveout.c : Windows waveOut plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          André Weber
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>

#ifndef UNICODE
# define UNICODE
#endif
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_charset.h>              /* FromWide() */

#include "audio_output/windows_audio_common.h"

#define FRAME_SIZE 4096              /* The size is in samples, not in bytes */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );
static void Play         ( audio_output_t *, block_t *, vlc_tick_t );

/*****************************************************************************
 * notification_thread_t: waveOut event thread
 *****************************************************************************/

typedef struct aout_sys_t aout_sys_t;

struct lkwavehdr
{
    WAVEHDR hdr;
    struct lkwavehdr * p_next;
};

/* local functions */
static int OpenWaveOut   ( audio_output_t *, uint32_t,
                           int, int, int, int, bool );
static int OpenWaveOutPCM( audio_output_t *, uint32_t,
                           vlc_fourcc_t*, int, int, int, bool );
static int PlayWaveOut   ( audio_output_t *, HWAVEOUT, struct lkwavehdr *,
                           block_t *, bool );

static void CALLBACK WaveOutCallback ( HWAVEOUT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR );

static void WaveOutClean( aout_sys_t * p_sys );

static void WaveOutClearBuffer( HWAVEOUT, WAVEHDR *);

static uint32_t findDeviceID(char *);
static int WaveOutTimeGet(audio_output_t * , vlc_tick_t *);
static void WaveOutFlush( audio_output_t *);
static void WaveOutDrain( audio_output_t *);
static void WaveOutPause( audio_output_t *, bool, vlc_tick_t);
static int WaveoutVolumeSet(audio_output_t * p_aout, float volume);
static int WaveoutMuteSet(audio_output_t * p_aout, bool mute);

static void WaveoutPollVolume( void * );

static const wchar_t device_name_fmt[] = L"%ls ($%x,$%x)";

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

    size_t i_frames;

    int i_repeat_counter;

    int i_buffer_size;

    int i_rate;

    uint8_t *p_silence_buffer;              /* buffer we use to play silence */

    float f_volume;

    bool b_spdif;
    bool b_mute;
    bool b_soft;                            /* Use software gain */
    uint8_t chans_to_reorder;              /* do we need channel reordering */

    uint8_t chan_table[AOUT_CHAN_MAX];
    vlc_fourcc_t format;

    vlc_tick_t i_played_length;

    struct lkwavehdr * p_free_list;

    vlc_mutex_t lock;
    vlc_cond_t cond;
    vlc_timer_t volume_poll_timer;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DEVICE_TEXT N_("Select Audio Device")
#define DEVICE_LONG N_("Select special Audio device, or let windows "\
                       "decide (default), change needs VLC restart "\
                       "to apply.")

#define AUDIO_CHAN_TEXT N_("Audio output channels")
#define AUDIO_CHAN_LONGTEXT N_("Channels available for audio output. " \
    "If the input has more channels than the output, it will be down-mixed. " \
    "This parameter is ignored when digital pass-through is active.")

#define VOLUME_TEXT N_("Audio volume")

vlc_module_begin ()
    set_shortname( "WaveOut" )
    set_description( N_("WaveOut audio output") )
    set_capability( "audio output", 50 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_string( "waveout-audio-device", "wavemapper",
                 DEVICE_TEXT, DEVICE_LONG, false )
    add_float( "waveout-volume", 1.0f, VOLUME_TEXT, NULL, true )
         change_float_range(0.0f, 2.0f)
    add_bool( "waveout-float32", true, FLOAT_TEXT, FLOAT_LONGTEXT, true )
    add_integer ("waveout-audio-channels", 9, AUDIO_CHAN_TEXT,
                 AUDIO_CHAN_LONGTEXT, false)
        change_integer_range(1,9)
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Opens the audio device
 *****************************************************************************
 * This function opens and setups Win32 waveOut
 *****************************************************************************/
static int Start( audio_output_t *p_aout, audio_sample_format_t *restrict fmt )
{
    if( aout_FormatNbChannels( fmt ) == 0 )
        return VLC_EGENERIC;

    p_aout->time_get = WaveOutTimeGet;
    p_aout->play = Play;
    p_aout->pause = WaveOutPause;
    p_aout->flush = WaveOutFlush;
    p_aout->drain = WaveOutDrain;

    aout_sys_t *sys = p_aout->sys;

    /* Default behaviour is to use software gain */
    sys->b_soft = true;

    char *dev = var_GetNonEmptyString( p_aout, "waveout-audio-device");
    uint32_t devid = findDeviceID( dev );

    if(devid == WAVE_MAPPER && dev != NULL && stricmp(dev,"wavemapper"))
        msg_Warn( p_aout, "configured audio device '%s' not available, "
                          "using default instead", dev );
    free( dev );

    WAVEOUTCAPS waveoutcaps;
    if(waveOutGetDevCaps( devid, &waveoutcaps,
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



    /* Open the device */
    if( AOUT_FMT_SPDIF(fmt) && var_InheritBool (p_aout, "spdif") )
    {

        if( OpenWaveOut( p_aout, devid, VLC_CODEC_SPDIFL,
                         fmt->i_physical_channels,
                         aout_FormatNbChannels( fmt ), fmt->i_rate, false )
            == VLC_SUCCESS )
        {
            fmt->i_format = VLC_CODEC_SPDIFL;

            /* Calculate the frame size in bytes */
            fmt->i_bytes_per_frame = AOUT_SPDIF_SIZE;
            fmt->i_frame_length = A52_FRAME_NB;
            sys->i_buffer_size = fmt->i_bytes_per_frame;
            sys->b_spdif = true;

        }
        else
            msg_Err( p_aout,
                     "cannot open waveout audio device for spdif fallback to PCM" );
    }

    if( fmt->i_format != VLC_CODEC_SPDIFL )
    {
       /*
         check for configured audio device!
       */
       fmt->i_format = var_InheritBool( p_aout, "waveout-float32" )?
           VLC_CODEC_FL32: VLC_CODEC_S16N;

        int max_chan = var_InheritInteger( p_aout, "waveout-audio-channels");
        int i_channels = aout_FormatNbChannels(fmt);
        i_channels = ( i_channels < max_chan )? i_channels: max_chan;
        do
        {
            switch(i_channels)
            {
                case 9:
                    fmt->i_physical_channels = AOUT_CHANS_8_1;
                    break;
                case 8:
                    fmt->i_physical_channels = AOUT_CHANS_7_1;
                    break;
                case 7:
                    fmt->i_physical_channels = AOUT_CHANS_7_0;
                    break;
                case 6:
                    fmt->i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                        | AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT
                        | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE;
                    break;
                case 5:
                    fmt->i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                        | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
                        | AOUT_CHAN_LFE;
                    break;
                case 4:
                    fmt->i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                        | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
                    break;
                case 3:
                    fmt->i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                        | AOUT_CHAN_LFE;
                    break;
                case 2:
                    fmt->i_physical_channels = AOUT_CHANS_STEREO;
                    break;
                case 1:
                default:
                    fmt->i_physical_channels = AOUT_CHAN_CENTER;
            }
            msg_Dbg( p_aout, "Trying %d channels", i_channels );
        }
        while( ( OpenWaveOutPCM( p_aout, devid, &fmt->i_format,
                                 fmt->i_physical_channels, i_channels,
                                 fmt->i_rate, false ) != VLC_SUCCESS ) &&
               --i_channels );

        if( !i_channels )
        {
            msg_Err(p_aout, "Waveout couldn't find appropriate channel mapping");
            return VLC_EGENERIC;
        }

        /* Calculate the frame size in bytes */
        aout_FormatPrepare( fmt );
        sys->i_buffer_size = FRAME_SIZE * fmt->i_bytes_per_frame;

        if( waveoutcaps.dwSupport & WAVECAPS_VOLUME )
        {
            aout_GainRequest( p_aout, 1.0f );
            sys->b_soft = false;
        }

        WaveoutMuteSet( p_aout, sys->b_mute );

        sys->b_spdif = false;
    }

    sys->i_rate = fmt->i_rate;

    waveOutReset( sys->h_waveout );

    /* Allocate silence buffer */
    sys->p_silence_buffer =
        malloc( sys->i_buffer_size );
    if( sys->p_silence_buffer == NULL )
    {
        msg_Err( p_aout, "Couldn't alloc silence buffer... aborting");
        return VLC_ENOMEM;
    }
    sys->i_repeat_counter = 0;


    /* Zero the buffer. WinCE doesn't have calloc(). */
    memset( sys->p_silence_buffer, 0,
            sys->i_buffer_size );

    /* Now we need to setup our waveOut play notification structure */
    sys->i_frames = 0;
    sys->i_played_length = 0;
    sys->p_free_list = NULL;

    fmt->channel_type = AUDIO_CHANNEL_TYPE_BITMAP;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Play: play a sound buffer
 *****************************************************************************
 * This doesn't actually play the buffer. This just stores the buffer so it
 * can be played by the callback thread.
 *****************************************************************************/
static void Play( audio_output_t *p_aout, block_t *block, vlc_tick_t date )
{
    aout_sys_t *sys = p_aout->sys;

    struct lkwavehdr * p_waveheader =
        (struct lkwavehdr *) malloc(sizeof(struct lkwavehdr));
    if(!p_waveheader)
    {
        msg_Err(p_aout, "Couldn't alloc WAVEHDR");
        if( block )
            block_Release( block );
        return;
    }

    p_waveheader->p_next = NULL;

    if( block && sys->chans_to_reorder )
    {
        aout_ChannelReorder( block->p_buffer, block->i_buffer,
                             sys->waveformat.Format.nChannels,
                             sys->chan_table, sys->format );
    }
    while( PlayWaveOut( p_aout, sys->h_waveout, p_waveheader, block,
                        sys->b_spdif ) != VLC_SUCCESS )

    {
        msg_Warn( p_aout, "Couln't write frame... sleeping");
        vlc_tick_sleep( block->i_length );
    }

    WaveOutClean( sys );
    WaveoutPollVolume( p_aout );

    vlc_mutex_lock( &sys->lock );
    sys->i_frames++;
    sys->i_played_length += block->i_length;
    vlc_mutex_unlock( &sys->lock );
    (void) date;
}

/*****************************************************************************
 * Close: close the audio device
 *****************************************************************************/
static void Stop( audio_output_t *p_aout )
{
    aout_sys_t *p_sys = p_aout->sys;

    /* Before calling waveOutClose we must reset the device */
    waveOutReset( p_sys->h_waveout );

    /* wait for the frames to be queued in cleaning list */
    WaveOutDrain( p_aout );
    WaveOutClean( p_sys );

    /* now we can Close the device */
    if( waveOutClose( p_sys->h_waveout ) != MMSYSERR_NOERROR )
    {
        msg_Err( p_aout, "waveOutClose failed" );
    }

    free( p_sys->p_silence_buffer );
    p_sys->i_played_length = 0;
    p_sys->b_soft = true;
}

/*****************************************************************************
 * OpenWaveOut: open the waveout sound device
 ****************************************************************************/
static int OpenWaveOut( audio_output_t *p_aout, uint32_t i_device_id, int i_format,
                        int i_channels, int i_nb_channels, int i_rate,
                        bool b_probe )
{
    MMRESULT result;
    aout_sys_t *sys = p_aout->sys;

    /* Set sound format */

#define waveformat sys->waveformat

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
        waveformat.SubFormat = __KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF;
        break;

    case VLC_CODEC_FL32:
        waveformat.Format.wBitsPerSample = sizeof(float) * 8;
        waveformat.Samples.wValidBitsPerSample =
            waveformat.Format.wBitsPerSample;
        waveformat.Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        waveformat.SubFormat = __KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        break;

    case VLC_CODEC_S16N:
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
        msg_Dbg( p_aout,"waveformat.dwChannelMask          = %u",
                 waveformat.dwChannelMask);
    }

    /* Open the device */
    result = waveOutOpen( &sys->h_waveout, i_device_id,
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

    sys->chans_to_reorder =
        aout_CheckChannelReorder( pi_channels_in, pi_channels_out,
                                  waveformat.dwChannelMask,
                                  sys->chan_table );
    if( sys->chans_to_reorder )
        msg_Dbg( p_aout, "channel reordering needed" );
    sys->format = i_format;

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
        if ( OpenWaveOut( p_aout, i_device_id, VLC_CODEC_S16N,
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
 * PlayWaveOut: play a buffer through the WaveOut device
 *****************************************************************************/
static int PlayWaveOut( audio_output_t *p_aout, HWAVEOUT h_waveout,
                        struct lkwavehdr *p_waveheader, block_t *p_buffer, bool b_spdif)
{
    MMRESULT result;
    aout_sys_t *sys = p_aout->sys;

    /* Prepare the buffer */
    if( p_buffer != NULL )
    {
        p_waveheader->hdr.lpData = (LPSTR)p_buffer->p_buffer;
        p_waveheader->hdr.dwBufferLength = p_buffer->i_buffer;
        /*
          copy the buffer to the silence buffer :) so in case we don't
          get the next buffer fast enough (I will repeat this one a time
          for AC3 / DTS and SPDIF this will sound better instead of
          a hickup)
        */
        if(b_spdif)
        {
           memcpy( sys->p_silence_buffer,
                       p_buffer->p_buffer,
                       sys->i_buffer_size );
           sys->i_repeat_counter = 2;
        }
    } else {
        /* Use silence buffer instead */
        if(sys->i_repeat_counter)
        {
           sys->i_repeat_counter--;
           if(!sys->i_repeat_counter)
           {
               memset( sys->p_silence_buffer,
                           0x00, sys->i_buffer_size );
           }
        }
        p_waveheader->hdr.lpData = (LPSTR)sys->p_silence_buffer;
        p_waveheader->hdr.dwBufferLength = sys->i_buffer_size;
    }

    p_waveheader->hdr.dwUser = p_buffer ? (DWORD_PTR)p_buffer : (DWORD_PTR)1;
    p_waveheader->hdr.dwFlags = 0;

    result = waveOutPrepareHeader( h_waveout, &p_waveheader->hdr, sizeof(WAVEHDR) );
    if( result != MMSYSERR_NOERROR )
    {
        msg_Err( p_aout, "waveOutPrepareHeader failed" );
        return VLC_EGENERIC;
    }

    /* Send the buffer to the waveOut queue */
    result = waveOutWrite( h_waveout, &p_waveheader->hdr, sizeof(WAVEHDR) );
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
    (void) h_waveout;
    (void) dwParam2;
    aout_sys_t *sys = ((audio_output_t *)_p_aout)->sys;
    struct lkwavehdr * p_waveheader =  (struct lkwavehdr *) dwParam1;

    if( uMsg != WOM_DONE ) return;

    vlc_mutex_lock( &sys->lock );
    p_waveheader->p_next = sys->p_free_list;
    sys->p_free_list = p_waveheader;
    sys->i_frames--;
    vlc_cond_broadcast( &sys->cond );
    vlc_mutex_unlock( &sys->lock );
}

static void WaveOutClean( aout_sys_t * p_sys )
{
    struct lkwavehdr *p_whdr, *p_list;

    vlc_mutex_lock(&p_sys->lock);
    p_list =  p_sys->p_free_list;
    p_sys->p_free_list = NULL;
    vlc_mutex_unlock(&p_sys->lock);

    while( p_list )
    {
        p_whdr = p_list;
        p_list = p_list->p_next;
        WaveOutClearBuffer( p_sys->h_waveout, &p_whdr->hdr );
        free(p_whdr);
    }
}

static void WaveOutClearBuffer( HWAVEOUT h_waveout, WAVEHDR *p_waveheader )
{
    block_t *p_buffer = (block_t *)(p_waveheader->dwUser);
    /* Unprepare and free the buffers which has just been played */
    waveOutUnprepareHeader( h_waveout, p_waveheader, sizeof(WAVEHDR) );

    if( p_waveheader->dwUser != 1 )
        block_Release( p_buffer );
}

/*
  reload the configuration drop down list, of the Audio Devices
*/
static int ReloadWaveoutDevices( char const *psz_name,
                                 char ***values, char ***descs )
{
    int n = 0, nb_devices = waveOutGetNumDevs();

    VLC_UNUSED( psz_name );

    *values = xmalloc( (nb_devices + 1) * sizeof(char *) );
    *descs = xmalloc( (nb_devices + 1) * sizeof(char *) );

    (*values)[n] = strdup( "wavemapper" );
    (*descs)[n] = strdup( _("Microsoft Soundmapper") );
    n++;

    for(int i = 0; i < nb_devices; i++)
    {
        WAVEOUTCAPS caps;
        wchar_t dev_name[MAXPNAMELEN+32];

        if(waveOutGetDevCaps(i, &caps, sizeof(WAVEOUTCAPS))
                                                           != MMSYSERR_NOERROR)
            continue;

        _snwprintf(dev_name, MAXPNAMELEN + 32, device_name_fmt,
                   caps.szPname, caps.wMid, caps.wPid);
        (*values)[n] = FromWide( dev_name );
        (*descs)[n] = strdup( (*values)[n] );
        n++;
    }

    return n;
}

VLC_CONFIG_STRING_ENUM(ReloadWaveoutDevices)

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

        _snwprintf( dev_name, MAXPNAMELEN + 32, device_name_fmt,
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

static int DeviceSelect (audio_output_t *aout, const char *id)
{
    var_SetString(aout, "waveout-audio-device", (id != NULL) ? id : "");
    aout_DeviceReport (aout, id);
    aout_RestartRequest (aout, AOUT_RESTART_OUTPUT);
    return 0;
}

static int Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = malloc(sizeof (*sys));

    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;
    aout->volume_set = WaveoutVolumeSet;
    aout->mute_set = WaveoutMuteSet;
    aout->device_select = DeviceSelect;

    sys->f_volume = var_InheritFloat(aout, "waveout-volume");
    sys->b_mute = var_InheritBool(aout, "mute");

    aout_MuteReport(aout, sys->b_mute);
    aout_VolumeReport(aout, sys->f_volume );

    if( vlc_timer_create( &sys->volume_poll_timer,
                          WaveoutPollVolume, aout ) )
    {
        msg_Err( aout, "Couldn't create volume polling timer" );
        free( sys );
        return VLC_ENOMEM;
    }

    vlc_mutex_init( &sys->lock );
    vlc_cond_init( &sys->cond );

    /* WaveOut does not support hot-plug events so list devices at startup */
    char **ids, **names;
    int count = ReloadWaveoutDevices(NULL, &ids, &names);
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

    char *dev = var_CreateGetNonEmptyString(aout, "waveout-audio-device");
    aout_DeviceReport(aout, dev);
    free(dev);

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    var_Destroy(aout, "waveout-audio-device");

    vlc_timer_destroy( sys->volume_poll_timer );
    free(sys);
}

static int WaveOutTimeGet(audio_output_t * p_aout, vlc_tick_t *delay)
{
    MMTIME mmtime;
    mmtime.wType = TIME_SAMPLES;
    aout_sys_t *sys = p_aout->sys;

    if( !sys->i_frames )
        return -1;

    if( waveOutGetPosition( sys->h_waveout, &mmtime, sizeof(MMTIME) )
            != MMSYSERR_NOERROR )
    {
        msg_Err( p_aout, "waveOutGetPosition failed");
        return -1;
    }

    vlc_tick_t i_pos = vlc_tick_from_samples(mmtime.u.sample, sys->i_rate);
    *delay = sys->i_played_length - i_pos;
    return 0;
}

static void WaveOutFlush( audio_output_t *p_aout)
{
    MMRESULT res;
    aout_sys_t *sys = p_aout->sys;

    res  = waveOutReset( sys->h_waveout );
    sys->i_played_length = 0;
    if( res != MMSYSERR_NOERROR )
        msg_Err( p_aout, "waveOutReset failed");
}

static void WaveOutDrain( audio_output_t *p_aout)
{
    aout_sys_t *sys = p_aout->sys;

    vlc_mutex_lock( &sys->lock );
    while( sys->i_frames )
    {
        vlc_cond_wait( &sys->cond, &sys->lock );
    }
    vlc_mutex_unlock( &sys->lock );
}

static void WaveOutPause( audio_output_t * p_aout, bool pause, vlc_tick_t date)
{
    MMRESULT res;
    (void) date;
    aout_sys_t *sys = p_aout->sys;

    if(pause)
    {
        vlc_timer_schedule_asap( sys->volume_poll_timer, VLC_TICK_FROM_MS(200) );
        res = waveOutPause( sys->h_waveout );
        if( res != MMSYSERR_NOERROR )
        {
            msg_Err( p_aout, "waveOutPause failed (0x%x)", res);
            return;
        }
    }
    else
    {
        vlc_timer_disarm( sys->volume_poll_timer );
        res = waveOutRestart( sys->h_waveout );
        if( res != MMSYSERR_NOERROR )
        {
            msg_Err( p_aout, "waveOutRestart failed (0x%x)", res);
            return;
        }
    }
}

static int WaveoutVolumeSet( audio_output_t *p_aout, float volume )
{
    aout_sys_t *sys = p_aout->sys;

    if( sys->b_soft )
    {
        float gain = volume * volume * volume;
        if ( !sys->b_mute && aout_GainRequest( p_aout, gain ) )
            return -1;
    }
    else
    {
        const HWAVEOUT hwo = sys->h_waveout;

        uint32_t vol = lroundf( volume * 0x7fff.fp0 );

        if( !sys->b_mute )
        {
            if( vol > 0xffff )
            {
                vol = 0xffff;
                volume = 2.0f;
            }

            MMRESULT r = waveOutSetVolume( hwo, vol | ( vol << 16 ) );
            if( r != MMSYSERR_NOERROR )
            {
                msg_Err( p_aout, "waveOutSetVolume failed (%u)", r );
                return -1;
            }
        }
    }

    vlc_mutex_lock(&sys->lock);
    sys->f_volume = volume;

    if( var_InheritBool( p_aout, "volume-save" ) )
        config_PutFloat( "waveout-volume", volume );

    aout_VolumeReport( p_aout, volume );
    vlc_mutex_unlock(&sys->lock);

    return 0;
}

static int WaveoutMuteSet( audio_output_t * p_aout, bool mute )
{
    aout_sys_t *sys = p_aout->sys;

    if( sys->b_soft )
    {
        float gain = sys->f_volume * sys->f_volume * sys->f_volume;
        if ( aout_GainRequest( p_aout, mute ? 0.f : gain ) )
            return -1;
    }
    else
    {

        const HWAVEOUT hwo = sys->h_waveout;
        uint32_t vol = mute ? 0 : lroundf( sys->f_volume * 0x7fff.fp0 );

        if( vol > 0xffff )
            vol = 0xffff;

        MMRESULT r = waveOutSetVolume( hwo, vol | ( vol << 16 ) );
        if( r != MMSYSERR_NOERROR )
        {
            msg_Err( p_aout, "waveOutSetVolume failed (%u)", r );
            return -1;
        }
    }

    vlc_mutex_lock(&sys->lock);
    sys->b_mute = mute;
    aout_MuteReport( p_aout, mute );
    vlc_mutex_unlock(&sys->lock);

    return 0;
}

static void WaveoutPollVolume( void * _aout )
{
    audio_output_t * aout = (audio_output_t *) _aout;
    aout_sys_t *sys = aout->sys;
    uint32_t vol;

    MMRESULT r = waveOutGetVolume( sys->h_waveout, (LPDWORD) &vol );

    if( r != MMSYSERR_NOERROR )
    {
        msg_Err( aout, "waveOutGetVolume failed (%u)", r );
        return;
    }

    float volume = (float) ( vol & UINT32_C( 0xffff ) );
    volume /= 0x7fff.fp0;

    vlc_mutex_lock(&sys->lock);
    if( !sys->b_mute && volume != sys->f_volume )
    {
        sys->f_volume = volume;

        if( var_InheritBool( aout, "volume-save" ) )
            config_PutFloat( "waveout-volume", volume );

        aout_VolumeReport( aout, volume );
    }
    vlc_mutex_unlock(&sys->lock);

    return;
}
