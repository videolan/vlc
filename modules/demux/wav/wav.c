/*****************************************************************************
 * wav.c : wav file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: wav.c,v 1.5 2002/12/03 17:00:16 fenrir Exp $
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
#include "wav.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int    WAVInit       ( vlc_object_t * );
static void __WAVEnd        ( vlc_object_t * );
static int    WAVDemux      ( input_thread_t * );
static int    WAVCallDemux  ( input_thread_t * );

#define WAVEnd(a) __WAVEnd(VLC_OBJECT(a))

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( "WAV demuxer" );
    set_capability( "demux", 142 );
    set_callbacks( WAVInit, __WAVEnd );
vlc_module_end();

/*****************************************************************************
 * Declaration of local function 
 *****************************************************************************/

#define FREE( p ) if( p ) free( p ); (p) = NULL

#define __EVEN( x ) ( (x)%2 != 0 ) ? ((x)+1) : (x)

/* Some functions to manipulate memory */
static u16 GetWLE( u8 *p_buff )
{
    return( (p_buff[0]) + ( p_buff[1] <<8 ) );
}

static u32 GetDWLE( u8 *p_buff )
{
    return( p_buff[0] + ( p_buff[1] <<8 ) +
            ( p_buff[2] <<16 ) + ( p_buff[3] <<24 ) );
}

static u32 CreateDWLE( int a, int b, int c, int d )
{
    return( a + ( b << 8 ) + ( c << 16 ) + ( d << 24 ) );
}


static off_t TellAbsolute( input_thread_t *p_input )
{
    off_t i_pos;
    
    vlc_mutex_lock( &p_input->stream.stream_lock );
    
    i_pos= p_input->stream.p_selected_area->i_tell;
//          - ( p_input->p_last_data - p_input->p_current_data  );

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return( i_pos );
}
 
static int SeekAbsolute( input_thread_t *p_input,
                         off_t i_pos)
{
    off_t i_filepos;

    if( i_pos >= p_input->stream.p_selected_area->i_size )
    {
    //    return( 0 );
    }
            
    i_filepos = TellAbsolute( p_input );
    if( i_pos != i_filepos )
    {
        p_input->pf_seek( p_input, i_pos );
        input_AccessReinit( p_input );
    }
    return( 1 );
}

static int SkipBytes( input_thread_t *p_input, int i_skip )
{
    return( SeekAbsolute( p_input, TellAbsolute( p_input ) + i_skip ) );
}

/* return 1 if success, 0 if fail */
static int ReadData( input_thread_t *p_input, u8 *p_buff, int i_size )
{
    data_packet_t *p_data;

    int i_read;

                
    if( !i_size )
    {
        return( 1 );
    }

    do
    {
        i_read = input_SplitBuffer(p_input, &p_data, __MIN( i_size, 1024 ) );
        if( i_read <= 0 )
        {
            return( 0 );
        }
        memcpy( p_buff, p_data->p_payload_start, i_read );
        input_DeletePacket( p_input->p_method_data, p_data );
        
        p_buff += i_read;
        i_size -= i_read;
                
    } while( i_size );
    
    return( 1 );
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
        return( 0 );
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
            return( 0 );
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
    return( 1 );
}

static int FindTag( input_thread_t *p_input, u32 i_tag )
{
    u32   i_id;
    u32   i_size;
    u8    *p_peek;

    for( ;; )
    {

        if( input_Peek( p_input, &p_peek, 8 ) < 8 )
        {
            msg_Err( p_input, "cannot peek()" );
            return( 0 );
        }

        i_id   = GetDWLE( p_peek );
        i_size = GetDWLE( p_peek + 4 );

        msg_Dbg( p_input, "FindTag: tag:%4.4s size:%d", &i_id, i_size );
        if( i_id == i_tag )
        {
            /* Yes, we have found the good tag */
            return( 1 );
        }
        if( !SkipBytes( p_input, __EVEN( i_size ) + 8 ) )
        {
            return( 0 );
        }
    }
}

static int LoadTag_fmt( input_thread_t *p_input, 
                        demux_sys_t *p_demux )
{
    u8  *p_peek;
    u32 i_size;
    WAVEFORMATEX *p_wf;
            

    if( input_Peek( p_input, &p_peek , 8 ) < 8 )
    {
        return( 0 );
    }

    p_demux->i_wf = i_size = GetDWLE( p_peek + 4 );
    SkipBytes( p_input, 8 );
    if( i_size < 16 )
    {
        SkipBytes( p_input, i_size );
        return( 0 );
    }
    p_wf = p_demux->p_wf = malloc( __MAX( i_size, sizeof( WAVEFORMATEX) ) );
    ReadData( p_input, (uint8_t*)p_wf, __EVEN( i_size ) );

    p_wf->wFormatTag      = GetWLE( (uint8_t*)&p_demux->p_wf->wFormatTag );
    p_wf->nChannels       = GetWLE( (uint8_t*)&p_demux->p_wf->nChannels );
    p_wf->nSamplesPerSec  = GetWLE( (uint8_t*)&p_demux->p_wf->nSamplesPerSec );
    p_wf->nAvgBytesPerSec = GetWLE( (uint8_t*)&p_demux->p_wf->nAvgBytesPerSec );
    p_wf->nBlockAlign     = GetWLE( (uint8_t*)&p_demux->p_wf->nBlockAlign );
    p_wf->wBitsPerSample  = GetWLE( (uint8_t*)&p_demux->p_wf->wBitsPerSample );
    if( i_size >= sizeof( WAVEFORMATEX) )
    {
        p_wf->cbSize          = GetWLE( (uint8_t*)&p_demux->p_wf->cbSize );
    }
    else
    {
        p_wf->cbSize = 0;
    }

    msg_Dbg( p_input, "loaded \"fmt \" chunk" );
    return( 1 );
}

static int PCM_GetFrame( input_thread_t *p_input,
                         WAVEFORMATEX   *p_wf,
                         pes_packet_t   **pp_pes,
                         mtime_t        *pi_length )
{
    int i_samples;

    int i_bytes;
    int i_modulo;

    /* read samples for 50ms of */
    i_samples = __MAX( p_wf->nSamplesPerSec / 20, 1 );
        
    
    *pi_length = (mtime_t)1000000 * 
                 (mtime_t)i_samples / 
                 (mtime_t)p_wf->nSamplesPerSec;

    i_bytes = i_samples * p_wf->nChannels * ( p_wf->wBitsPerSample + 7 ) / 8;
    
    if( p_wf->nBlockAlign > 0 )
    {
        if( ( i_modulo = i_bytes % p_wf->nBlockAlign ) != 0 )
        {
            i_bytes += p_wf->nBlockAlign - i_modulo;
        }
    }

    return( ReadPES( p_input, pp_pes, i_bytes ) );
}

static int MS_ADPCM_GetFrame( input_thread_t *p_input,
                              WAVEFORMATEX   *p_wf,
                              pes_packet_t   **pp_pes,
                              mtime_t        *pi_length )
{
    int i_samples;

    i_samples = 2 + 2 * ( p_wf->nBlockAlign - 
                                7 * p_wf->nChannels ) / p_wf->nChannels;
    
    *pi_length = (mtime_t)1000000 *
                 (mtime_t)i_samples /
                 (mtime_t)p_wf->nSamplesPerSec;

    return( ReadPES( p_input, pp_pes, p_wf->nBlockAlign ) );
}

static int IMA_ADPCM_GetFrame( input_thread_t *p_input,
                               WAVEFORMATEX   *p_wf,
                               pes_packet_t   **pp_pes,
                               mtime_t        *pi_length )
{
    int i_samples;

    i_samples = 2 * ( p_wf->nBlockAlign - 
                        4 * p_wf->nChannels ) / p_wf->nChannels;
    
    *pi_length = (mtime_t)1000000 *
                 (mtime_t)i_samples /
                 (mtime_t)p_wf->nSamplesPerSec;

    return( ReadPES( p_input, pp_pes, p_wf->nBlockAlign ) );
}

/*****************************************************************************
 * WAVInit: check file and initializes structures
 *****************************************************************************/
static int WAVInit( vlc_object_t * p_this )
{   
    input_thread_t *p_input = (input_thread_t *)p_this;
    u8  *p_peek;
    u32 i_size;
    
    demux_sys_t *p_demux;
    


    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE ;
    }

    /* a little test to see if it's a wav file */
    if( input_Peek( p_input, &p_peek, 12 ) < 12 )
    {
        msg_Warn( p_input, "WAV plugin discarded (cannot peek)" );
        return( -1 );
    }

    if( ( GetDWLE( p_peek ) != CreateDWLE( 'R', 'I', 'F', 'F' ) )||
        ( GetDWLE( p_peek + 8 ) != CreateDWLE( 'W', 'A', 'V', 'E' ) ) )
    {
        msg_Warn( p_input, "WAV plugin discarded (not a valid file)" );
        return( -1 );
    }
    i_size = GetDWLE( p_peek + 4 );
    SkipBytes( p_input, 12 );

    if( !FindTag( p_input, CreateDWLE( 'f', 'm', 't' ,' ' ) ) )
    {
        msg_Err( p_input, "cannot find \"fmt \" tag" );
        return( -1 );
    }

    /* create our structure that will contains all data */
    if( !( p_input->p_demux_data = 
                p_demux = malloc( sizeof( demux_sys_t ) ) ) )
    {
        msg_Err( p_input, "out of memory" );
        return( -1 );
    }
    memset( p_demux, 0, sizeof( demux_sys_t ) );
       
    /* Load WAVEFORMATEX header */
    if( !LoadTag_fmt( p_input, p_demux ) )
    {
        msg_Err( p_input, "cannot load \"fmt \" tag" );
        FREE( p_demux );
        return( -1 );
    }
    msg_Dbg( p_input, "format:0x%4.4x channels:%d %dHz %dKo/s blockalign:%d bits/samples:%d extra size:%d",
            p_demux->p_wf->wFormatTag,
            p_demux->p_wf->nChannels,
            p_demux->p_wf->nSamplesPerSec,
            p_demux->p_wf->nAvgBytesPerSec / 1024,
            p_demux->p_wf->nBlockAlign,
            p_demux->p_wf->wBitsPerSample,
            p_demux->p_wf->cbSize );
           
    if( !FindTag( p_input, CreateDWLE( 'd', 'a', 't', 'a' ) ) )
    {
        msg_Err( p_input, "cannot find \"data\" tag" );
        FREE( p_demux->p_wf );
        FREE( p_demux );
        return( -1 );
    }
    if( input_Peek( p_input, &p_peek, 8 ) < 8 )
    {
        msg_Warn( p_input, "WAV plugin discarded (cannot peek)" );
        FREE( p_demux->p_wf );
        FREE( p_demux );
        return( -1 );
    }

    p_demux->i_data_pos = TellAbsolute( p_input ) + 8;
    p_demux->i_data_size = GetDWLE( p_peek + 4 );
    SkipBytes( p_input, 8 );

    /* XXX p_demux->psz_demux shouldn't be NULL ! */
    switch( p_demux->p_wf->wFormatTag )
    {
        case( WAVE_FORMAT_PCM ):
            msg_Dbg( p_input,"found raw pcm audio format" );
            p_demux->i_fourcc = VLC_FOURCC( 'a', 'r', 'a', 'w' );
            p_demux->GetFrame = PCM_GetFrame;
            p_demux->psz_demux = strdup( "" );
            break;
        case( WAVE_FORMAT_MPEG ):
        case( WAVE_FORMAT_MPEGLAYER3 ):
            msg_Dbg( p_input, "found mpeg audio format" );
            p_demux->i_fourcc = VLC_FOURCC( 'm', 'p', 'g', 'a' );
            p_demux->GetFrame = NULL;
            p_demux->psz_demux = strdup( "mpegaudio" );
            break;
        case( WAVE_FORMAT_A52 ):
            msg_Dbg( p_input,"found a52 audio format" );
            p_demux->i_fourcc = VLC_FOURCC( 'a', '5', '2', ' ' );
            p_demux->GetFrame = NULL;
            p_demux->psz_demux = strdup( "a52" );
            break;
        case( WAVE_FORMAT_ADPCM ):
            msg_Dbg( p_input, "found ms adpcm audio format" );
            p_demux->i_fourcc = VLC_FOURCC( 'm', 's', 0x00, 0x02 );
            p_demux->GetFrame = MS_ADPCM_GetFrame;
            p_demux->psz_demux = strdup( "" );
            break;
        case( WAVE_FORMAT_IMA_ADPCM ):
            msg_Dbg( p_input, "found ima adpcm audio format" );
            p_demux->i_fourcc = VLC_FOURCC( 'm', 's', 0x00, 0x11 );
            p_demux->GetFrame = IMA_ADPCM_GetFrame;
            p_demux->psz_demux = strdup( "" );
            break;
        default:
            msg_Warn( p_input,"unrecognize audio format(0x%x)", 
                      p_demux->p_wf->wFormatTag );
            p_demux->i_fourcc = 
                VLC_FOURCC( 'm', 's', 
                            (p_demux->p_wf->wFormatTag >> 8)&0xff,
                            (p_demux->p_wf->wFormatTag )&0xff);
            p_demux->GetFrame = NULL;
            p_demux->psz_demux = strdup( "" );
            break;
    }
    
    if( p_demux->GetFrame )
    {
        msg_Dbg( p_input, "using internal demux" );

        p_input->pf_demux = WAVDemux;
        p_input->p_demux_data = p_demux;
        
        /*  create one program */
        vlc_mutex_lock( &p_input->stream.stream_lock );
        if( input_InitStream( p_input, 0 ) == -1)
        {
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            msg_Err( p_input, "cannot init stream" );
            // FIXME 
            return( -1 );
        }
        if( input_AddProgram( p_input, 0, 0) == NULL )
        {
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            msg_Err( p_input, "cannot add program" );
            // FIXME 
            return( -1 );
        }
        p_input->stream.p_selected_program = p_input->stream.pp_programs[0];
        p_input->stream.i_mux_rate = 0 ; /* FIXME */

        p_demux->p_es = input_AddES( p_input,
                                     p_input->stream.p_selected_program, 1,
                                     p_demux->i_wf );
        p_demux->p_es->i_stream_id = 1;
        p_demux->p_es->i_fourcc = p_demux->i_fourcc;
        p_demux->p_es->i_cat = AUDIO_ES;
        memcpy( p_demux->p_es->p_demux_data,
                p_demux->p_wf,
                p_demux->i_wf );
        
        input_SelectES( p_input, p_demux->p_es );
        
        p_input->stream.p_selected_program->b_is_ok = 1;
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
    else
    {
        char *psz_sav;
        /* call an external demux */
        msg_Warn( p_input, "unsupported formattag, using external demux" );
        
        psz_sav = p_input->psz_demux;
        p_input->psz_demux = p_demux->psz_demux;

        p_demux->p_demux = module_Need( p_input, "demux", NULL );
        
        p_input->psz_demux = psz_sav;
        
        if( !p_demux->p_demux )
        {
            msg_Err( p_input, 
                     "cannot get external demux for formattag 0x%x",
                     p_demux->p_wf->wFormatTag );
            FREE( p_demux->psz_demux );
            FREE( p_demux->p_wf );
            FREE( p_demux );
            return( -1 );
        }
        /* save value and switch back */
        p_demux->pf_demux = p_input->pf_demux;
        p_demux->p_demux_data = p_input->p_demux_data;

        p_input->pf_demux = WAVCallDemux;
        p_input->p_demux_data = p_demux;

    }

    return( 0 );    
}

/*****************************************************************************
 * WAVCallDemux: call true demux
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int WAVCallDemux( input_thread_t *p_input )
{
    demux_sys_t  *p_demux = p_input->p_demux_data;
    int i_status;
    char *psz_sav;
    
    /* save context */
    psz_sav = p_input->psz_demux;

    /* switch context */
    p_input->pf_demux = p_demux->pf_demux;
    p_input->p_demux_data = p_demux->p_demux_data;
    p_input->psz_demux = p_demux->psz_demux;

    /* call demux */
    i_status = p_input->pf_demux( p_input );

    /* save (new?) state */
    p_demux->pf_demux = p_input->pf_demux;
    p_demux->p_demux_data = p_input->p_demux_data;
    
    /* switch back */
    p_input->psz_demux = psz_sav;
    p_input->pf_demux = WAVCallDemux;
    p_input->p_demux_data = p_demux;

    return( i_status );
}

/*****************************************************************************
 * WAVDemux: read packet and send them to decoders
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int WAVDemux( input_thread_t *p_input )
{
    demux_sys_t  *p_demux = p_input->p_demux_data;
    pes_packet_t *p_pes;
    mtime_t      i_length;

    if( p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT )
    {
        off_t   i_offset;

        i_offset = TellAbsolute( p_input ) - p_demux->i_data_pos;
        if( i_offset < 0 )
        {
            i_offset = 0;
        }
        if( p_demux->p_wf->nBlockAlign != 0 )
        {
            i_offset += p_demux->p_wf->nBlockAlign - 
                                i_offset % p_demux->p_wf->nBlockAlign;
        }
        SeekAbsolute( p_input, p_demux->i_data_pos + i_offset );
    }
    
    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_demux->i_pcr );

    if( TellAbsolute( p_input ) >= p_demux->i_data_pos + p_demux->i_data_size )
    {
        return( 0 ); // EOF
    }

    if( !p_demux->GetFrame( p_input, p_demux->p_wf, &p_pes, &i_length ) )
    {
        msg_Warn( p_input, "failed to get one frame" );
        return( 0 );
    }

    p_pes->i_dts = 
        p_pes->i_pts = input_ClockGetTS( p_input, 
                                         p_input->stream.p_selected_program,
                                         p_demux->i_pcr );
   
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
    
    p_demux->i_pcr += i_length * 9 / 100;
    return( 1 );
}

/*****************************************************************************
 * WAVEnd: frees unused data
 *****************************************************************************/
static void __WAVEnd ( vlc_object_t * p_this )
{   
    input_thread_t *  p_input = (input_thread_t *)p_this;
    demux_sys_t *p_demux = p_input->p_demux_data;
    
    FREE( p_demux->p_wf );
    FREE( p_demux->psz_demux );
    
    if( p_demux->p_demux )
    {
        module_Unneed( p_input, p_demux->p_demux );
    }

}

