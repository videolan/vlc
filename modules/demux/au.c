/*****************************************************************************
 * au.c : au file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: au.c,v 1.2 2003/05/05 22:23:34 gbazin Exp $
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#include <vlc/vlc.h>
#include <vlc/input.h>

#include <codecs.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int    AUInit       ( vlc_object_t * );
static void   AUEnd        ( vlc_object_t * );
static int    AUDemux      ( input_thread_t * );

static int    AUDemuxPCM   ( input_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("AU demuxer") );
    set_capability( "demux", 142 );
    set_callbacks( AUInit, AUEnd );
vlc_module_end();


#define AUDIO_FILE_ENCODING_MULAW_8         1  /* 8-bit ISDN u-law */
#define AUDIO_FILE_ENCODING_LINEAR_8        2  /* 8-bit linear PCM */
#define AUDIO_FILE_ENCODING_LINEAR_16       3  /* 16-bit linear PCM */
#define AUDIO_FILE_ENCODING_LINEAR_24       4  /* 24-bit linear PCM */
#define AUDIO_FILE_ENCODING_LINEAR_32       5  /* 32-bit linear PCM */
#define AUDIO_FILE_ENCODING_FLOAT           6  /* 32-bit IEEE floating point */
#define AUDIO_FILE_ENCODING_DOUBLE          7  /* 64-bit IEEE floating point */
#define AUDIO_FILE_ENCODING_ADPCM_G721      23 /* 4-bit CCITT g.721 ADPCM */
#define AUDIO_FILE_ENCODING_ADPCM_G722      24 /* CCITT g.722 ADPCM */
#define AUDIO_FILE_ENCODING_ADPCM_G723_3    25 /* CCITT g.723 3-bit ADPCM */
#define AUDIO_FILE_ENCODING_ADPCM_G723_5    26 /* CCITT g.723 5-bit ADPCM */
#define AUDIO_FILE_ENCODING_ALAW_8          27 /* 8-bit ISDN A-law */

typedef struct
{
    uint32_t    i_header_size;
    uint32_t    i_data_size;
    uint32_t    i_encoding;
    uint32_t    i_sample_rate;
    uint32_t    i_channels;
} au_t;

#define AU_DEMUX_PCM        0x01
#define AU_DEMUX_ADPCM      0x02
struct demux_sys_t
{
    au_t            au;
    WAVEFORMATEX    wf;

    mtime_t         i_time;

    es_descriptor_t *p_es;

    int             i_demux;
};

/*****************************************************************************
 * Declaration of local function
 *****************************************************************************/

#define FREE( p ) if( p ) { free( p ); (p) = NULL; }

static uint32_t GetDWBE( uint8_t *p_buff )
{
    return( ( p_buff[0] << 24 ) + ( p_buff[1] << 16 ) +
            ( p_buff[2] <<  8 ) + p_buff[3] );
}


static off_t TellAbsolute( input_thread_t *p_input )
{
    off_t i_pos;

    vlc_mutex_lock( &p_input->stream.stream_lock );

    i_pos= p_input->stream.p_selected_area->i_tell;

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return( i_pos );
}

/* return 1 if success, 0 if fail */
static int ReadData( input_thread_t *p_input, uint8_t *p_buff, int i_size )
{
    data_packet_t *p_data;

    int i_count = 0;

    if( !i_size )
    {
        return( 0 );
    }

    do
    {
        int i_read;

        i_read = input_SplitBuffer(p_input, &p_data, __MIN( i_size, 1024 ) );
        if( i_read <= 0 )
        {
            return( i_count );
        }
        memcpy( p_buff, p_data->p_payload_start, i_read );
        input_DeletePacket( p_input->p_method_data, p_data );

        p_buff += i_read;
        i_size -= i_read;
        i_count += i_read;

    } while( i_size );

    return( i_count );
}

static int SeekAbsolute( input_thread_t *p_input,
                         off_t i_pos)
{
    int         i_skip;

    i_skip    = i_pos - TellAbsolute( p_input );
    if( i_skip == 0 )
    {
        return( VLC_SUCCESS );
    }
    if( i_skip < 0 && !p_input->stream.b_seekable )
    {
        return( VLC_EGENERIC );
    }
    else if( !p_input->stream.b_seekable ||
             ( i_skip > 0 && i_skip < 1024 && p_input->stream.i_method != INPUT_METHOD_FILE ) )
    {
        while( i_skip > 0 )
        {
            uint8_t dummy[1024];
            int     i_read;

            i_read = ReadData( p_input, dummy, __MIN( i_skip, 1024 ) );
            if( i_read <= 0 )
            {
                return( VLC_EGENERIC );
            }
            i_skip -= i_read;
        }
        return( VLC_SUCCESS );
    }
    else
    {
            input_AccessReinit( p_input );
            p_input->pf_seek( p_input, i_pos );
            return( VLC_SUCCESS );
    }
}

static int SkipBytes( input_thread_t *p_input, int i_skip )
{
    return( SeekAbsolute( p_input, TellAbsolute( p_input ) + i_skip ) );
}

static int ReadPES( input_thread_t *p_input,
                    pes_packet_t **pp_pes,
                    int i_size )
{
    pes_packet_t *p_pes;

    *pp_pes = NULL;

    if( !(p_pes = input_NewPES( p_input->p_method_data )) )
    {
        msg_Err( p_input, "cannot allocate new PES" );
        return( VLC_EGENERIC );
    }

    while( i_size > 0 )
    {
        data_packet_t   *p_data;
        int i_read;

        if( (i_read = input_SplitBuffer( p_input,
                                         &p_data,
                                         __MIN( i_size, 1024 ) ) ) <= 0 )
        {
            input_DeletePES( p_input->p_method_data, p_pes );
            return( VLC_EGENERIC );
        }
        if( !p_pes->p_first )
        {
            p_pes->p_first = p_data;
            p_pes->i_nb_data = 1;
            p_pes->i_pes_size = i_read;
        }
        else
        {
            p_pes->p_last->p_next  = p_data;
            p_pes->i_nb_data++;
            p_pes->i_pes_size += i_read;
        }
        p_pes->p_last  = p_data;
        i_size -= i_read;
    }
    *pp_pes = p_pes;
    return( VLC_SUCCESS );
}

/*****************************************************************************
 * AUInit: check file and initializes structures
 *****************************************************************************/
static int AUInit( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    uint8_t  *p_peek;

    au_t            au;
    WAVEFORMATEX    wf;
    vlc_fourcc_t    i_codec;

    demux_sys_t     *p_demux;
    es_descriptor_t *p_es;


    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE ;
    }

    /* a little test to see if it's a wav file */
    if( input_Peek( p_input, &p_peek, 24 ) < 24 )
    {
        msg_Warn( p_input, "AU plugin discarded (cannot peek)" );
        return( -1 );
    }

    /* read header */
    if( strcmp( p_peek, ".snd" ) )
    {
        msg_Warn( p_input, "AU plugin discarded (not a valid file)" );
        return( VLC_EGENERIC );
    }
    au.i_header_size   = GetDWBE( &p_peek[0x04] );
    au.i_data_size     = GetDWBE( &p_peek[0x08] );
    au.i_encoding      = GetDWBE( &p_peek[0x0c] );
    au.i_sample_rate   = GetDWBE( &p_peek[0x10] );
    au.i_channels      = GetDWBE( &p_peek[0x14] );
    msg_Dbg( p_input,
             "au file: header_size=%d data_size=%d encoding=0x%x sample_rate=%d channels=%d",
             au.i_header_size,
             au.i_data_size,
             au.i_encoding,
             au.i_sample_rate,
             au.i_channels );
    if( au.i_header_size < 24 )
    {
        msg_Warn( p_input, "AU plugin discarded (not a valid file)" );
        return( VLC_EGENERIC );
    }

    /* Create WAVEFORMATEX structure */
    wf.nChannels     = au.i_channels;
    wf.nSamplesPerSec= au.i_sample_rate;
    wf.cbSize        = 0;

    switch( au.i_encoding )
    {
        case AUDIO_FILE_ENCODING_MULAW_8:       /* 8-bit ISDN u-law */
            wf.wFormatTag     = WAVE_FORMAT_MULAW;   // FIXME ??
            wf.wBitsPerSample = 8;
            wf.nBlockAlign    = 1 * wf.nChannels;
            i_codec              = VLC_FOURCC( 'u','l','a','w' );
            break;
        case AUDIO_FILE_ENCODING_LINEAR_8:      /* 8-bit linear PCM */
            wf.wFormatTag     = WAVE_FORMAT_PCM;
            wf.wBitsPerSample = 8;
            wf.nBlockAlign    = 1 * wf.nChannels;
            i_codec              = VLC_FOURCC( 't','w','o','s' );
            break;

        case AUDIO_FILE_ENCODING_LINEAR_16:     /* 16-bit linear PCM */
            wf.wFormatTag     = WAVE_FORMAT_PCM;
            wf.wBitsPerSample = 16;
            wf.nBlockAlign    = 2 * wf.nChannels;
            i_codec              = VLC_FOURCC( 't','w','o','s' );
            break;

        case AUDIO_FILE_ENCODING_LINEAR_24:     /* 24-bit linear PCM */
            wf.wFormatTag     = WAVE_FORMAT_PCM;
            wf.wBitsPerSample = 24;
            wf.nBlockAlign    = 3 * wf.nChannels;
            i_codec              = VLC_FOURCC( 't','w','o','s' );
            break;

        case AUDIO_FILE_ENCODING_LINEAR_32:     /* 32-bit linear PCM */
            wf.wFormatTag     = WAVE_FORMAT_PCM;
            wf.wBitsPerSample = 32;
            wf.nBlockAlign    = 4 * wf.nChannels;
            i_codec              = VLC_FOURCC( 't','w','o','s' );
            break;

        case AUDIO_FILE_ENCODING_FLOAT:         /* 32-bit IEEE floating point */
            wf.wFormatTag     = WAVE_FORMAT_UNKNOWN;
            wf.wBitsPerSample = 32;
            wf.nBlockAlign    = 4 * wf.nChannels;
            i_codec              = VLC_FOURCC( 'a', 'u', 0, AUDIO_FILE_ENCODING_FLOAT );
            break;

        case AUDIO_FILE_ENCODING_DOUBLE:        /* 64-bit IEEE floating point */
            wf.wFormatTag     = WAVE_FORMAT_UNKNOWN;
            wf.wBitsPerSample = 64;
            wf.nBlockAlign    = 8 * wf.nChannels;
            i_codec              = VLC_FOURCC( 'a', 'u', 0, AUDIO_FILE_ENCODING_DOUBLE );
            break;

        case AUDIO_FILE_ENCODING_ADPCM_G721:    /* 4-bit CCITT g.721 ADPCM */
            wf.wFormatTag     = WAVE_FORMAT_UNKNOWN;
            wf.wBitsPerSample = 0;
            wf.nBlockAlign    = 0 * wf.nChannels;
            i_codec              = VLC_FOURCC( 'a', 'u', 0, AUDIO_FILE_ENCODING_ADPCM_G721 );
            break;

        case AUDIO_FILE_ENCODING_ADPCM_G722:    /* CCITT g.722 ADPCM */
            wf.wFormatTag     = WAVE_FORMAT_UNKNOWN;
            wf.wBitsPerSample = 0;
            wf.nBlockAlign    = 0 * wf.nChannels;
            i_codec              = VLC_FOURCC( 'a', 'u', 0, AUDIO_FILE_ENCODING_ADPCM_G722 );
            break;

        case AUDIO_FILE_ENCODING_ADPCM_G723_3:  /* CCITT g.723 3-bit ADPCM */
            wf.wFormatTag     = WAVE_FORMAT_UNKNOWN;
            wf.wBitsPerSample = 0;
            wf.nBlockAlign    = 0 * wf.nChannels;
            i_codec              = VLC_FOURCC( 'a', 'u', 0, AUDIO_FILE_ENCODING_ADPCM_G723_3 );
            break;

        case AUDIO_FILE_ENCODING_ADPCM_G723_5:  /* CCITT g.723 5-bit ADPCM */
            wf.wFormatTag     = WAVE_FORMAT_UNKNOWN;
            wf.wBitsPerSample = 0;
            wf.nBlockAlign    = 0 * wf.nChannels;
            i_codec              = VLC_FOURCC( 'a', 'u', 0, AUDIO_FILE_ENCODING_ADPCM_G723_5 );
            break;

        case AUDIO_FILE_ENCODING_ALAW_8:        /* 8-bit ISDN A-law */
            wf.wFormatTag     = WAVE_FORMAT_ALAW;   // FIXME ??
            wf.wBitsPerSample = 8;
            wf.nBlockAlign    = 1 * wf.nChannels;
            i_codec              = VLC_FOURCC( 'a','l','a','w' );
            break;

        default:
            msg_Warn( p_input, "unknow encoding=0x%x", au.i_encoding );
            wf.wFormatTag     = WAVE_FORMAT_UNKNOWN;
            wf.wBitsPerSample = 0;
            wf.nBlockAlign    = 0 * wf.nChannels;
            i_codec              = VLC_FOURCC( 'a','u', 0, au.i_encoding );
            break;
    }
    wf.nAvgBytesPerSec        = wf.nSamplesPerSec * wf.nChannels * wf.wBitsPerSample / 8;

    /*  create one program */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        return( VLC_EGENERIC );
    }
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot add program" );
        return( VLC_EGENERIC );
    }

    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];
    p_input->stream.i_mux_rate =  wf.nAvgBytesPerSec / 50;

    p_es = input_AddES( p_input, p_input->stream.p_selected_program,
                        0x01, AUDIO_ES, NULL, 0 );

    p_es->i_stream_id   = 0x01;
    p_es->i_fourcc      = i_codec;
    p_es->p_waveformatex= malloc( sizeof( WAVEFORMATEX ) );
    memcpy( p_es->p_waveformatex,
            &wf,
            sizeof( WAVEFORMATEX ) );

    input_SelectES( p_input, p_es );
    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* create our structure that will contains all data */
    if( ( p_demux = malloc( sizeof( demux_sys_t ) ) ) == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return( VLC_EGENERIC );
    }
    p_demux->p_es   = p_es;
    p_demux->i_time = 0;
    memcpy( &p_demux->au, &au, sizeof( au_t ) );
    memcpy( &p_demux->wf, &wf, sizeof( WAVEFORMATEX ) );
    switch( au.i_encoding )
    {
        case AUDIO_FILE_ENCODING_MULAW_8:
        case AUDIO_FILE_ENCODING_LINEAR_8:
        case AUDIO_FILE_ENCODING_LINEAR_16:
        case AUDIO_FILE_ENCODING_LINEAR_24:
        case AUDIO_FILE_ENCODING_LINEAR_32:
        case AUDIO_FILE_ENCODING_FLOAT:
        case AUDIO_FILE_ENCODING_DOUBLE:
        case AUDIO_FILE_ENCODING_ALAW_8:
            p_demux->i_demux = AU_DEMUX_PCM;
            break;

        case AUDIO_FILE_ENCODING_ADPCM_G721:
        case AUDIO_FILE_ENCODING_ADPCM_G722:
        case AUDIO_FILE_ENCODING_ADPCM_G723_3:
        case AUDIO_FILE_ENCODING_ADPCM_G723_5:
            p_demux->i_demux = AU_DEMUX_ADPCM;
            break;

        default:
            p_demux->i_demux = 0;
            break;
    }

    p_input->p_demux_data = p_demux;
    p_input->pf_demux = AUDemux;

    /* skip header*/
    SkipBytes( p_input, au.i_header_size );

    return( VLC_SUCCESS );
}

/*****************************************************************************
 * AUDemux: read packet and send them to decoders
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int AUDemux( input_thread_t *p_input )
{
    demux_sys_t  *p_demux = p_input->p_demux_data;

    if( p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT )
    {
        off_t   i_offset;

        i_offset = TellAbsolute( p_input ) - p_demux->au.i_header_size;
        if( i_offset < 0 )
        {
            i_offset = 0;
        }
        if( p_demux->wf.nBlockAlign != 0 )
        {
            i_offset += p_demux->wf.nBlockAlign -
                                i_offset % p_demux->wf.nBlockAlign;
        }
        SeekAbsolute( p_input, p_demux->au.i_header_size + i_offset );
    }

    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_demux->i_time * 9 / 100 );

    switch( p_demux->i_demux )
    {
        case AU_DEMUX_PCM:
            return( AUDemuxPCM( p_input ) );
        case AU_DEMUX_ADPCM:
        default:
            msg_Err( p_input, "Internal error (p_demux->i_demux invalid/unsported)" );
            return( 0 );
    }
}
static int AUDemuxPCM( input_thread_t *p_input )
{
    demux_sys_t  *p_demux = p_input->p_demux_data;
    pes_packet_t *p_pes;

    int i_samples;

    int i_bytes;
    int i_modulo;

    /* read samples for 50ms of */
    i_samples = __MAX( p_demux->wf.nSamplesPerSec / 20, 1 );



    i_bytes = i_samples * p_demux->wf.nChannels * ( (p_demux->wf.wBitsPerSample + 7) / 8 );

    if( p_demux->wf.nBlockAlign > 0 )
    {
        if( ( i_modulo = i_bytes % p_demux->wf.nBlockAlign ) != 0 )
        {
            i_bytes += p_demux->wf.nBlockAlign - i_modulo;
        }
    }

    if( ReadPES( p_input, &p_pes, i_bytes ) )
    {
        msg_Warn( p_input, "failed to get one frame" );
        return( 0 );
    }

    p_pes->i_dts =
        p_pes->i_pts = input_ClockGetTS( p_input,
                                         p_input->stream.p_selected_program,
                                         p_demux->i_time * 9 / 100 );

    if( !p_demux->p_es->p_decoder_fifo )
    {
        msg_Err( p_input, "no audio decoder" );
        input_DeletePES( p_input->p_method_data, p_pes );
        return( -1 );
    }
    else
    {
        input_DecodePES( p_demux->p_es->p_decoder_fifo, p_pes );
    }

    p_demux->i_time += (mtime_t)1000000 *
                       (mtime_t)i_samples /
                       (mtime_t)p_demux->wf.nSamplesPerSec;

    return( 1 );
}

/*****************************************************************************
 * AUEnd: frees unused data
 *****************************************************************************/
static void AUEnd ( vlc_object_t * p_this )
{
    input_thread_t *  p_input = (input_thread_t *)p_this;

    FREE( p_input->p_demux_data );
}

