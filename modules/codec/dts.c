/*****************************************************************************
 * dts.c: DTS basic parser
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: dts.c,v 1.4 2003/09/02 20:19:25 gbazin Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>                                              /* memcpy() */
#include <fcntl.h>

#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include <vlc/input.h>
#include <vlc/aout.h>
#include <vlc/sout.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#define DTS_HEADER_SIZE 10

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
    int     i_state;

    uint8_t p_header[DTS_HEADER_SIZE];
    int     i_header;

    mtime_t pts;

    int     i_frame_size;

    /*
     * Decoder output properties
     */
    aout_instance_t *     p_aout;                                  /* opaque */
    aout_input_t *        p_aout_input;                            /* opaque */
    audio_sample_format_t aout_format;
    aout_buffer_t *       p_aout_buffer; /* current aout buffer being filled */
    /* This is very hacky. For DTS over S/PDIF we apparently need to send
     * 3 frames at a time. This should likely be moved to the output stage. */
    int                   i_frames_in_buf;

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
    int                   i_out_buffer;         /* position in output buffer */
    audio_date_t          end_date;
};

enum {

    STATE_NOSYNC,
    STATE_PARTIAL_SYNC,
    STATE_SYNC,
    STATE_HEADER,
    STATE_DATA
};

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  OpenPacketizer( vlc_object_t * );

static int  InitDecoder   ( decoder_t * );
static int  RunDecoder    ( decoder_t *, block_t * );
static int  EndDecoder    ( decoder_t * );

static int  SyncInfo      ( const byte_t *, unsigned int *, unsigned int *,
                            unsigned int *, unsigned int *, unsigned int * );

static int GetOutBuffer ( decoder_t *, uint8_t ** );
static int GetAoutBuffer( decoder_t *, aout_buffer_t ** );
static int GetSoutBuffer( decoder_t *, sout_buffer_t ** );
static int SendOutBuffer( decoder_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("DTS parser") );
    set_capability( "decoder", 100 );
    set_callbacks( OpenDecoder, NULL );

    add_submodule();
    set_description( _("DTS audio packetizer") );
    set_capability( "packetizer", 10 );
    set_callbacks( OpenPacketizer, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    if( p_dec->p_fifo->i_fourcc != VLC_FOURCC('d','t','s',' ')
         && p_dec->p_fifo->i_fourcc != VLC_FOURCC('d','t','s','b') )
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
    p_dec->p_sys->i_out_buffer = 0;
    aout_DateSet( &p_dec->p_sys->end_date, 0 );

    p_dec->p_sys->p_aout = NULL;
    p_dec->p_sys->p_aout_input = NULL;
    p_dec->p_sys->p_aout_buffer = NULL;
    p_dec->p_sys->aout_format.i_format = VLC_FOURCC('d','t','s',' ');

    p_dec->p_sys->p_sout_input = NULL;
    p_dec->p_sys->p_sout_buffer = NULL;
    p_dec->p_sys->sout_format.i_cat = AUDIO_ES;
    p_dec->p_sys->sout_format.i_fourcc = VLC_FOURCC('d','t','s',' ');

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
    int i, i_block_pos = 0;
    mtime_t i_pts = p_block->i_pts;

    while( i_block_pos < p_block->i_buffer )
    {
        switch( p_sys->i_state )
        {
        case STATE_NOSYNC:
            /* Look for sync dword - should be 0x7ffe8001 */
            while( i_block_pos < p_block->i_buffer &&
                   p_block->p_buffer[i_block_pos] != 0x7f )
            {
                i_block_pos++;
            }

            if( i_block_pos < p_block->i_buffer )
            {
                p_sys->i_state = STATE_PARTIAL_SYNC;
                i_block_pos++;
                p_sys->p_header[0] = 0x7f;
                p_sys->i_header = 1;
                break;
            }
            break;

        case STATE_PARTIAL_SYNC:
            /* Get the full 4 sync bytes */
            if( p_sys->i_header < 4 )
            {
                int i_size = __MIN( 4 - p_sys->i_header,
                                    p_block->i_buffer - i_block_pos );

                memcpy( p_sys->p_header + p_sys->i_header,
                        p_block->p_buffer + i_block_pos, i_size );
                i_block_pos += i_size;
                p_sys->i_header += i_size;
            }

            if( p_sys->i_header < 4 )
                break;

            if( p_sys->p_header[0] == 0x7f && p_sys->p_header[1] == 0xfe &&
                p_sys->p_header[2] == 0x80 && p_sys->p_header[3] == 0x01 )
            {
                p_sys->i_state = STATE_SYNC;
            }
            else
            {
                for( i = 1; i < 4; i++ )
                {
                    if( p_sys->p_header[i] == 0x7f ) break;
                }

                if( p_sys->p_header[i] == 0x7f )
                {
                    /* Potential new sync */
                    p_sys->i_header -= i;
                    memmove( p_sys->p_header, &p_sys->p_header[i],
                             p_sys->i_header );
                    break;
                }

                /* retry to sync*/
                p_sys->i_state = STATE_NOSYNC;
            }
            break;

        case STATE_SYNC:
            /* New frame, set the Presentation Time Stamp */
            p_sys->pts = i_pts; i_pts = 0;
            if( p_sys->pts != 0 &&
                p_sys->pts != aout_DateGet( &p_sys->end_date ) )
            {
                aout_DateSet( &p_sys->end_date, p_sys->pts );
            }
            p_sys->i_state = STATE_HEADER;
            break;

        case STATE_HEADER:
            /* Get DTS frame header (DTS_HEADER_SIZE bytes) */
            if( p_sys->i_header < DTS_HEADER_SIZE )
            {
                int i_size = __MIN( DTS_HEADER_SIZE - p_sys->i_header,
                                    p_block->i_buffer - i_block_pos );

                memcpy( p_sys->p_header + p_sys->i_header,
                        p_block->p_buffer + i_block_pos, i_size );
                i_block_pos += i_size;
                p_sys->i_header += i_size;
            }

            if( p_sys->i_header < DTS_HEADER_SIZE )
                break;

            if( GetOutBuffer( p_dec, &p_sys->p_out_buffer )
                != VLC_SUCCESS )
            {
                block_Release( p_block );
                return VLC_EGENERIC;
            }

            if( !p_sys->p_out_buffer )
            {
                p_sys->i_state = STATE_NOSYNC;
                break;
            }

            memcpy( p_sys->p_out_buffer, p_sys->p_header, DTS_HEADER_SIZE );
            p_sys->i_out_buffer = DTS_HEADER_SIZE;
            p_sys->i_state = STATE_DATA;
            break;

        case STATE_DATA:
            /* Copy the whole DTS frame into the aout buffer */
            if( p_sys->i_out_buffer < p_sys->i_frame_size )
            {
                int i_size = __MIN( p_sys->i_frame_size - p_sys->i_out_buffer,
                                    p_block->i_buffer - i_block_pos );

                memcpy( p_sys->p_out_buffer + p_sys->i_out_buffer,
                        p_block->p_buffer + i_block_pos, i_size );
                i_block_pos += i_size;
                p_sys->i_out_buffer += i_size;
            }

            if( p_sys->i_out_buffer < p_sys->i_frame_size )
                break; /* Need more data */

            SendOutBuffer( p_dec );

            p_sys->i_state = STATE_NOSYNC;
            break;
        }
    }

    block_Release( p_block );
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

    free( p_dec->p_sys );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * GetOutBuffer:
 *****************************************************************************/
static int GetOutBuffer ( decoder_t *p_dec, uint8_t **pp_out_buffer )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_ret;

    if( p_sys->b_packetizer )
    {
        i_ret= GetSoutBuffer( p_dec, &p_sys->p_sout_buffer );
        *pp_out_buffer =
            p_sys->p_sout_buffer ? p_sys->p_sout_buffer->p_buffer : NULL;
    }
    else
    {
        i_ret = GetAoutBuffer( p_dec, &p_sys->p_aout_buffer );
        if( p_sys->i_frames_in_buf == 1 )
            *pp_out_buffer = p_sys->p_aout_buffer ?
                p_sys->p_aout_buffer->p_buffer : NULL;
	else
            *pp_out_buffer = p_sys->p_aout_buffer ?
                p_sys->p_aout_buffer->p_buffer + p_sys->i_frame_size *
                (p_sys->i_frames_in_buf - 1) : NULL;
    }

    return i_ret;
}

/*****************************************************************************
 * GetAoutBuffer:
 *****************************************************************************/
static int GetAoutBuffer( decoder_t *p_dec, aout_buffer_t **pp_buffer )
{
    int i_bit_rate;
    unsigned int i_frame_length, i_rate, i_channels, i_channels_conf;

    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Check if frame is valid and get frame info */
    p_sys->i_frame_size = SyncInfo( p_sys->p_header,
                                    &i_channels, &i_channels_conf,
                                    &i_rate, &i_bit_rate, &i_frame_length );

    if( !p_sys->i_frame_size )
    {
        msg_Warn( p_dec, "dts syncinfo failed" );
        *pp_buffer = NULL;
        return VLC_SUCCESS;
    }

    if( p_sys->p_aout_input != NULL && ( p_sys->aout_format.i_rate != i_rate
        || p_sys->aout_format.i_original_channels != i_channels_conf
        || (int)p_sys->aout_format.i_bytes_per_frame != p_sys->i_frame_size ) )
    {
        /* Parameters changed - this should not happen. */
        aout_DecDelete( p_sys->p_aout, p_sys->p_aout_input );
        p_sys->p_aout_input = NULL;
    }

    /* Creating the audio input if not created yet. */
    if( p_sys->p_aout_input == NULL )
    {
        p_sys->aout_format.i_rate = i_rate;
        p_sys->aout_format.i_original_channels = i_channels_conf;
        p_sys->aout_format.i_physical_channels
            = i_channels_conf & AOUT_CHAN_PHYSMASK;
        p_sys->aout_format.i_bytes_per_frame = p_sys->i_frame_size;
        p_sys->aout_format.i_frame_length = i_frame_length;
        aout_DateInit( &p_sys->end_date, i_rate );
        aout_DateSet( &p_sys->end_date, p_sys->pts );
        p_sys->i_frames_in_buf = 3;
        p_sys->p_aout_input = aout_DecNew( p_dec,
                                           &p_sys->p_aout,
                                           &p_sys->aout_format );

        if ( p_sys->p_aout_input == NULL )
        {
            *pp_buffer = NULL;
            return VLC_SUCCESS;
        }
    }

    if( !aout_DateGet( &p_sys->end_date ) )
    {
        /* We've just started the stream, wait for the first PTS. */
        *pp_buffer = NULL;
        return VLC_SUCCESS;
    }

    if( p_sys->i_frames_in_buf == 3 )
    {
        p_sys->i_frames_in_buf = 0;
        *pp_buffer = aout_DecNewBuffer( p_sys->p_aout, p_sys->p_aout_input,
                                        i_frame_length * 3 );
        if( *pp_buffer == NULL )
        {
            return VLC_SUCCESS;
        }

        (*pp_buffer)->start_date = aout_DateGet( &p_sys->end_date );
        (*pp_buffer)->end_date =
             aout_DateIncrement( &p_sys->end_date, i_frame_length * 3 );
    }

    p_sys->i_frames_in_buf++;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * GetSoutBuffer:
 *****************************************************************************/
static int GetSoutBuffer( decoder_t *p_dec, sout_buffer_t **pp_buffer )
{
    int i_bit_rate;
    unsigned int i_frame_length, i_rate, i_channels, i_channels_conf;

    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Check if frame is valid and get frame info */
    p_sys->i_frame_size = SyncInfo( p_sys->p_header,
                                    &i_channels, &i_channels_conf,
                                    &i_rate, &i_bit_rate, &i_frame_length );

    if( !p_sys->i_frame_size )
    {
        msg_Warn( p_dec, "dts syncinfo failed" );
        *pp_buffer = NULL;
        return VLC_SUCCESS;
    }

    if( p_sys->p_sout_input != NULL &&
        ( p_sys->sout_format.i_sample_rate != (int)i_rate
          || p_sys->sout_format.i_channels != (int)i_channels ) )
    {
        /* Parameters changed - this should not happen. */
    }

    /* Creating the sout input if not created yet. */
    if( p_sys->p_sout_input == NULL )
    {
        p_sys->sout_format.i_sample_rate = i_rate;
        p_sys->sout_format.i_channels    = i_channels;
        p_sys->sout_format.i_block_align = 0;
        p_sys->sout_format.i_bitrate     = i_bit_rate;
        p_sys->sout_format.i_extra_data  = 0;
        p_sys->sout_format.p_extra_data  = NULL;

        aout_DateInit( &p_sys->end_date, i_rate );
        aout_DateSet( &p_sys->end_date, p_sys->pts );

        p_sys->p_sout_input = sout_InputNew( p_dec,
                                             &p_sys->sout_format );

        if( p_sys->p_sout_input == NULL )
        {
            msg_Err( p_dec, "cannot add a new stream" );
            *pp_buffer = NULL;
            return VLC_EGENERIC;
        }
        msg_Info( p_dec, "DTS channels:%d samplerate:%d bitrate:%d",
                  i_channels, i_rate, i_bit_rate );
    }

    if( !aout_DateGet( &p_sys->end_date ) )
    {
        /* We've just started the stream, wait for the first PTS. */
        *pp_buffer = NULL;
        return VLC_SUCCESS;
    }

    *pp_buffer = sout_BufferNew( p_sys->p_sout_input->p_sout,
                                 p_sys->i_frame_size );
    if( *pp_buffer == NULL )
    {
        return VLC_SUCCESS;
    }

    (*pp_buffer)->i_pts =
        (*pp_buffer)->i_dts = aout_DateGet( &p_sys->end_date );

    (*pp_buffer)->i_length =
        aout_DateIncrement( &p_sys->end_date, i_frame_length )
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
    else if( p_sys->i_frames_in_buf == 3 )
    {
        /* We have all we need, send the buffer to the aout core. */
        aout_DecPlay( p_sys->p_aout, p_sys->p_aout_input,
                      p_sys->p_aout_buffer );
        p_sys->p_aout_buffer = NULL;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SyncInfo: parse DTS sync info
 *****************************************************************************/
static int SyncInfo( const byte_t * p_buf,
                     unsigned int * pi_channels,
                     unsigned int * pi_channels_conf,
                     unsigned int * pi_sample_rate,
                     unsigned int * pi_bit_rate,
                     unsigned int * pi_frame_length )
{
    unsigned int i_bit_rate;
    unsigned int i_audio_mode;
    unsigned int i_sample_rate;
    unsigned int i_frame_size;
    unsigned int i_frame_length;

    static const unsigned int ppi_dts_samplerate[] =
    {
        0, 8000, 16000, 32000, 64000, 128000,
        11025, 22050, 44010, 88020, 176400,
        12000, 24000, 48000, 96000, 192000
    };

    static const unsigned int ppi_dts_bitrate[] =
    {
        32000, 56000, 64000, 96000, 112000, 128000,
        192000, 224000, 256000, 320000, 384000,
        448000, 512000, 576000, 640000, 768000,
        896000, 1024000, 1152000, 1280000, 1344000,
        1408000, 1411200, 1472000, 1536000, 1920000,
        2048000, 3072000, 3840000, 4096000, 0, 0
    };

    if( (p_buf[0] != 0x7f) || (p_buf[1] != 0xfe) ||
        (p_buf[2] != 0x80) || (p_buf[3] != 0x01) )
    {
        return( 0 );
    }

    i_frame_length = (p_buf[4] & 0x01) << 6 | (p_buf[5] >> 2);
    i_frame_size = (p_buf[5] & 0x03) << 12 | (p_buf[6] << 4) |
                   (p_buf[7] >> 4);

    i_audio_mode = (p_buf[7] & 0x0f) << 2 | (p_buf[8] >> 6);
    i_sample_rate = (p_buf[8] >> 2) & 0x0f;
    i_bit_rate = (p_buf[8] & 0x03) << 3 | ((p_buf[9] >> 5) & 0x07);

    switch( i_audio_mode )
    {
        case 0x0:
            /* Mono */
            *pi_channels_conf = AOUT_CHAN_CENTER;
            break;
        case 0x1:
            /* Dual-mono = stereo + dual-mono */
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                           AOUT_CHAN_DUALMONO;
            break;
        case 0x2:
        case 0x3:
        case 0x4:
            /* Stereo */
            *pi_channels = 2;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
            break;
        case 0x5:
            /* 3F */
            *pi_channels = 3;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_CENTER;
            break;
        case 0x6:
            /* 2F/LFE */
            *pi_channels = 3;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_LFE;
            break;
        case 0x7:
            /* 3F/LFE */
            *pi_channels = 4;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_CENTER | AOUT_CHAN_LFE;
            break;
        case 0x8:
            /* 2F2R */
            *pi_channels = 4;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
            break;
        case 0x9:
            /* 3F2R */
            *pi_channels = 5;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT |
                                AOUT_CHAN_REARRIGHT;
            break;
        case 0xA:
        case 0xB:
            /* 2F2M2R */
            *pi_channels = 6;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT |
                                AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
            break;
        case 0xC:
            /* 3F2M2R */
            *pi_channels = 7;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_CENTER | AOUT_CHAN_MIDDLELEFT |
                                AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_REARLEFT |
                                AOUT_CHAN_REARRIGHT;
            break;
        case 0xD:
        case 0xE:
            /* 3F2M2R/LFE */
            *pi_channels = 8;
            *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
                                AOUT_CHAN_CENTER | AOUT_CHAN_MIDDLELEFT |
                                AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_REARLEFT |
                                AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE;
            break;

        default:
            if( i_audio_mode <= 63 )
            {
                /* User defined */
                *pi_channels = 0;
                *pi_channels_conf = 0; 
            }
            else
            {
                return( 0 );
            }
            break;
    }

    if( i_sample_rate >= sizeof( ppi_dts_samplerate ) /
                         sizeof( ppi_dts_samplerate[0] ) )
    {
        return( 0 );
    }

    *pi_sample_rate = ppi_dts_samplerate[ i_sample_rate ];

    if( i_bit_rate >= sizeof( ppi_dts_bitrate ) /
                      sizeof( ppi_dts_bitrate[0] ) )
    {
        return( 0 );
    }

    *pi_bit_rate = ppi_dts_bitrate[ i_bit_rate ];

    *pi_frame_length = (i_frame_length + 1) * 32;

    return( i_frame_size + 1 );
}
