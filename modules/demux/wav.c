/*****************************************************************************
 * wav.c : wav file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: wav.c,v 1.1 2003/08/01 00:09:37 fenrir Exp $
 *
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

#include <vlc/vlc.h>
#include <vlc/input.h>

#include <codecs.h>
#include <ninput.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

static int  Activate    ( vlc_object_t * );
static void Deactivate  ( vlc_object_t * );

vlc_module_begin();
    set_description( _("WAV demuxer") );
    set_capability( "demux", 142 );
    set_callbacks( Activate, Deactivate );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int  Demux       ( input_thread_t * );

struct demux_sys_t
{
    stream_t        *s;

    WAVEFORMATEX    *p_wf;
    es_descriptor_t *p_es;

    int64_t         i_data_pos;
    unsigned int    i_data_size;

    mtime_t         i_time;
    unsigned int    i_frame_size;
    mtime_t         i_frame_length;
};


/*****************************************************************************
 * Declaration of local function
 *****************************************************************************/
#define __EVEN( x ) ( ( (x)%2 != 0 ) ? ((x)+1) : (x) )

#define GetWLE( p ) __GetWLE( (uint8_t*)(p) )
static inline uint32_t __GetWLE( uint8_t *p )
{
    return( p[0] + ( p[1] << 8 ) );
}
#define GetDWLE( p ) __GetDWLE( (uint8_t*)(p) )
static inline uint32_t __GetDWLE( uint8_t *p )
{
    return( p[0] + ( p[1] << 8 ) + ( p[2] << 16 ) + ( p[3] << 24 ) );
}

static int ChunkFind( input_thread_t *, char *, unsigned int * );

static void FrameInfo_IMA_ADPCM( input_thread_t *, unsigned int *, mtime_t * );
static void FrameInfo_MS_ADPCM ( input_thread_t *, unsigned int *, mtime_t * );
static void FrameInfo_PCM      ( input_thread_t *, unsigned int *, mtime_t * );

/*****************************************************************************
 * Activate: check file and initializes structures
 *****************************************************************************/
static int Activate( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys;

    uint8_t        *p_peek;
    unsigned int   i_size;
    vlc_fourcc_t   i_fourcc;

    /* Is it a wav file ? */
    if( input_Peek( p_input, &p_peek, 12 ) < 12 )
    {
        msg_Warn( p_input, "WAV module discarded (cannot peek)" );
        return VLC_EGENERIC;
    }
    if( strncmp( p_peek, "RIFF", 4 ) || strncmp( &p_peek[8], "WAVE", 4 ) )
    {
        msg_Warn( p_input, "WAV module discarded (not a valid file)" );
        return VLC_EGENERIC;
    }

    p_input->pf_demux     = Demux;
    p_input->p_demux_data = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->p_wf           = NULL;
    p_sys->p_es           = NULL;
    p_sys->i_time         = 0;

    if( ( p_sys->s = stream_OpenInput( p_input ) ) == NULL )
    {
        msg_Err( p_input, "cannot create stream" );
        goto error;
    }

    /* skip riff header */
    stream_Read( p_sys->s, NULL, 12 );  /* cannot fail as peek succeed */

    /* search fmt chunk */
    if( ChunkFind( p_input, "fmt ", &i_size ) )
    {
        msg_Err( p_input, "cannot find 'fmt ' chunk" );
        goto error;
    }
    if( i_size < sizeof( WAVEFORMATEX ) - 2 )   /* XXX -2 isn't a typo */
    {
        msg_Err( p_input, "invalid 'fmt ' chunk" );
        goto error;
    }
    stream_Read( p_sys->s, NULL, 8 );   /* cannot fail */

    /* load waveformatex */
    p_sys->p_wf = malloc( __EVEN( i_size ) + 2 ); /* +2, for raw audio -> no cbSize */
    p_sys->p_wf->cbSize = 0;
    if( stream_Read( p_sys->s, p_sys->p_wf, __EVEN( i_size ) ) < (int)__EVEN( i_size ) )
    {
        msg_Err( p_input, "cannot load 'fmt ' chunk" );
        goto error;
    }

    /* le->me */
    p_sys->p_wf->wFormatTag      = GetWLE ( &p_sys->p_wf->wFormatTag );
    p_sys->p_wf->nChannels       = GetWLE ( &p_sys->p_wf->nChannels );
    p_sys->p_wf->nSamplesPerSec  = GetDWLE( &p_sys->p_wf->nSamplesPerSec );
    p_sys->p_wf->nAvgBytesPerSec = GetDWLE( &p_sys->p_wf->nAvgBytesPerSec );
    p_sys->p_wf->nBlockAlign     = GetWLE ( &p_sys->p_wf->nBlockAlign );
    p_sys->p_wf->wBitsPerSample  = GetWLE ( &p_sys->p_wf->wBitsPerSample );
    p_sys->p_wf->cbSize          = GetWLE ( &p_sys->p_wf->cbSize );

    msg_Dbg( p_input, "format:0x%4.4x channels:%d %dHz %dKo/s blockalign:%d bits/samples:%d extra size:%d",
            p_sys->p_wf->wFormatTag,
            p_sys->p_wf->nChannels,
            p_sys->p_wf->nSamplesPerSec,
            p_sys->p_wf->nAvgBytesPerSec / 1024,
            p_sys->p_wf->nBlockAlign,
            p_sys->p_wf->wBitsPerSample,
            p_sys->p_wf->cbSize );

    if( ChunkFind( p_input, "data", &p_sys->i_data_size ) )
    {
        msg_Err( p_input, "cannot find 'data' chunk" );
        goto error;
    }

    stream_Control( p_sys->s, STREAM_GET_POSITION, &p_sys->i_data_pos );

    stream_Read( p_sys->s, NULL, 8 );   /* cannot fail */

    /* XXX p_sys->psz_demux shouldn't be NULL ! */
    switch( p_sys->p_wf->wFormatTag )
    {
        case( WAVE_FORMAT_PCM ):
            msg_Dbg( p_input,"found raw pcm audio format" );
            i_fourcc = VLC_FOURCC( 'a', 'r', 'a', 'w' );
            FrameInfo_PCM( p_input, &p_sys->i_frame_size, &p_sys->i_frame_length );
            break;
        case( WAVE_FORMAT_MULAW ):
            msg_Dbg( p_input,"found mulaw pcm audio format" );
            i_fourcc = VLC_FOURCC( 'u', 'l', 'a', 'w' );
            FrameInfo_PCM( p_input, &p_sys->i_frame_size, &p_sys->i_frame_length );
            break;
        case( WAVE_FORMAT_ALAW ):
            msg_Dbg( p_input,"found alaw pcm audio format" );
            i_fourcc = VLC_FOURCC( 'a', 'l', 'a', 'w' );
            FrameInfo_PCM( p_input, &p_sys->i_frame_size, &p_sys->i_frame_length );
            break;
        case( WAVE_FORMAT_ADPCM ):
            msg_Dbg( p_input, "found ms adpcm audio format" );
            i_fourcc = VLC_FOURCC( 'm', 's', 0x00, 0x02 );
            FrameInfo_MS_ADPCM( p_input, &p_sys->i_frame_size, &p_sys->i_frame_length );
            break;
        case( WAVE_FORMAT_IMA_ADPCM ):
            msg_Dbg( p_input, "found ima adpcm audio format" );
            i_fourcc = VLC_FOURCC( 'm', 's', 0x00, 0x11 );
            FrameInfo_IMA_ADPCM( p_input, &p_sys->i_frame_size, &p_sys->i_frame_length );
            break;

        case( WAVE_FORMAT_MPEG ):
        case( WAVE_FORMAT_MPEGLAYER3 ):
            msg_Dbg( p_input, "found mpeg audio format (relaying to another demux)" );
            /* FIXME set end of area FIXME */
            goto relay;
        case( WAVE_FORMAT_A52 ):
            msg_Dbg( p_input,"found a52 audio format (relaying to another demux)" );
            /* FIXME set end of area FIXME */
            goto relay;

        default:
            msg_Err( p_input,"unrecognize audio format(0x%x)",
                     p_sys->p_wf->wFormatTag );
            goto error;
    }


    /*  create one program */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        goto error;
    }
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot add program" );
        goto error;
    }
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];

    if( p_sys->i_data_size > 0 )
    {
        p_input->stream.i_mux_rate = (mtime_t)p_sys->i_frame_size * (mtime_t)1000000 / 50 / p_sys->i_frame_length;
    }
    else
    {
        p_input->stream.i_mux_rate = 0;
    }

    p_sys->p_es = input_AddES( p_input,
                                 p_input->stream.p_selected_program,
                                 1, AUDIO_ES, NULL, 0 );
    p_sys->p_es->i_stream_id = 1;
    p_sys->p_es->i_fourcc = i_fourcc;
    p_sys->p_es->p_waveformatex = malloc( sizeof( WAVEFORMATEX ) + p_sys->p_wf->cbSize );
    memcpy( p_sys->p_es->p_waveformatex,
            p_sys->p_wf,
            sizeof( WAVEFORMATEX ) + p_sys->p_wf->cbSize );

    input_SelectES( p_input, p_sys->p_es );

    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return VLC_SUCCESS;

error:
relay:
    if( p_sys->p_wf )
    {
        free( p_sys->p_wf );
    }
    if( p_sys->s )
    {
        stream_Release( p_sys->s );
    }
    free( p_sys );

    return VLC_EGENERIC;
}

/*****************************************************************************
 * Demux: read packet and send them to decoders
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( input_thread_t *p_input )
{
    demux_sys_t  *p_sys = p_input->p_demux_data;
    int64_t      i_pos;
    pes_packet_t *p_pes;

    if( p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT )
    {
        stream_Control( p_sys->s, STREAM_GET_POSITION, &i_pos );
        if( p_sys->p_wf->nBlockAlign != 0 )
        {
            i_pos += p_sys->p_wf->nBlockAlign - i_pos % p_sys->p_wf->nBlockAlign;
            if( stream_Control( p_sys->s, STREAM_SET_POSITION, i_pos ) )
            {
                msg_Err( p_input, "STREAM_SET_POSITION failed (cannot resync)" );
            }
        }
    }

    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_sys->i_time * 9 / 100 );

    stream_Control( p_sys->s, STREAM_GET_POSITION, &i_pos );

    if( p_sys->i_data_size > 0 &&
        i_pos >= p_sys->i_data_pos + p_sys->i_data_size )
    {
        /* EOF */
        return 0;
    }

    if( ( p_pes = stream_PesPacket( p_sys->s, p_sys->i_frame_size ) ) == NULL )
    {
        msg_Warn( p_input, "cannot read data" );
        return 0;
    }
    p_pes->i_dts =
    p_pes->i_pts = input_ClockGetTS( p_input,
                                     p_input->stream.p_selected_program,
                                     p_sys->i_time * 9 / 100 );

    if( !p_sys->p_es->p_decoder_fifo )
    {
        msg_Err( p_input, "no audio decoder" );
        input_DeletePES( p_input->p_method_data, p_pes );
        return( -1 );
    }

    input_DecodePES( p_sys->p_es->p_decoder_fifo, p_pes );
    p_sys->i_time += p_sys->i_frame_length;
    return( 1 );
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
static void Deactivate ( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys = p_input->p_demux_data;

    stream_Release( p_sys->s );
    free( p_sys->p_wf );
    free( p_sys );
}


/*****************************************************************************
 * Local functions
 *****************************************************************************/
static int ChunkFind( input_thread_t *p_input,
                      char *fcc, unsigned int *pi_size )
{
    demux_sys_t *p_sys = p_input->p_demux_data;
    uint8_t     *p_peek;

    for( ;; )
    {
        int i_size;

        if( stream_Peek( p_sys->s, &p_peek, 8 ) < 8 )
        {
            msg_Err( p_input, "cannot peek()" );
            return VLC_EGENERIC;
        }

        i_size = GetDWLE( p_peek + 4 );

        msg_Dbg( p_input, "Chunk: fcc=`%4.4s` size=%d", p_peek, i_size );

        if( !strncmp( p_peek, fcc, 4 ) )
        {
            if( pi_size )
            {
                *pi_size = i_size;
            }
            return VLC_SUCCESS;
        }

        i_size = __EVEN( i_size ) + 8;
        if( stream_Read( p_sys->s, NULL, i_size ) != i_size )
        {
            return VLC_EGENERIC;
        }
    }
}

static void FrameInfo_PCM( input_thread_t *p_input,
                           unsigned int   *pi_size,
                           mtime_t        *pi_length )
{
    WAVEFORMATEX *p_wf = p_input->p_demux_data->p_wf;
    int i_samples;

    int i_bytes;
    int i_modulo;

    /* read samples for 50ms of */
    i_samples = __MAX( p_wf->nSamplesPerSec / 20, 1 );


    *pi_length = (mtime_t)1000000 *
                 (mtime_t)i_samples /
                 (mtime_t)p_wf->nSamplesPerSec;

    i_bytes = i_samples * p_wf->nChannels * ( (p_wf->wBitsPerSample + 7) / 8 );

    if( p_wf->nBlockAlign > 0 )
    {
        if( ( i_modulo = i_bytes % p_wf->nBlockAlign ) != 0 )
        {
            i_bytes += p_wf->nBlockAlign - i_modulo;
        }
    }
    *pi_size = i_bytes;
}

static void FrameInfo_MS_ADPCM( input_thread_t *p_input,
                              unsigned int   *pi_size,
                              mtime_t        *pi_length )
{
    WAVEFORMATEX *p_wf = p_input->p_demux_data->p_wf;
    int i_samples;

    i_samples = 2 + 2 * ( p_wf->nBlockAlign -
                                7 * p_wf->nChannels ) / p_wf->nChannels;

    *pi_length = (mtime_t)1000000 *
                 (mtime_t)i_samples /
                 (mtime_t)p_wf->nSamplesPerSec;

    *pi_size = p_wf->nBlockAlign;
}

static void FrameInfo_IMA_ADPCM( input_thread_t *p_input,
                               unsigned int   *pi_size,
                               mtime_t        *pi_length )
{
    WAVEFORMATEX *p_wf = p_input->p_demux_data->p_wf;
    int i_samples;

    i_samples = 2 * ( p_wf->nBlockAlign -
                        4 * p_wf->nChannels ) / p_wf->nChannels;

    *pi_length = (mtime_t)1000000 *
                 (mtime_t)i_samples /
                 (mtime_t)p_wf->nSamplesPerSec;

    *pi_size = p_wf->nBlockAlign;
}

