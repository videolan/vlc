/*****************************************************************************
 * decoder.c: AAC decoder using libfaad2
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: decoder.c,v 1.12 2002/11/14 22:38:47 massiot Exp $
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
#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#include <faad.h>

#include "decoder.h"


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder    ( vlc_object_t * );

static int  RunDecoder     ( decoder_fifo_t * );
static int  InitThread     ( adec_thread_t * );
static void DecodeThread   ( adec_thread_t * );
static void EndThread      ( adec_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    set_description( _("AAC decoder module (libfaad2)") );
    set_capability( "decoder", 60 );
    set_callbacks( OpenDecoder, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to choose.
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;
    
    if( p_fifo->i_fourcc != VLC_FOURCC('m','p','4','a') )
    {   
        return VLC_EGENERIC;
    }

    p_fifo->pf_run = RunDecoder;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunDecoder: this function is called just after the thread is created
 *****************************************************************************/
static int RunDecoder( decoder_fifo_t *p_fifo )
{
    adec_thread_t *p_adec;
    int b_error;

    if( !( p_adec = malloc( sizeof( adec_thread_t ) ) ) )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return( -1 );
    }
    memset( p_adec, 0, sizeof( adec_thread_t ) );
    
    p_adec->p_fifo = p_fifo;

    if( InitThread( p_adec ) != 0 )
    {
        DecoderError( p_fifo );
        return( -1 );
    }

    while( ( !p_adec->p_fifo->b_die )&&( !p_adec->p_fifo->b_error ) )
    {
        DecodeThread( p_adec );
    }


    if( ( b_error = p_adec->p_fifo->b_error ) )
    {
        DecoderError( p_adec->p_fifo );
    }

    EndThread( p_adec );
    if( b_error )
    {
        return( -1 );
    }

    return( 0 );
}

static int pi_channels_maps[6] =
{
    0,
    AOUT_CHAN_CENTER,   AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
};

#define FREE( p ) if( p != NULL ) free( p ); p = NULL
#define GetWLE( p ) \
    ( *(u8*)(p) + ( *((u8*)(p)+1) << 8 ) )

#define GetDWLE( p ) \
    (  *(u8*)(p) + ( *((u8*)(p)+1) << 8 ) + \
        ( *((u8*)(p)+2) << 16 ) + ( *((u8*)(p)+3) << 24 ) )
    
static void faac_GetWaveFormatEx( waveformatex_t *p_wh,
                                          u8 *p_data )
{

    p_wh->i_formattag     = GetWLE( p_data );
    p_wh->i_nb_channels   = GetWLE( p_data + 2 );
    p_wh->i_samplespersec = GetDWLE( p_data + 4 );
    p_wh->i_avgbytespersec= GetDWLE( p_data + 8 );
    p_wh->i_blockalign    = GetWLE( p_data + 12 );
    p_wh->i_bitspersample = GetWLE( p_data + 14 );
    p_wh->i_size          = GetWLE( p_data + 16 );

    if( p_wh->i_size )
    {
        p_wh->p_data = malloc( p_wh->i_size );
        memcpy( p_wh->p_data, p_data + 18, p_wh->i_size );
    }
}

static void GetPESData( u8 *p_buf, int i_max, pes_packet_t *p_pes )
{   
    int i_copy; 
    int i_count;

    data_packet_t   *p_data;

    i_count = 0;
    p_data = p_pes->p_first;
    while( p_data != NULL && i_count < i_max )
    {

        i_copy = __MIN( p_data->p_payload_end - p_data->p_payload_start, 
                        i_max - i_count );

        if( i_copy > 0 )
        {
            memcpy( p_buf,
                    p_data->p_payload_start,
                    i_copy );
        }

        p_data = p_data->p_next;
        i_count += i_copy;
        p_buf   += i_copy;
    }

    if( i_count < i_max )
    {
        memset( p_buf, 0, i_max - i_count );
    }
}

/*****************************************************************************
 * InitThread: initialize data before entering main loop
 *****************************************************************************/
static int InitThread( adec_thread_t * p_adec )
{
    int i_status;
    unsigned long i_rate;
    unsigned char i_nb_channels;
            
    faacDecConfiguration *p_faad_config;

    if( !p_adec->p_fifo->p_demux_data )
    {
        msg_Warn( p_adec->p_fifo,
                  "cannot load stream informations" );
    }
    else
    {
        faac_GetWaveFormatEx( &p_adec->format,
                              (u8*)p_adec->p_fifo->p_demux_data );
    }

    p_adec->p_buffer = NULL;
    p_adec->i_buffer = 0;


    if( !( p_adec->p_handle = faacDecOpen() ) )
    {
        msg_Err( p_adec->p_fifo,
                 "cannot initialize faad" );
        FREE( p_adec->format.p_data );
        return( -1 );
    }
    
    if( p_adec->format.p_data == NULL )
    {
        int i_frame_size;
        pes_packet_t *p_pes;

        msg_Warn( p_adec->p_fifo,
                 "DecoderSpecificInfo missing, trying with first frame" );
        // gather first frame
        do
        {
            input_ExtractPES( p_adec->p_fifo, &p_pes );
            if( !p_pes )
            {
                return( -1 );
            }
            i_frame_size = p_pes->i_pes_size;

            if( i_frame_size > 0 )
            {
                if( p_adec->i_buffer < i_frame_size + 16 )
                {
                    FREE( p_adec->p_buffer );
                    p_adec->p_buffer = malloc( i_frame_size + 16 );
                    p_adec->i_buffer = i_frame_size + 16;
                }
                
                GetPESData( p_adec->p_buffer, p_adec->i_buffer, p_pes );
            }
            else
            {
                input_DeletePES( p_adec->p_fifo->p_packets_mgt, p_pes );
            }
        } while( i_frame_size <= 0 );


        i_status = faacDecInit( p_adec->p_handle,
                                p_adec->p_buffer,
                                i_frame_size,
                                &i_rate,
                                &i_nb_channels );
    }
    else
    {
        i_status = faacDecInit2( p_adec->p_handle,
                                 p_adec->format.p_data,
                                 p_adec->format.i_size,
                                 &i_rate,
                                 &i_nb_channels );
    }

    if( i_status < 0 )
    {
        msg_Err( p_adec->p_fifo,
                 "failed to initialize faad" );
        faacDecClose( p_adec->p_handle );
        return( -1 );
    }
    msg_Dbg( p_adec->p_fifo,
             "faad intitialized, samplerate:%dHz channels:%d",
             i_rate, 
             i_nb_channels );


    /* set default configuration */
    p_faad_config = faacDecGetCurrentConfiguration( p_adec->p_handle );
    p_faad_config->outputFormat = FAAD_FMT_FLOAT;
    faacDecSetConfiguration( p_adec->p_handle, p_faad_config );
        

    /* Initialize the thread properties */
    p_adec->output_format.i_format = VLC_FOURCC('f','l','3','2');
    p_adec->output_format.i_rate = i_rate;
    p_adec->output_format.i_channels = pi_channels_maps[i_nb_channels];
    p_adec->p_aout = NULL;
    p_adec->p_aout_input = NULL;

    p_adec->pts = 0;
  
    return( 0 );
}

/*****************************************************************************
 * DecodeThread: decodes a frame
 *****************************************************************************
 * XXX it will work only for frame based streams like from mp4 file
 *     but it's the only way to use libfaad2 without having segfault
 *     after 1 or 2 frames
 *****************************************************************************/
static void DecodeThread( adec_thread_t *p_adec )
{
    aout_buffer_t    *p_aout_buffer;

    void             *p_faad_buffer;
    faacDecFrameInfo faad_frame;
    int  i_frame_size;
    pes_packet_t *p_pes;

    /* **** Get a new frames from streams **** */
    do
    {
        input_ExtractPES( p_adec->p_fifo, &p_pes );
        if( !p_pes )
        {
            p_adec->p_fifo->b_error = 1;
            return;
        }
        if( p_pes->i_pts != 0 )
        {
            p_adec->pts = p_pes->i_pts;
        }
        i_frame_size = p_pes->i_pes_size;

        if( i_frame_size > 0 )
        {
            if( p_adec->i_buffer < i_frame_size + 16 )
            {
                FREE( p_adec->p_buffer );
                p_adec->p_buffer = malloc( i_frame_size + 16 );
                p_adec->i_buffer = i_frame_size + 16;
            }
            
            GetPESData( p_adec->p_buffer, p_adec->i_buffer, p_pes );
        }
        input_DeletePES( p_adec->p_fifo->p_packets_mgt, p_pes );
    } while( i_frame_size <= 0 );
    
    /* **** decode this frame **** */
    p_faad_buffer = faacDecDecode( p_adec->p_handle,
                                   &faad_frame,
                                   p_adec->p_buffer,
                                   i_frame_size );

    /* **** some sanity checks to see if we have samples to out **** */
    if( faad_frame.error > 0 )
    {
        msg_Warn( p_adec->p_fifo, "%s", 
                  faacDecGetErrorMessage(faad_frame.error) );
        return;
    }
    if( ( faad_frame.channels <= 0 )||
        ( faad_frame.channels > AAC_MAXCHANNELS) ||
        ( faad_frame.channels > 5 ) )
    {
        msg_Warn( p_adec->p_fifo,
                  "invalid channels count(%d)", faad_frame.channels );
        return;
    }
    if( faad_frame.samples <= 0 )
    {
        msg_Warn( p_adec->p_fifo, "decoded zero sample !" );
        return;
    }

#if 0
    msg_Dbg( p_adec->p_fifo,
             "decoded frame samples:%d, channels:%d, consumed:%d",
             faad_frame.samples,
             faad_frame.channels,
             faad_frame.bytesconsumed );
#endif

    /* **** Now we can output these samples **** */
    
    /* **** First check if we have a valid output **** */
    if( ( !p_adec->p_aout_input )||
        ( p_adec->output_format.i_channels !=
             pi_channels_maps[faad_frame.channels] ) )
    {
        if( p_adec->p_aout_input )
        {
            /* **** Delete the old **** */
            aout_DecDelete( p_adec->p_aout, p_adec->p_aout_input );
        }

        /* **** Create a new audio output **** */
        p_adec->output_format.i_channels = 
                pi_channels_maps[faad_frame.channels];
        aout_DateInit( &p_adec->date, p_adec->output_format.i_rate );
        p_adec->p_aout_input = aout_DecNew( p_adec->p_fifo,
                                            &p_adec->p_aout,
                                            &p_adec->output_format );
    }

    if( !p_adec->p_aout_input )
    {
        msg_Err( p_adec->p_fifo, "cannot create aout" );
        return;
    }
    
    if( p_adec->pts != 0 && p_adec->pts != aout_DateGet( &p_adec->date ) )
    {
        aout_DateSet( &p_adec->date, p_adec->pts );
    }
    else if( !aout_DateGet( &p_adec->date ) )
    {
        return;
    }

    p_aout_buffer = aout_DecNewBuffer( p_adec->p_aout, 
                                       p_adec->p_aout_input,
                                       faad_frame.samples / faad_frame.channels );
    if( !p_aout_buffer )
    {
        msg_Err( p_adec->p_fifo, "cannot get aout buffer" );
        p_adec->p_fifo->b_error = 1;
        return;
    }
    p_aout_buffer->start_date = aout_DateGet( &p_adec->date );
    p_aout_buffer->end_date = aout_DateIncrement( &p_adec->date,
                                                  faad_frame.samples /
                                                      faad_frame.channels );
    memcpy( p_aout_buffer->p_buffer,
            p_faad_buffer,
            p_aout_buffer->i_nb_bytes );

    aout_DecPlay( p_adec->p_aout, p_adec->p_aout_input, p_aout_buffer );
}


/*****************************************************************************
 * EndThread : faad decoder thread destruction
 *****************************************************************************/
static void EndThread (adec_thread_t *p_adec)
{
    if( p_adec->p_aout_input )
    {
        aout_DecDelete( p_adec->p_aout, p_adec->p_aout_input );
    }

    if( p_adec->p_handle )
    {
        faacDecClose( p_adec->p_handle );
    }

    FREE( p_adec->format.p_data );
    FREE( p_adec->p_buffer );

    msg_Dbg( p_adec->p_fifo, "faad decoder closed" );
        
    free( p_adec );
}

