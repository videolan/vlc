/*****************************************************************************
 * file.c : audio output which writes the samples to a file
 *****************************************************************************
 * Copyright (C) 2002 VLC authors and VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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

#include <stdio.h>
#include <errno.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_codecs.h> /* WAVEHEADER */
#include <vlc_fs.h>

#define A52_FRAME_NB 1536

/*****************************************************************************
 * aout_sys_t: audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the direct sound specific properties of an audio device.
 *****************************************************************************/
typedef struct
{
    FILE     * p_file;
    bool b_add_wav_header;

    WAVEHEADER waveh;                      /* Wave header of the output file */
} aout_sys_t;

#define CHANNELS_MAX 6
static const int pi_channels_maps[CHANNELS_MAX+1] =
{
    0,
    AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int     Open        ( vlc_object_t * );
static void    Play        ( audio_output_t *, block_t *, vlc_tick_t );
static void    Pause       ( audio_output_t *, bool, vlc_tick_t );
static void    Flush       ( audio_output_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FORMAT_TEXT N_("Output format")

#define CHANNELS_TEXT N_("Number of output channels")
#define CHANNELS_LONGTEXT N_("By default (0), all the channels of the incoming " \
    "will be saved but you can restrict the number of channels here.")

#define WAV_TEXT N_("Add WAVE header")
#define WAV_LONGTEXT N_("Instead of writing a raw file, you can add a WAV " \
                        "header to the file.")

static const char *const format_list[] = {
    "u8", "s16",
#ifndef WORDS_BIGENDIAN
    "float32",
#endif
    "spdif",
};
static const int format_int[] = {
    VLC_CODEC_U8, VLC_CODEC_S16N,
#ifndef WORDS_BIGENDIAN
    VLC_CODEC_FL32,
#endif
    VLC_CODEC_SPDIFL,
};

#define FILE_TEXT N_("Output file")
#define FILE_LONGTEXT N_("File to which the audio samples will be written to (\"-\" for stdout).")

vlc_module_begin ()
    set_description( N_("File audio output") )
    set_shortname( N_("File") )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )

    add_savefile("audiofile-file", "audiofile.wav", FILE_TEXT, FILE_LONGTEXT)
    add_string( "audiofile-format", "s16",
                FORMAT_TEXT, FORMAT_TEXT, true )
        change_string_list( format_list, format_list )
    add_integer( "audiofile-channels", 0,
                 CHANNELS_TEXT, CHANNELS_LONGTEXT, true )
        change_integer_range( 0, 6 )
    add_bool( "audiofile-wav", true, WAV_TEXT, WAV_LONGTEXT, true )

    set_capability( "audio output", 0 )
    add_shortcut( "file", "audiofile" )
    set_callback( Open )
vlc_module_end ()

static int Start( audio_output_t *p_aout, audio_sample_format_t *restrict fmt )
{
    char * psz_name, * psz_format;
    int i_channels, i = 0;
    int i_format_list_size;

    if( aout_FormatNbChannels( fmt ) == 0 )
        return VLC_EGENERIC;

    psz_name = var_InheritString( p_aout, "audiofile-file" );
    if( !psz_name )
    {
        msg_Err( p_aout, "you need to specify an output file name" );
        free( psz_name );
        return VLC_EGENERIC;
    }

    /* Allocate structure */
    aout_sys_t *p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;
    p_aout->sys = p_sys;

    if( !strcmp( psz_name, "-" ) )
        p_sys->p_file = stdout;
    else
        p_sys->p_file = vlc_fopen( psz_name, "wb" );

    free( psz_name );
    if ( p_sys->p_file == NULL )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_aout->time_get = aout_TimeGetDefault;
    p_aout->play = Play;
    p_aout->pause = Pause;
    p_aout->flush = Flush;

    /* Audio format */
    psz_format = var_InheritString( p_aout, "audiofile-format" );
    if ( !psz_format ) /* FIXME */
    {
        if( p_sys->p_file != stdout )
            fclose( p_sys->p_file );
        free( p_sys );
        return VLC_EGENERIC;
    }

    i_format_list_size = (int)ARRAY_SIZE(format_list);
    for (i = 0; i < i_format_list_size; i++)
    {
        if ( !strncmp( format_list[i], psz_format, strlen(format_list[i]) ) )
        {
            break;
        }
    }

    if ( i == i_format_list_size )
    {
        msg_Err( p_aout, "cannot understand the format string (%s)",
                 psz_format );
        if( p_sys->p_file != stdout )
            fclose( p_sys->p_file );
        free( p_sys );
        free( psz_format );
        return VLC_EGENERIC;
    }
    free( psz_format );

    fmt->i_format = format_int[i];
    if ( AOUT_FMT_SPDIF( fmt ) )
    {
        fmt->i_bytes_per_frame = AOUT_SPDIF_SIZE;
        fmt->i_frame_length = A52_FRAME_NB;
    }

    /* Channels number */
    i_channels = var_InheritInteger( p_aout, "audiofile-channels" );
    if( i_channels > 0 && i_channels <= CHANNELS_MAX )
    {
        fmt->i_physical_channels = pi_channels_maps[i_channels];
    }
    fmt->channel_type = AUDIO_CHANNEL_TYPE_BITMAP;

    /* WAV header */
    p_sys->b_add_wav_header = var_InheritBool( p_aout, "audiofile-wav" );
    if( p_sys->b_add_wav_header )
    {
        /* Write wave header */
        WAVEHEADER *wh = &p_sys->waveh;

        memset( wh, 0, sizeof(*wh) );

        switch( fmt->i_format )
        {
#ifndef WORDS_BIGENDIAN
        case VLC_CODEC_FL32:
            wh->Format     = WAVE_FORMAT_IEEE_FLOAT;
            wh->BitsPerSample = sizeof(float) * 8;
            break;
#endif
        case VLC_CODEC_U8:
            wh->Format     = WAVE_FORMAT_PCM;
            wh->BitsPerSample = 8;
            break;
        default:
            wh->Format     = WAVE_FORMAT_PCM;
            wh->BitsPerSample = 16;
            break;
        }

        wh->MainChunkID = VLC_FOURCC('R', 'I', 'F', 'F');
        wh->Length = 0;                    /* temp, to be filled in as we go */
        wh->ChunkTypeID = VLC_FOURCC('W', 'A', 'V', 'E');
        wh->SubChunkID = VLC_FOURCC('f', 'm', 't', ' ');
        wh->SubChunkLength = 16;

        wh->Modus = aout_FormatNbChannels( fmt );
        wh->SampleFreq = fmt->i_rate;
        wh->BytesPerSample = wh->Modus * ( wh->BitsPerSample / 8 );
        wh->BytesPerSec = wh->BytesPerSample * wh->SampleFreq;

        wh->DataChunkID = VLC_FOURCC('d', 'a', 't', 'a');
        wh->DataLength = 0;                /* temp, to be filled in as we go */

        /* Header -> little endian format */
        SetWLE( &wh->Format, wh->Format );
        SetWLE( &wh->BitsPerSample, wh->BitsPerSample );
        SetDWLE( &wh->SubChunkLength, wh->SubChunkLength );
        SetWLE( &wh->Modus, wh->Modus );
        SetDWLE( &wh->SampleFreq, wh->SampleFreq );
        SetWLE( &wh->BytesPerSample, wh->BytesPerSample );
        SetDWLE( &wh->BytesPerSec, wh->BytesPerSec );

        if( fwrite( wh, sizeof(WAVEHEADER), 1,
                    p_sys->p_file ) != 1 )
        {
            msg_Err( p_aout, "write error: %s", vlc_strerror_c(errno) );
        }
    }

    return 0;
}

/*****************************************************************************
 * Close: close our file
 *****************************************************************************/
static void Stop( audio_output_t *p_aout )
{
    msg_Dbg( p_aout, "closing audio file" );
    aout_sys_t *p_sys = p_aout->sys;

    if( p_sys->b_add_wav_header )
    {
        /* Update Wave Header */
        p_sys->waveh.Length =
            p_sys->waveh.DataLength + sizeof(WAVEHEADER) - 4;

        /* Write Wave Header */
        if( fseek( p_sys->p_file, 0, SEEK_SET ) )
        {
            msg_Err( p_aout, "seek error: %s", vlc_strerror_c(errno) );
        }

        /* Header -> little endian format */
        SetDWLE( &p_sys->waveh.Length,
                 p_sys->waveh.Length );
        SetDWLE( &p_sys->waveh.DataLength,
                 p_sys->waveh.DataLength );

        if( fwrite( &p_sys->waveh, sizeof(WAVEHEADER), 1,
                    p_sys->p_file ) != 1 )
        {
            msg_Err( p_aout, "write error: %s", vlc_strerror_c(errno) );
        }
    }

    if( p_sys->p_file != stdout )
        fclose( p_sys->p_file );
    free( p_sys );
}

/*****************************************************************************
 * Play: pretend to play a sound
 *****************************************************************************/
static void Play( audio_output_t * p_aout, block_t *p_buffer, vlc_tick_t date )
{
    aout_sys_t *p_sys = p_aout->sys;
    if( fwrite( p_buffer->p_buffer, p_buffer->i_buffer, 1,
                p_sys->p_file ) != 1 )
    {
        msg_Err( p_aout, "write error: %s", vlc_strerror_c(errno) );
    }

    if( p_sys->b_add_wav_header )
    {
        /* Update Wave Header */
        p_sys->waveh.DataLength += p_buffer->i_buffer;
    }

    block_Release( p_buffer );
    (void) date;
}

static void Pause( audio_output_t *aout, bool paused, vlc_tick_t date )
{
    (void) aout; (void) paused; (void) date;
}

static void Flush( audio_output_t *aout )
{
    aout_sys_t *p_sys = aout->sys;
    if( fflush( p_sys->p_file ) )
        msg_Err( aout, "flush error: %s", vlc_strerror_c(errno) );
}

static int Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;

    aout->start = Start;
    aout->stop = Stop;
    aout->volume_set = NULL;
    aout->mute_set = NULL;
    return VLC_SUCCESS;
}
