/*****************************************************************************
 * wav.c : wav file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN
 * $Id: wav.c,v 1.8 2003/11/11 02:49:26 fenrir Exp $
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

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

static int  Open    ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin();
    set_description( _("WAV demuxer") );
    set_capability( "demux", 142 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int  Demux       ( input_thread_t * );

struct demux_sys_t
{
    es_format_t     fmt;
    es_out_id_t     *p_es;

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

static int ChunkFind( input_thread_t *, char *, unsigned int * );

static void FrameInfo_IMA_ADPCM( input_thread_t *, unsigned int *, mtime_t * );
static void FrameInfo_MS_ADPCM ( input_thread_t *, unsigned int *, mtime_t * );
static void FrameInfo_PCM      ( input_thread_t *, unsigned int *, mtime_t * );

/*****************************************************************************
 * Open: check file and initializes structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys;

    uint8_t        *p_peek;
    WAVEFORMATEX   *p_wf;
    unsigned int   i_size;
    char *psz_name;

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
    p_input->pf_demux_control = demux_vaControlDefault;
    p_input->p_demux_data = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->p_es           = NULL;
    p_sys->i_time         = 0;

    /* skip riff header */
    stream_Read( p_input->s, NULL, 12 );  /* cannot fail as peek succeed */

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
    stream_Read( p_input->s, NULL, 8 );   /* cannot fail */

    /* load waveformatex */
    p_wf = malloc( __EVEN( i_size ) + 2 ); /* +2, for raw audio -> no cbSize */
    p_wf->cbSize = 0;
    if( stream_Read( p_input->s,
                     p_wf, __EVEN( i_size ) ) < (int)__EVEN( i_size ) )
    {
        msg_Err( p_input, "cannot load 'fmt ' chunk" );
        goto error;
    }

    es_format_Init( &p_sys->fmt, AUDIO_ES, 0 );
    wf_tag_to_fourcc( GetWLE( &p_wf->wFormatTag ), &p_sys->fmt.i_codec, &psz_name );
    p_sys->fmt.audio.i_channels = GetWLE ( &p_wf->nChannels );
    p_sys->fmt.audio.i_samplerate = GetDWLE( &p_wf->nSamplesPerSec );
    p_sys->fmt.audio.i_blockalign = GetWLE ( &p_wf->nBlockAlign );
    p_sys->fmt.audio.i_bitrate    = GetDWLE( &p_wf->nAvgBytesPerSec ) * 8;
    p_sys->fmt.audio.i_bitspersample = GetWLE ( &p_wf->wBitsPerSample );;

    p_sys->fmt.i_extra = GetWLE ( &p_wf->cbSize );
    if( p_sys->fmt.i_extra > 0 )
    {
        p_sys->fmt.p_extra = malloc( p_sys->fmt.i_extra );
        memcpy( p_sys->fmt.p_extra, &p_wf[1], p_sys->fmt.i_extra );
    }

    msg_Dbg( p_input, "format:0x%4.4x channels:%d %dHz %dKo/s blockalign:%d bits/samples:%d extra size:%d",
            GetWLE( &p_wf->wFormatTag ),
            p_sys->fmt.audio.i_channels,
            p_sys->fmt.audio.i_samplerate,
            p_sys->fmt.audio.i_bitrate / 8 / 1024,
            p_sys->fmt.audio.i_blockalign,
            p_sys->fmt.audio.i_bitspersample,
            p_sys->fmt.i_extra );
    free( p_wf );

    switch( p_sys->fmt.i_codec )
    {
        case VLC_FOURCC( 'a', 'r', 'a', 'w' ):
        case VLC_FOURCC( 'u', 'l', 'a', 'w' ):
        case VLC_FOURCC( 'a', 'l', 'a', 'w' ):
            FrameInfo_PCM( p_input, &p_sys->i_frame_size, &p_sys->i_frame_length );
            break;
        case VLC_FOURCC( 'm', 's', 0x00, 0x02 ):
            FrameInfo_MS_ADPCM( p_input, &p_sys->i_frame_size, &p_sys->i_frame_length );
            break;
        case VLC_FOURCC( 'm', 's', 0x00, 0x11 ):
            FrameInfo_IMA_ADPCM( p_input, &p_sys->i_frame_size, &p_sys->i_frame_length );
            break;
        case VLC_FOURCC( 'm', 's', 0x00, 0x61 ):
        case VLC_FOURCC( 'm', 's', 0x00, 0x62 ):
            /* FIXME not sure at all FIXME */
            FrameInfo_MS_ADPCM( p_input, &p_sys->i_frame_size, &p_sys->i_frame_length );
            break;
        case VLC_FOURCC( 'm', 'p', 'g', 'a' ):
        case VLC_FOURCC( 'a', '5', '2', ' ' ):
            /* FIXME set end of area FIXME */
            goto relay;
        default:
            msg_Err( p_input, "unsupported codec (%4.4s)", (char*)&p_sys->fmt.i_codec );
            goto error;
    }

    msg_Dbg( p_input, "found %s audio format", psz_name );

    if( ChunkFind( p_input, "data", &p_sys->i_data_size ) )
    {
        msg_Err( p_input, "cannot find 'data' chunk" );
        goto error;
    }

    p_sys->i_data_pos = stream_Tell( p_input->s );

    stream_Read( p_input->s, NULL, 8 );   /* cannot fail */

    /*  create one program */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        goto error;
    }
    p_input->stream.i_mux_rate = 0;
    if( p_sys->i_data_size > 0 )
    {
        p_input->stream.i_mux_rate = (mtime_t)p_sys->i_frame_size *
                                     (mtime_t)1000000 / 50 / p_sys->i_frame_length;
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    p_sys->p_es = es_out_Add( p_input->p_es_out, &p_sys->fmt );
    return VLC_SUCCESS;

error:
relay:
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
        i_pos = stream_Tell( p_input->s );
        if( p_sys->fmt.audio.i_blockalign != 0 )
        {
            i_pos += p_sys->fmt.audio.i_blockalign - i_pos % p_sys->fmt.audio.i_blockalign;
            if( stream_Seek( p_input->s, i_pos ) )
            {
                msg_Err( p_input, "stream_Sekk failed (cannot resync)" );
            }
        }
    }

    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_sys->i_time * 9 / 100 );

    i_pos = stream_Tell( p_input->s );

    if( p_sys->i_data_size > 0 &&
        i_pos >= p_sys->i_data_pos + p_sys->i_data_size )
    {
        /* EOF */
        return 0;
    }

    if( ( p_pes = stream_PesPacket( p_input->s, p_sys->i_frame_size ) )==NULL )
    {
        msg_Warn( p_input, "cannot read data" );
        return 0;
    }
    p_pes->i_dts =
    p_pes->i_pts = input_ClockGetTS( p_input,
                                     p_input->stream.p_selected_program,
                                     p_sys->i_time * 9 / 100 );

    es_out_Send( p_input->p_es_out, p_sys->p_es, p_pes );

    p_sys->i_time += p_sys->i_frame_length;

    return( 1 );
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close ( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys = p_input->p_demux_data;

    free( p_sys );
}


/*****************************************************************************
 * Local functions
 *****************************************************************************/
static int ChunkFind( input_thread_t *p_input,
                      char *fcc, unsigned int *pi_size )
{
    uint8_t     *p_peek;

    for( ;; )
    {
        int i_size;

        if( stream_Peek( p_input->s, &p_peek, 8 ) < 8 )
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
        if( stream_Read( p_input->s, NULL, i_size ) != i_size )
        {
            return VLC_EGENERIC;
        }
    }
}

static void FrameInfo_PCM( input_thread_t *p_input,
                           unsigned int   *pi_size,
                           mtime_t        *pi_length )
{
    demux_sys_t    *p_sys = p_input->p_demux_data;

    int i_samples;

    int i_bytes;
    int i_modulo;

    /* read samples for 50ms of */
    i_samples = __MAX( p_sys->fmt.audio.i_samplerate / 20, 1 );


    *pi_length = (mtime_t)1000000 *
                 (mtime_t)i_samples /
                 (mtime_t)p_sys->fmt.audio.i_samplerate;

    i_bytes = i_samples * p_sys->fmt.audio.i_channels * ( (p_sys->fmt.audio.i_bitspersample + 7) / 8 );

    if( p_sys->fmt.audio.i_blockalign > 0 )
    {
        if( ( i_modulo = i_bytes % p_sys->fmt.audio.i_blockalign ) != 0 )
        {
            i_bytes += p_sys->fmt.audio.i_blockalign - i_modulo;
        }
    }
    *pi_size = i_bytes;
}

static void FrameInfo_MS_ADPCM( input_thread_t *p_input,
                              unsigned int   *pi_size,
                              mtime_t        *pi_length )
{
    demux_sys_t    *p_sys = p_input->p_demux_data;

    int i_samples;

    i_samples = 2 + 2 * ( p_sys->fmt.audio.i_blockalign -
                                7 * p_sys->fmt.audio.i_channels ) / p_sys->fmt.audio.i_channels;

    *pi_length = (mtime_t)1000000 *
                 (mtime_t)i_samples /
                 (mtime_t)p_sys->fmt.audio.i_samplerate;

    *pi_size = p_sys->fmt.audio.i_blockalign;
}

static void FrameInfo_IMA_ADPCM( input_thread_t *p_input,
                               unsigned int   *pi_size,
                               mtime_t        *pi_length )
{
    demux_sys_t    *p_sys = p_input->p_demux_data;

    int i_samples;

    i_samples = 2 * ( p_sys->fmt.audio.i_blockalign -
                        4 * p_sys->fmt.audio.i_channels ) / p_sys->fmt.audio.i_channels;

    *pi_length = (mtime_t)1000000 *
                 (mtime_t)i_samples /
                 (mtime_t)p_sys->fmt.audio.i_samplerate;

    *pi_size = p_sys->fmt.audio.i_blockalign;
}

