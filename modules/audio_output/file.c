/*****************************************************************************
 * file.c : audio output which writes the samples to a file
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: file.c,v 1.21 2003/04/20 22:52:03 gbazin Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/aout.h>

#include "aout_internal.h"
#include "codecs.h"

#define FRAME_SIZE 2048
#define A52_FRAME_NB 1536

typedef struct WAVEHEADER
{
    uint32_t MainChunkID;                      // it will be 'RIFF'
    uint32_t Length;
    uint32_t ChunkTypeID;                      // it will be 'WAVE'
    uint32_t SubChunkID;                       // it will be 'fmt '
    uint32_t SubChunkLength;
    uint16_t Format;
    uint16_t Modus;
    uint32_t SampleFreq;
    uint32_t BytesPerSec;
    uint16_t BytesPerSample;
    uint16_t BitsPerSample;
    uint32_t DataChunkID;                      // it will be 'data'
    uint32_t DataLength;
} WAVEHEADER;

/*****************************************************************************
 * aout_sys_t: audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the direct sound specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    FILE     * p_file;
    vlc_bool_t b_add_wav_header;

    WAVEHEADER waveh;                      /* Wave header of the output file */
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int     Open        ( vlc_object_t * );
static void    Close       ( vlc_object_t * );
static void    Play        ( aout_instance_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FORMAT_TEXT N_("output format")
#define FORMAT_LONGTEXT N_("one of \"u8\", \"s8\", \"u16\", \"s16\", " \
                        "\"u16_le\", \"s16_le\", \"u16_be\", " \
                        "\"s16_be\", \"fixed32\", \"float32\" or \"spdif\"")
#define WAV_TEXT N_("add wave header")
#define WAV_LONGTEXT N_("instead of writing a raw file, you can add a wav " \
                        "header to the file")

static char *format_list[] = { "u8", "s8", "u16", "s16", "u16_le", "s16_le",
                               "u16_be", "s16_be", "fixed32", "float32",
                               "spdif", NULL };
static int format_int[] = { VLC_FOURCC('u','8',' ',' '),
                            VLC_FOURCC('s','8',' ',' '),
                            AOUT_FMT_U16_NE, AOUT_FMT_S16_NE,
                            VLC_FOURCC('u','1','6','l'),
                            VLC_FOURCC('s','1','6','l'),
                            VLC_FOURCC('u','1','6','b'),
                            VLC_FOURCC('s','1','6','b'),
                            VLC_FOURCC('f','i','3','2'),
                            VLC_FOURCC('f','l','3','2'),
                            VLC_FOURCC('s','p','i','f') };

#define FILE_TEXT N_("output file")
#define FILE_LONGTEXT N_("file to which the audio samples will be written to")

vlc_module_begin();
    add_category_hint( N_("Audio"), NULL, VLC_FALSE );
    add_string_from_list( "audiofile-format", "s16", format_list, NULL,
                          FORMAT_TEXT, FORMAT_LONGTEXT, VLC_TRUE );
    add_file( "audiofile", "audiofile.wav", NULL, FILE_TEXT,
              FILE_LONGTEXT, VLC_FALSE );
    add_bool( "audiofile-wav", 1, NULL, WAV_TEXT, WAV_LONGTEXT, VLC_TRUE );
    set_description( N_("file audio output") );
    set_capability( "audio output", 0 );
    add_shortcut( "file" );
    add_shortcut( "audiofile" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: open a dummy audio device
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;
    char * psz_name = config_GetPsz( p_this, "audiofile" );
    char * psz_format = config_GetPsz( p_aout, "audiofile-format" );
    char ** ppsz_compare = format_list;
    int i = 0;

   /* Allocate structure */
    p_aout->output.p_sys = malloc( sizeof( aout_sys_t ) );
    if( p_aout->output.p_sys == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return VLC_EGENERIC;
    }

    p_aout->output.p_sys->p_file = fopen( psz_name, "wb" );
    free( psz_name );
    if ( p_aout->output.p_sys->p_file == NULL )
    {
        free( p_aout->output.p_sys );
        return -1;
    }

    p_aout->output.pf_play = Play;

    while ( *ppsz_compare != NULL )
    {
        if ( !strncmp( *ppsz_compare, psz_format, strlen(*ppsz_compare) ) )
        {
            break;
        }
        ppsz_compare++; i++;
    }

    if ( *ppsz_compare == NULL )
    {
        msg_Err( p_aout, "Cannot understand the format string (%s)",
                 psz_format );
        fclose( p_aout->output.p_sys->p_file );
        free( p_aout->output.p_sys );
        return -1;
    }

    p_aout->output.output.i_format = format_int[i];
    if ( AOUT_FMT_NON_LINEAR( &p_aout->output.output ) )
    {
        p_aout->output.i_nb_samples = A52_FRAME_NB;
        p_aout->output.output.i_bytes_per_frame = AOUT_SPDIF_SIZE;
        p_aout->output.output.i_frame_length = A52_FRAME_NB;
        aout_VolumeNoneInit( p_aout );
    }
    else
    {
        p_aout->output.i_nb_samples = FRAME_SIZE;
        aout_VolumeSoftInit( p_aout );
    }

    p_aout->output.p_sys->b_add_wav_header =
        config_GetInt( p_this, "audiofile-wav" );

    if( p_aout->output.p_sys->b_add_wav_header )
    {
        /* Write wave header */
        WAVEHEADER *wh = &p_aout->output.p_sys->waveh;

        memset( wh, 0, sizeof(wh) );

        switch( p_aout->output.output.i_format )
        {
        case VLC_FOURCC('f','l','3','2'):
            wh->Format     = WAVE_FORMAT_IEEE_FLOAT;
            wh->BitsPerSample = sizeof(float) * 8;
            break;
        case VLC_FOURCC('u','8',' ',' '):
            wh->Format     = WAVE_FORMAT_PCM;
            wh->BitsPerSample = 8;
            break;
        case VLC_FOURCC('s','1','6','l'):
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

        wh->Modus = aout_FormatNbChannels( &p_aout->output.output );
        wh->SampleFreq = p_aout->output.output.i_rate;
        wh->BytesPerSample = wh->Modus * ( wh->BitsPerSample / 8 );
        wh->BytesPerSec = wh->BytesPerSample * wh->SampleFreq;

        wh->DataChunkID = VLC_FOURCC('d', 'a', 't', 'a');
        wh->DataLength = 0;                /* temp, to be filled in as we go */

        if( fwrite( wh, sizeof(WAVEHEADER), 1,
                    p_aout->output.p_sys->p_file ) != 1 )
        {
            msg_Err( p_aout, "write error (%s)", strerror(errno) );
        }
    }

    return 0;
}

/*****************************************************************************
 * Close: close our file
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;

    msg_Dbg( p_aout, "closing audio file" );

    if( p_aout->output.p_sys->b_add_wav_header )
    {
        /* Update Wave Header */
        p_aout->output.p_sys->waveh.Length =
            p_aout->output.p_sys->waveh.DataLength + sizeof(WAVEHEADER) - 4;

        /* Write Wave Header */
        if( fseek( p_aout->output.p_sys->p_file, 0, SEEK_SET ) )
        {
            msg_Err( p_aout, "seek error (%s)", strerror(errno) );
        }
        if( fwrite( &p_aout->output.p_sys->waveh, sizeof(WAVEHEADER), 1,
                    p_aout->output.p_sys->p_file ) != 1 )
        {
            msg_Err( p_aout, "write error (%s)", strerror(errno) );
        }
    }

    fclose( p_aout->output.p_sys->p_file );
    free( p_aout->output.p_sys );
}

/*****************************************************************************
 * Play: pretend to play a sound
 *****************************************************************************/
static void Play( aout_instance_t * p_aout )
{
    aout_buffer_t * p_buffer;

    p_buffer = aout_FifoPop( p_aout, &p_aout->output.fifo );

    if( fwrite( p_buffer->p_buffer, p_buffer->i_nb_bytes, 1,
                p_aout->output.p_sys->p_file ) != 1 )
    {
        msg_Err( p_aout, "write error (%s)", strerror(errno) );
    }

    if( p_aout->output.p_sys->b_add_wav_header )
    {
        /* Update Wave Header */
        p_aout->output.p_sys->waveh.DataLength += p_buffer->i_nb_bytes;
    }

    aout_BufferFree( p_buffer );
}
