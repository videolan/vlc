/*****************************************************************************
 * kai.c : KAI audio output plugin for vlc
 *****************************************************************************
 * Copyright (C) 2010 the VideoLAN team
 *
 * Authors: KO Myung-Hun <komh@chollian.net>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>

#include <float.h>

#include <kai.h>

#define FRAME_SIZE 2048

/*****************************************************************************
 * aout_sys_t: KAI audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    aout_packet_t   packet;
    HKAI            hkai;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );
static void Play  ( audio_output_t *_p_aout, block_t *block );

static ULONG APIENTRY KaiCallback ( PVOID, PVOID, ULONG );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define KAI_AUDIO_DEVICE_TEXT N_( \
    "Device" )
#define KAI_AUDIO_DEVICE_LONGTEXT N_( \
    "Select a proper audio device to be used by KAI." )

#define KAI_AUDIO_EXCLUSIVE_MODE_TEXT N_( \
    "Open audio in exclusive mode." )
#define KAI_AUDIO_EXCLUSIVE_MODE_LONGTEXT N_( \
    "Enable this option if you want your audio not to be interrupted by the " \
    "other audio." )

static const char *const ppsz_kai_audio_device[] = {
    "auto", "dart", "uniaud" };
static const char *const ppsz_kai_audio_device_text[] = {
    N_("Auto"), "DART", "UNIAUD" };

vlc_module_begin ()
    set_shortname( "KAI" )
    set_description( N_("K Audio Interface audio output") )
    set_capability( "audio output", 100 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_string( "kai-audio-device", ppsz_kai_audio_device[0],
                KAI_AUDIO_DEVICE_TEXT, KAI_AUDIO_DEVICE_LONGTEXT, false )
        change_string_list( ppsz_kai_audio_device, ppsz_kai_audio_device_text,
                            0 )
    add_bool( "kai-audio-exclusive-mode", false,
              KAI_AUDIO_EXCLUSIVE_MODE_TEXT, KAI_AUDIO_EXCLUSIVE_MODE_LONGTEXT,
              true )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Open: open the audio device
 *****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    audio_output_t *p_aout = (audio_output_t *)p_this;
    aout_sys_t *p_sys;
    char *psz_mode;
    ULONG i_kai_mode;
    KAISPEC ks_wanted, ks_obtained;
    int i_nb_channels;
    int i_bytes_per_frame;
    vlc_value_t val, text;
    audio_format_t format =  p_aout->format;

    /* Allocate structure */
    p_aout->sys = calloc( 1, sizeof( aout_sys_t ) );

    if( p_aout->sys == NULL )
        return VLC_ENOMEM;

    p_sys = p_aout->sys;

    if( var_Get( p_aout, "audio-device", &val ) != VLC_ENOVAR )
    {
        /* The user has selected an audio device. */
        if ( val.i_int == AOUT_VAR_STEREO )
        {
            format.i_physical_channels
                = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
        }
        else if ( val.i_int == AOUT_VAR_MONO )
        {
            format.i_physical_channels = AOUT_CHAN_CENTER;
        }
    }

    psz_mode = var_InheritString( p_aout, "kai-audio-device" );
    if( !psz_mode )
        psz_mode = ( char * )ppsz_kai_audio_device[ 0 ];  // "auto"

    i_kai_mode = KAIM_AUTO;
    if( strcmp( psz_mode, "dart" ) == 0 )
        i_kai_mode = KAIM_DART;
    else if( strcmp( psz_mode, "uniaud" ) == 0 )
        i_kai_mode = KAIM_UNIAUD;
    msg_Dbg( p_aout, "selected mode = %s", psz_mode );

    if( psz_mode != ppsz_kai_audio_device[ 0 ])
        free( psz_mode );

    i_nb_channels = aout_FormatNbChannels( &format );
    if ( i_nb_channels > 2 )
    {
        /* KAI doesn't support more than two channels. */
        i_nb_channels = 2;
        format.i_physical_channels
            = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    }

    /* Support s16l only */
    format.i_format = VLC_CODEC_S16L;

    aout_FormatPrepare( &format );

    i_bytes_per_frame = format.i_bytes_per_frame;

    /* Initialize library */
    if( kaiInit( i_kai_mode ))
    {
        msg_Err( p_aout, "cannot initialize KAI");

        goto exit_free_sys;
    }

    ks_wanted.usDeviceIndex   = 0;
    ks_wanted.ulType          = KAIT_PLAY;
    ks_wanted.ulBitsPerSample = BPS_16;
    ks_wanted.ulSamplingRate  = format.i_rate;
    ks_wanted.ulDataFormat    = MCI_WAVE_FORMAT_PCM;
    ks_wanted.ulChannels      = i_nb_channels;
    ks_wanted.ulNumBuffers    = 2;
    ks_wanted.ulBufferSize    = FRAME_SIZE * i_bytes_per_frame;
    ks_wanted.fShareable      = !var_InheritBool( p_aout,
                                                  "kai-audio-exclusive-mode");
    ks_wanted.pfnCallBack     = KaiCallback;
    ks_wanted.pCallBackData   = p_aout;
    msg_Dbg( p_aout, "requested ulBufferSize = %ld", ks_wanted.ulBufferSize );

    /* Open the sound device. */
    if( kaiOpen( &ks_wanted, &ks_obtained, &p_sys->hkai ))
    {
        msg_Err( p_aout, "cannot open KAI device");

        goto exit_kai_done;
    }

    msg_Dbg( p_aout, "open in %s mode",
             ks_obtained.fShareable ? "shareable" : "exclusive" );
    msg_Dbg( p_aout, "obtained i_nb_samples = %lu",
             ks_obtained.ulBufferSize / i_bytes_per_frame );
    msg_Dbg( p_aout, "obtained i_bytes_per_frame = %d",
             format.i_bytes_per_frame );

    p_aout->format   = format;

    p_aout->pf_play  = Play;
    p_aout->pf_pause = aout_PacketPause;
    p_aout->pf_flush = aout_PacketFlush;

    aout_PacketInit( p_aout, &p_sys->packet,
                     ks_obtained.ulBufferSize / i_bytes_per_frame );
    aout_VolumeSoftInit( p_aout );

    if ( var_Type( p_aout, "audio-device" ) == 0 )
    {
        /* First launch. */
        var_Create( p_aout, "audio-device",
                    VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
        text.psz_string = _("Audio Device");
        var_Change( p_aout, "audio-device", VLC_VAR_SETTEXT, &text, NULL );

        val.i_int = AOUT_VAR_STEREO;
        text.psz_string = _("Stereo");
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text );
        val.i_int = AOUT_VAR_MONO;
        text.psz_string = _("Mono");
        var_Change( p_aout, "audio-device", VLC_VAR_ADDCHOICE, &val, &text );
        if ( i_nb_channels == 2 )
        {
            val.i_int = AOUT_VAR_STEREO;
        }
        else
        {
            val.i_int = AOUT_VAR_MONO;
        }
        var_Change( p_aout, "audio-device", VLC_VAR_SETDEFAULT, &val, NULL );
        var_AddCallback( p_aout, "audio-device", aout_ChannelsRestart, NULL );
    }

    var_TriggerCallback( p_aout, "intf-change" );

    /* Prevent SIG_FPE */
    _control87(MCW_EM, MCW_EM);

    return VLC_SUCCESS;

exit_kai_done :
    kaiDone();

exit_free_sys :
    free( p_sys );

    return VLC_EGENERIC;
}

/*****************************************************************************
 * Play: play a sound samples buffer
 *****************************************************************************/
static void Play (audio_output_t *p_aout, block_t *block)
{
    aout_sys_t *p_sys = p_aout->sys;

    kaiPlay( p_sys->hkai );

    aout_PacketPlay( p_aout, block );
}

/*****************************************************************************
 * Close: close the audio device
 *****************************************************************************/
static void Close ( vlc_object_t *p_this )
{
    audio_output_t *p_aout = (audio_output_t *)p_this;
    aout_sys_t *p_sys = p_aout->sys;

    kaiClose( p_sys->hkai );
    kaiDone();

    aout_PacketDestroy( p_aout );
    free( p_sys );
}

/*****************************************************************************
 * KaiCallback: what to do once KAI has played sound samples
 *****************************************************************************/
static ULONG APIENTRY KaiCallback( PVOID p_cb_data,
                                   PVOID p_buffer,
                                   ULONG i_buf_size )
{
    audio_output_t *p_aout = (audio_output_t *)p_cb_data;
    aout_buffer_t  *p_aout_buffer;
    mtime_t current_date, next_date;
    ULONG i_len;

    /* We have 2 buffers, and a callback function is called right after KAI
     * runs out of a buffer. So we should get a packet to be played after the
     * remaining buffer.
     */
    next_date = mdate() + ( i_buf_size * 1000000LL
                                       / p_aout->format.i_bytes_per_frame
                                       / p_aout->format.i_rate
                                       * p_aout->format.i_frame_length );

    for (i_len = 0; i_len < i_buf_size;)
    {
        current_date = mdate();
        if( next_date < current_date )
            next_date = current_date;

        /* Get the next audio data buffer */
        p_aout_buffer = aout_PacketNext( p_aout, next_date );

        if( p_aout_buffer == NULL )
        {
            /* Means we are too early to request a new buffer ?
             * Try once again.
             */
            msleep( AOUT_MIN_PREPARE_TIME );
            next_date = mdate();
            p_aout_buffer = aout_PacketNext( p_aout, next_date );
        }

        if ( p_aout_buffer != NULL )
        {
            vlc_memcpy( ( uint8_t * ) p_buffer + i_len,
                        p_aout_buffer->p_buffer,
                        p_aout_buffer->i_buffer );

            i_len += p_aout_buffer->i_buffer;

            next_date += p_aout_buffer->i_length;

            block_Release( p_aout_buffer );
        }
        else
        {
            vlc_memset( ( uint8_t * ) p_buffer + i_len, 0, i_buf_size - i_len );

            i_len = i_buf_size;
        }
    }

    return i_buf_size;
}
