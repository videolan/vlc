/*****************************************************************************
 * mpeg_audio.c: parse MPEG audio sync info and packetize the stream
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN
 * $Id: mpeg_audio.c,v 1.18 2003/10/04 12:04:06 gbazin Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include <vlc/input.h>
#include <vlc/aout.h>
#include <vlc/sout.h>

#include "vlc_block_helper.h"

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /* Module mode */
    vlc_bool_t b_packetizer;

    /*
     * Input properties
     */
    int        i_state;

    block_t *p_chain;
    block_bytestream_t bytestream;

    /*
     * Decoder output properties
     */
    aout_instance_t *     p_aout;                                  /* opaque */
    aout_input_t *        p_aout_input;                            /* opaque */
    audio_sample_format_t aout_format;
    aout_buffer_t *       p_aout_buffer; /* current aout buffer being filled */

    /*
     * Packetizer output properties
     */
    sout_packetizer_input_t *p_sout_input;
    sout_format_t           sout_format;
    sout_buffer_t *         p_sout_buffer;            /* current sout buffer */

    /*
     * Common properties
     */
    uint8_t               *p_out_buffer;                    /* output buffer */
    audio_date_t          end_date;
    unsigned int          i_current_layer;

    mtime_t pts;

    int i_frame_size, i_free_frame_size;
    unsigned int i_channels_conf, i_channels;
    unsigned int i_rate, i_max_frame_size, i_frame_length;
    unsigned int i_layer, i_bit_rate;
};

enum {

    STATE_NOSYNC,
    STATE_SYNC,
    STATE_HEADER,
    STATE_NEXT_SYNC,
    STATE_DATA
};

#define MAX_FRAME_SIZE 10000
/* This isn't the place to put mad-specific stuff. However, it makes the
 * mad plug-in's life much easier if we put 8 extra bytes at the end of the
 * buffer, because that way it doesn't have to copy the aout_buffer_t to a
 * bigger buffer. This has no implication on other plug-ins, and we only
 * lose 8 bytes per frame. --Meuuh */
#define MAD_BUFFER_GUARD 8
#define MPGA_HEADER_SIZE 4

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int OpenDecoder   ( vlc_object_t * );
static int OpenPacketizer( vlc_object_t * );

static int InitDecoder   ( decoder_t * );
static int RunDecoder    ( decoder_t *, block_t * );
static int EndDecoder    ( decoder_t * );

static int GetOutBuffer ( decoder_t *, uint8_t ** );
static int GetAoutBuffer( decoder_t *, aout_buffer_t ** );
static int GetSoutBuffer( decoder_t *, sout_buffer_t ** );
static int SendOutBuffer( decoder_t * );

static int SyncInfo( uint32_t i_header, unsigned int * pi_channels,
                     unsigned int * pi_sample_rate, unsigned int * pi_bit_rate,
                     unsigned int * pi_frame_length,
                     unsigned int * pi_current_frame_length,
                     unsigned int * pi_layer );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("MPEG audio layer I/II/III parser") );
    set_capability( "decoder", 100 );
    set_callbacks( OpenDecoder, NULL );

    add_submodule();
    set_description( _("MPEG audio layer I/II/III packetizer") );
    set_capability( "packetizer", 10 );
    set_callbacks( OpenPacketizer, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    if( p_dec->p_fifo->i_fourcc != VLC_FOURCC( 'm', 'p', 'g', 'a') )
    {
        return VLC_EGENERIC;
    }

    p_dec->pf_init = InitDecoder;
    p_dec->pf_decode = RunDecoder;
    p_dec->pf_end = EndDecoder;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return VLC_EGENERIC;
    }
    p_dec->p_sys->b_packetizer = VLC_FALSE;

    return VLC_SUCCESS;
}

static int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    int i_ret = OpenDecoder( p_this );

    if( i_ret == VLC_SUCCESS ) p_dec->p_sys->b_packetizer = VLC_TRUE;

    return i_ret;
}

/*****************************************************************************
 * InitDecoder: Initalize the decoder
 *****************************************************************************/
static int InitDecoder( decoder_t *p_dec )
{
    p_dec->p_sys->i_state = STATE_NOSYNC;

    p_dec->p_sys->p_out_buffer = NULL;
    aout_DateSet( &p_dec->p_sys->end_date, 0 );

    p_dec->p_sys->p_aout = NULL;
    p_dec->p_sys->p_aout_input = NULL;
    p_dec->p_sys->p_aout_buffer = NULL;
    p_dec->p_sys->aout_format.i_format = VLC_FOURCC('m','p','g','a');

    p_dec->p_sys->p_sout_input = NULL;
    p_dec->p_sys->p_sout_buffer = NULL;
    p_dec->p_sys->sout_format.i_cat = AUDIO_ES;
    p_dec->p_sys->sout_format.i_fourcc = VLC_FOURCC('m','p','g','a');

    /* Start with the inimum size for a free bitrate frame */
    p_dec->p_sys->i_free_frame_size = MPGA_HEADER_SIZE;

    p_dec->p_sys->p_chain = NULL;

    return VLC_SUCCESS;
}

/****************************************************************************
 * RunDecoder: the whole thing
 ****************************************************************************
 * This function is called just after the thread is launched.
 ****************************************************************************/
static int RunDecoder( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t p_header[MAD_BUFFER_GUARD];
    uint32_t i_header;

    if( !aout_DateGet( &p_sys->end_date ) && !p_block->i_pts )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( p_block );
        return VLC_SUCCESS;
    }

    if( p_sys->p_chain )
    {
        block_ChainAppend( &p_sys->p_chain, p_block );
    }
    else
    {
        block_ChainAppend( &p_sys->p_chain, p_block );
        p_sys->bytestream = block_BytestreamInit( p_dec, p_sys->p_chain, 0 );
    }

    while( 1 )
    {
        switch( p_sys->i_state )
        {

        case STATE_NOSYNC:
            while( block_PeekBytes( &p_sys->bytestream, p_header, 2 )
                   == VLC_SUCCESS )
            {
                /* Look for sync word - should be 0xffe */
                if( p_header[0] == 0xff && (p_header[1] & 0xe0) == 0xe0 )
                {
                    p_sys->i_state = STATE_SYNC;
                    break;
                }
                block_SkipByte( &p_sys->bytestream );
            }
            if( p_sys->i_state != STATE_SYNC )
            {
                block_ChainRelease( p_sys->p_chain );
                p_sys->p_chain = NULL;

                /* Need more data */
                return VLC_SUCCESS;
            }

        case STATE_SYNC:
            /* New frame, set the Presentation Time Stamp */
            p_sys->pts = p_sys->bytestream.p_block->i_pts;
            if( p_sys->pts != 0 &&
                p_sys->pts != aout_DateGet( &p_sys->end_date ) )
            {
                aout_DateSet( &p_sys->end_date, p_sys->pts );
            }
            p_sys->i_state = STATE_HEADER;
            break;

        case STATE_HEADER:
            /* Get MPGA frame header (MPGA_HEADER_SIZE bytes) */
            if( block_PeekBytes( &p_sys->bytestream, p_header,
                                 MPGA_HEADER_SIZE ) != VLC_SUCCESS )
            {
                /* Need more data */
                return VLC_SUCCESS;
            }

            /* Build frame header */
            i_header = (p_header[0]<<24)|(p_header[1]<<16)|(p_header[2]<<8)
                       |p_header[3];

            /* Check if frame is valid and get frame info */
            p_sys->i_frame_size = SyncInfo( i_header,
                                            &p_sys->i_channels_conf,
                                            &p_sys->i_rate,
                                            &p_sys->i_bit_rate,
                                            &p_sys->i_frame_length,
                                            &p_sys->i_frame_size,
                                            &p_sys->i_layer );

            if( p_sys->i_frame_size == -1 )
            {
                msg_Dbg( p_dec, "emulated start code" );
                block_SkipByte( &p_sys->bytestream );
                p_sys->i_state = STATE_NOSYNC;
                break;
            }

            if( p_sys->i_bit_rate == 0 )
            {
                /* Free birate, but 99% emulated startcode :( */
                if( p_dec->p_sys->i_free_frame_size == MPGA_HEADER_SIZE )
                {
                    msg_Dbg( p_dec, "free bitrate mode");
                }
                p_sys->i_frame_size = p_sys->i_free_frame_size;
            }

            p_sys->i_state = STATE_NEXT_SYNC;

        case STATE_NEXT_SYNC:
            /* TODO: If p_block == NULL, flush the buffer without checking the
             * next sync word */

            /* Check if next expected frame contains the sync word */
            if( block_PeekOffsetBytes( &p_sys->bytestream,
                                       p_sys->i_frame_size, p_header,
                                       MAD_BUFFER_GUARD ) != VLC_SUCCESS )
            {
                /* Need more data */
                return VLC_SUCCESS;
            }

            if( p_header[0] == 0xff && (p_header[1] & 0xe0) == 0xe0 )
            {
                /* Startcode is fine, let's try the header as an extra check */
                int i_next_frame_size;
                unsigned int i_next_channels, i_next_rate, i_next_bit_rate;
                unsigned int i_next_frame_length, i_next_max_frame_size;
                unsigned int i_next_layer;

                /* Build frame header */
                i_header = (p_header[0]<<24)|(p_header[1]<<16)|(p_header[2]<<8)
                           |p_header[3];

                i_next_frame_size = SyncInfo( i_header,
                                              &i_next_channels,
                                              &i_next_rate,
                                              &i_next_bit_rate,
                                              &i_next_frame_length,
                                              &i_next_max_frame_size,
                                              &i_next_layer );

                if( i_next_frame_size == -1 )
                {
                    msg_Dbg( p_dec, "emulated start code on next frame" );
                    block_SkipByte( &p_sys->bytestream );
                    p_sys->i_state = STATE_NOSYNC;
                    break;
                }

                /* Free bitrate only */
                if( p_sys->i_bit_rate == 0 )
                {
                    if( i_next_bit_rate != 0 )
                    {
                        p_sys->i_frame_size++;
                        break;
                    }

                    if( (unsigned int)p_sys->i_frame_size >
                        p_sys->i_max_frame_size )
                    {
                        msg_Dbg( p_dec, "frame too big %d > %d "
                                 "(emulated startcode ?)", p_sys->i_frame_size,
                                 p_sys->i_max_frame_size );
                        block_SkipByte( &p_sys->bytestream );
                        p_sys->i_state = STATE_NOSYNC;
                        p_sys->i_free_frame_size = MPGA_HEADER_SIZE;
                        break;
                    }
                }

                /* Check info is in sync with previous one */
                if( i_next_channels != p_sys->i_channels_conf ||
                    i_next_rate != p_sys->i_rate ||
                    i_next_layer != p_sys->i_layer ||
                    i_next_frame_length != p_sys->i_frame_length )
                {
                    /* Free bitrate only */
                    if( p_sys->i_bit_rate == 0 )
                    {
                        p_sys->i_frame_size++;
                        break;
                    }

                    msg_Dbg( p_dec, "parameters changed unexpectedly "
                             "(emulated startcode ?)" );
                    block_SkipByte( &p_sys->bytestream );
                    p_sys->i_state = STATE_NOSYNC;
                    break;
                }
            }
            else
            {
                msg_Dbg( p_dec, "emulated startcode "
                         "(no startcode on following frame)" );
                p_sys->i_state = STATE_NOSYNC;
                block_SkipByte( &p_sys->bytestream );
                break;
            }

            if( GetOutBuffer( p_dec, &p_sys->p_out_buffer ) != VLC_SUCCESS )
            {
                return VLC_EGENERIC;
            }

            /* Free bitrate only */
            if( p_sys->i_bit_rate == 0 )
            {
                p_sys->i_free_frame_size = p_sys->i_frame_size;
            }

            p_sys->i_state = STATE_DATA;

        case STATE_DATA:
            /* Copy the whole frame into the buffer */
            if( block_GetBytes( &p_sys->bytestream, p_sys->p_out_buffer,
                                p_sys->i_frame_size ) != VLC_SUCCESS )
            {
                /* Need more data */
                return VLC_SUCCESS;
            }

            p_sys->p_chain = block_BytestreamFlush( &p_sys->bytestream );

            /* Get beginning of next frame for libmad */
            if( !p_sys->b_packetizer )
            {
                memcpy( p_sys->p_out_buffer + p_sys->i_frame_size,
                        p_header, MAD_BUFFER_GUARD );
            }

            SendOutBuffer( p_dec );
            p_sys->i_state = STATE_NOSYNC;

            /* Make sure we don't reuse the same pts twice */
            if( p_sys->pts == p_sys->bytestream.p_block->i_pts )
                p_sys->pts = p_sys->bytestream.p_block->i_pts = 0;
        }
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * GetOutBuffer:
 *****************************************************************************/
static int GetOutBuffer( decoder_t *p_dec, uint8_t **pp_out_buffer )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_ret;

    if( p_sys->b_packetizer )
    {
        i_ret = GetSoutBuffer( p_dec, &p_sys->p_sout_buffer );
        *pp_out_buffer =
            p_sys->p_sout_buffer ? p_sys->p_sout_buffer->p_buffer : NULL;
    }
    else
    {
        i_ret = GetAoutBuffer( p_dec, &p_sys->p_aout_buffer );
        *pp_out_buffer =
            p_sys->p_aout_buffer ? p_sys->p_aout_buffer->p_buffer : NULL;
    }

    return i_ret;
}

/*****************************************************************************
 * GetAoutBuffer:
 *****************************************************************************/
static int GetAoutBuffer( decoder_t *p_dec, aout_buffer_t **pp_buffer )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_aout_input != NULL &&
        ( p_sys->aout_format.i_rate != p_sys->i_rate
        || p_sys->aout_format.i_original_channels != p_sys->i_channels_conf
        || (int)p_sys->aout_format.i_bytes_per_frame !=
           p_sys->i_max_frame_size + MAD_BUFFER_GUARD
        || p_sys->aout_format.i_frame_length != p_sys->i_frame_length
        || p_sys->i_current_layer != p_sys->i_layer ) )
    {
        /* Parameters changed - this should not happen. */
        aout_DecDelete( p_sys->p_aout, p_sys->p_aout_input );
        p_sys->p_aout_input = NULL;
    }

    /* Creating the audio input if not created yet. */
    if( p_sys->p_aout_input == NULL )
    {
        p_sys->i_current_layer = p_sys->i_layer;
        if( p_sys->i_layer == 3 )
        {
            p_sys->aout_format.i_format = VLC_FOURCC('m','p','g','3');
        }
        else
        {
            p_sys->aout_format.i_format = VLC_FOURCC('m','p','g','a');
        }

        p_sys->aout_format.i_rate = p_sys->i_rate;
        p_sys->aout_format.i_original_channels = p_sys->i_channels_conf;
        p_sys->aout_format.i_physical_channels
            = p_sys->i_channels_conf & AOUT_CHAN_PHYSMASK;
        p_sys->aout_format.i_bytes_per_frame = p_sys->i_max_frame_size
            + MAD_BUFFER_GUARD;
        p_sys->aout_format.i_frame_length = p_sys->i_frame_length;
        aout_DateInit( &p_sys->end_date, p_sys->i_rate );
        aout_DateSet( &p_sys->end_date, p_sys->pts );
        p_sys->p_aout_input = aout_DecNew( p_dec,
                                           &p_sys->p_aout,
                                           &p_sys->aout_format );
        if( p_sys->p_aout_input == NULL )
        {
            *pp_buffer = NULL;
            return VLC_EGENERIC;
        }
    }

    *pp_buffer = aout_DecNewBuffer( p_sys->p_aout, p_sys->p_aout_input,
                                    p_sys->i_frame_length );
    if( *pp_buffer == NULL )
    {
        return VLC_EGENERIC;
    }

    (*pp_buffer)->start_date = aout_DateGet( &p_sys->end_date );
    (*pp_buffer)->end_date =
         aout_DateIncrement( &p_sys->end_date, p_sys->i_frame_length );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * GetSoutBuffer:
 *****************************************************************************/
static int GetSoutBuffer( decoder_t *p_dec, sout_buffer_t **pp_buffer )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_sout_input != NULL &&
        ( p_sys->sout_format.i_sample_rate != (int)p_sys->i_rate
          || p_sys->sout_format.i_channels != (int)p_sys->i_channels ) )
    {
        /* Parameters changed - this should not happen. */
    }

    /* Creating the sout input if not created yet. */
    if( p_sys->p_sout_input == NULL )
    {
        p_sys->sout_format.i_sample_rate = p_sys->i_rate;
        p_sys->sout_format.i_channels    = p_sys->i_channels;
        p_sys->sout_format.i_block_align = 0;
        p_sys->sout_format.i_bitrate     = p_sys->i_bit_rate;
        p_sys->sout_format.i_extra_data  = 0;
        p_sys->sout_format.p_extra_data  = NULL;

        aout_DateInit( &p_sys->end_date, p_sys->i_rate );
        aout_DateSet( &p_sys->end_date, p_sys->pts );

        p_sys->p_sout_input = sout_InputNew( p_dec, &p_sys->sout_format );
        if( p_sys->p_sout_input == NULL )
        {
            msg_Err( p_dec, "cannot add a new stream" );
            *pp_buffer = NULL;
            return VLC_EGENERIC;
        }
        msg_Info( p_dec, "A/52 channels:%d samplerate:%d bitrate:%d",
                  p_sys->i_channels, p_sys->i_rate, p_sys->i_bit_rate );
    }

    *pp_buffer = sout_BufferNew( p_sys->p_sout_input->p_sout,
                                 p_sys->i_frame_size );
    if( *pp_buffer == NULL )
    {
        return VLC_EGENERIC;
    }

    (*pp_buffer)->i_pts =
        (*pp_buffer)->i_dts = aout_DateGet( &p_sys->end_date );

    (*pp_buffer)->i_length =
        aout_DateIncrement( &p_sys->end_date, p_sys->i_frame_length )
        - (*pp_buffer)->i_pts;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SendOutBuffer:
 *****************************************************************************/
static int SendOutBuffer( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->b_packetizer )
    {
        sout_InputSendBuffer( p_sys->p_sout_input, p_sys->p_sout_buffer );
        p_sys->p_sout_buffer = NULL;
    }
    else
    {
        /* We have all we need, send the buffer to the aout core. */
        aout_DecPlay( p_sys->p_aout, p_sys->p_aout_input,
                      p_sys->p_aout_buffer );
        p_sys->p_aout_buffer = NULL;
    }

    p_sys->p_out_buffer = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * EndDecoder: clean up the decoder
 *****************************************************************************/
static int EndDecoder( decoder_t *p_dec )
{
    if( p_dec->p_sys->p_aout_input != NULL )
    {
        if( p_dec->p_sys->p_aout_buffer )
        {
            aout_DecDeleteBuffer( p_dec->p_sys->p_aout,
                                  p_dec->p_sys->p_aout_input,
                                  p_dec->p_sys->p_aout_buffer );
        }

        aout_DecDelete( p_dec->p_sys->p_aout, p_dec->p_sys->p_aout_input );
    }

    if( p_dec->p_sys->p_sout_input != NULL )
    {
        if( p_dec->p_sys->p_sout_buffer )
        {
            sout_BufferDelete( p_dec->p_sys->p_sout_input->p_sout,
                               p_dec->p_sys->p_sout_buffer );
        }

        sout_InputDelete( p_dec->p_sys->p_sout_input );
    }

    if( p_dec->p_sys->p_chain ) block_ChainRelease( p_dec->p_sys->p_chain );

    free( p_dec->p_sys );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SyncInfo: parse MPEG audio sync info
 *****************************************************************************/
static int SyncInfo( uint32_t i_header, unsigned int * pi_channels,
                     unsigned int * pi_sample_rate, unsigned int * pi_bit_rate,
                     unsigned int * pi_frame_length,
                     unsigned int * pi_frame_size, unsigned int * pi_layer )
{
    static const int pppi_mpegaudio_bitrate[2][3][16] =
    {
        {
            /* v1 l1 */
            { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384,
              416, 448, 0},
            /* v1 l2 */
            { 0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256,
              320, 384, 0},
            /* v1 l3 */
            { 0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224,
              256, 320, 0}
        },

        {
            /* v2 l1 */
            { 0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192,
              224, 256, 0},
            /* v2 l2 */
            { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128,
              144, 160, 0},
            /* v2 l3 */
            { 0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128,
              144, 160, 0}
        }
    };

    static const int ppi_mpegaudio_samplerate[2][4] = /* version 1 then 2 */
    {
        { 44100, 48000, 32000, 0 },
        { 22050, 24000, 16000, 0 }
    };

    int i_version, i_mode, i_emphasis;
    vlc_bool_t b_padding, b_mpeg_2_5;
    int i_current_frame_size = 0;
    int i_bitrate_index, i_samplerate_index;
    int i_max_bit_rate;

    b_mpeg_2_5  = 1 - ((i_header & 0x100000) >> 20);
    i_version   = 1 - ((i_header & 0x80000) >> 19);
    *pi_layer   = 4 - ((i_header & 0x60000) >> 17);
    /* CRC */
    i_bitrate_index = (i_header & 0xf000) >> 12;
    i_samplerate_index = (i_header & 0xc00) >> 10;
    b_padding   = (i_header & 0x200) >> 9;
    /* Extension */
    i_mode      = (i_header & 0xc0) >> 6;
    /* Modeext, copyright & original */
    i_emphasis  = i_header & 0x3;

    if( *pi_layer != 4 &&
        i_bitrate_index < 0x0f &&
        i_samplerate_index != 0x03 &&
        i_emphasis != 0x02 )
    {
        switch ( i_mode )
        {
        case 0: /* stereo */
        case 1: /* joint stereo */
            *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
            break;
        case 2: /* dual-mono */
            *pi_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                            | AOUT_CHAN_DUALMONO;
            break;
        case 3: /* mono */
            *pi_channels = AOUT_CHAN_CENTER;
            break;
        }
        *pi_bit_rate = pppi_mpegaudio_bitrate[i_version][*pi_layer-1][i_bitrate_index];
        i_max_bit_rate = pppi_mpegaudio_bitrate[i_version][*pi_layer-1][14];
        *pi_sample_rate = ppi_mpegaudio_samplerate[i_version][i_samplerate_index];

        if ( b_mpeg_2_5 )
        {
            *pi_sample_rate >>= 1;
        }

        switch( *pi_layer )
        {
        case 1:
            i_current_frame_size = ( 12000 * *pi_bit_rate /
                                     *pi_sample_rate + b_padding ) * 4;
            *pi_frame_size = ( 12000 * i_max_bit_rate /
                               *pi_sample_rate + 1 ) * 4;
            *pi_frame_length = 384;
            break;

        case 2:
            i_current_frame_size = 144000 * *pi_bit_rate /
                                   *pi_sample_rate + b_padding;
            *pi_frame_size = 144000 * i_max_bit_rate / *pi_sample_rate + 1;
            *pi_frame_length = 1152;
            break;

        case 3:
            i_current_frame_size = ( i_version ? 72000 : 144000 ) *
                                   *pi_bit_rate / *pi_sample_rate + b_padding;
            *pi_frame_size = ( i_version ? 72000 : 144000 ) *
                                 i_max_bit_rate / *pi_sample_rate + 1;
            *pi_frame_length = i_version ? 576 : 1152;
            break;

        default:
            break;
        }
    }
    else
    {
        return -1;
    }

    return i_current_frame_size;
}
