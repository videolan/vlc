/*****************************************************************************
 * a52.c: A/52 basic parser
 *****************************************************************************
 * Copyright (C) 2001-2002 VideoLAN
 * $Id: a52.c,v 1.23 2003/09/02 20:19:25 gbazin Exp $
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
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

#define A52_HEADER_SIZE 7

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

    uint8_t p_header[A52_HEADER_SIZE];
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

static int  SyncInfo      ( const byte_t *, int *, int *, int *,int * );

static int GetOutBuffer ( decoder_t *, uint8_t ** );
static int GetAoutBuffer( decoder_t *, aout_buffer_t ** );
static int GetSoutBuffer( decoder_t *, sout_buffer_t ** );
static int SendOutBuffer( decoder_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("A/52 parser") );
    set_capability( "decoder", 100 );
    set_callbacks( OpenDecoder, NULL );

    add_submodule();
    set_description( _("A/52 audio packetizer") );
    set_capability( "packetizer", 10 );
    set_callbacks( OpenPacketizer, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    if( p_dec->p_fifo->i_fourcc != VLC_FOURCC('a','5','2',' ')
         && p_dec->p_fifo->i_fourcc != VLC_FOURCC('a','5','2','b') )
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
    p_dec->p_sys->aout_format.i_format = VLC_FOURCC('a','5','2',' ');

    p_dec->p_sys->p_sout_input = NULL;
    p_dec->p_sys->p_sout_buffer = NULL;
    p_dec->p_sys->sout_format.i_cat = AUDIO_ES;
    p_dec->p_sys->sout_format.i_fourcc = VLC_FOURCC( 'a', '5', '2', ' ' );

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
    int i_block_pos = 0;
    mtime_t i_pts = p_block->i_pts;

    while( i_block_pos < p_block->i_buffer )
    {
        switch( p_sys->i_state )
        {
        case STATE_NOSYNC:
            /* Look for sync word - should be 0x0b77 */
            while( i_block_pos < p_block->i_buffer &&
                   p_block->p_buffer[i_block_pos] != 0x0b )
            {
                i_block_pos++;
            }

            if( i_block_pos < p_block->i_buffer )
            {
                p_sys->i_state = STATE_PARTIAL_SYNC;
                i_block_pos++;
                p_sys->p_header[0] = 0x0b;
                break;
            }
            break;

        case STATE_PARTIAL_SYNC:
            if( p_block->p_buffer[i_block_pos] == 0x77 )
            {
                p_sys->i_state = STATE_SYNC;
                i_block_pos++;
                p_sys->p_header[1] = 0x77;
                p_sys->i_header = 2;
            }
            else
            {
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
            /* Get A/52 frame header (A52_HEADER_SIZE bytes) */
            if( p_sys->i_header < A52_HEADER_SIZE )
            {
                int i_size = __MIN( A52_HEADER_SIZE - p_sys->i_header,
                                    p_block->i_buffer - i_block_pos );

                memcpy( p_sys->p_header + p_sys->i_header,
                        p_block->p_buffer + i_block_pos, i_size );
                i_block_pos += i_size;
                p_sys->i_header += i_size;
            }

            if( p_sys->i_header < A52_HEADER_SIZE )
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

            memcpy( p_sys->p_out_buffer, p_sys->p_header, A52_HEADER_SIZE );
            p_sys->i_out_buffer = A52_HEADER_SIZE;
            p_sys->i_state = STATE_DATA;
            break;

        case STATE_DATA:
            /* Copy the whole A52 frame into the aout buffer */
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
    int i_bit_rate;
    unsigned int i_rate, i_channels, i_channels_conf;

    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Check if frame is valid and get frame info */
    p_sys->i_frame_size = SyncInfo( p_sys->p_header,
                                    &i_channels, &i_channels_conf,
                                    &i_rate, &i_bit_rate );

    if( !p_sys->i_frame_size )
    {
        msg_Warn( p_dec, "a52 syncinfo failed" );
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
        p_sys->aout_format.i_frame_length = A52_FRAME_NB;
        aout_DateInit( &p_sys->end_date, i_rate );
        aout_DateSet( &p_sys->end_date, p_sys->pts );
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

    *pp_buffer = aout_DecNewBuffer( p_sys->p_aout, p_sys->p_aout_input,
                                    A52_FRAME_NB );
    if( *pp_buffer == NULL )
    {
        return VLC_SUCCESS;
    }

    (*pp_buffer)->start_date = aout_DateGet( &p_sys->end_date );
    (*pp_buffer)->end_date =
         aout_DateIncrement( &p_sys->end_date, A52_FRAME_NB );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * GetSoutBuffer:
 *****************************************************************************/
static int GetSoutBuffer( decoder_t *p_dec, sout_buffer_t **pp_buffer )
{
    int i_bit_rate;
    unsigned int i_rate, i_channels, i_channels_conf;

    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Check if frame is valid and get frame info */
    p_sys->i_frame_size = SyncInfo( p_sys->p_header,
                                    &i_channels, &i_channels_conf,
                                    &i_rate, &i_bit_rate );

    if( !p_sys->i_frame_size )
    {
        msg_Warn( p_dec, "a52 syncinfo failed" );
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
        msg_Info( p_dec, "A/52 channels:%d samplerate:%d bitrate:%d",
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
        aout_DateIncrement( &p_sys->end_date, A52_FRAME_NB )
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

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SyncInfo: parse A/52 sync info
 *****************************************************************************
 * This code is borrowed from liba52 by Aaron Holtzman & Michel Lespinasse,
 * since we don't want to oblige S/PDIF people to use liba52 just to get
 * their SyncInfo...
 *****************************************************************************/
static int SyncInfo( const byte_t * p_buf,
                     int * pi_channels, int * pi_channels_conf,
                     int * pi_sample_rate, int * pi_bit_rate )
{
    static const uint8_t halfrate[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3 };
    static const int rate[] = { 32,  40,  48,  56,  64,  80,  96, 112,
                                128, 160, 192, 224, 256, 320, 384, 448,
                                512, 576, 640 };
    static const uint8_t lfeon[8] = { 0x10, 0x10, 0x04, 0x04,
                                      0x04, 0x01, 0x04, 0x01 };
    int frmsizecod;
    int bitrate;
    int half;
    int acmod;

    if ((p_buf[0] != 0x0b) || (p_buf[1] != 0x77))        /* syncword */
        return 0;

    if (p_buf[5] >= 0x60)                /* bsid >= 12 */
        return 0;
    half = halfrate[p_buf[5] >> 3];

    /* acmod, dsurmod and lfeon */
    acmod = p_buf[6] >> 5;
    if ( (p_buf[6] & 0xf8) == 0x50 )
    {
        /* Dolby surround = stereo + Dolby */
        *pi_channels = 2;
        *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                            | AOUT_CHAN_DOLBYSTEREO;
    }
    else switch ( acmod )
    {
    case 0x0:
        /* Dual-mono = stereo + dual-mono */
        *pi_channels = 2;
        *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                            | AOUT_CHAN_DUALMONO;
        break;
    case 0x1:
        /* Mono */
        *pi_channels = 1;
        *pi_channels_conf = AOUT_CHAN_CENTER;
        break;
    case 0x2:
        /* Stereo */
        *pi_channels = 2;
        *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
        break;
    case 0x3:
        /* 3F */
        *pi_channels = 3;
        *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                            | AOUT_CHAN_CENTER;
        break;
    case 0x4:
        /* 2F1R */
        *pi_channels = 3;
        *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                            | AOUT_CHAN_REARCENTER;
        break;
    case 0x5:
        /* 3F1R */
        *pi_channels = 4;
        *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
                            | AOUT_CHAN_REARCENTER;
        break;
    case 0x6:
        /* 2F2R */
        *pi_channels = 4;
        *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                            | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
        break;
    case 0x7:
        /* 3F2R */
        *pi_channels = 5;
        *pi_channels_conf = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
                            | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
        break;
    default:
        return 0;
    }

    if ( p_buf[6] & lfeon[acmod] )
    {
        (*pi_channels)++;
        *pi_channels_conf |= AOUT_CHAN_LFE;
    }

    frmsizecod = p_buf[4] & 63;
    if (frmsizecod >= 38)
        return 0;
    bitrate = rate [frmsizecod >> 1];
    *pi_bit_rate = (bitrate * 1000) >> half;

    switch (p_buf[4] & 0xc0) {
    case 0:
        *pi_sample_rate = 48000 >> half;
        return 4 * bitrate;
    case 0x40:
        *pi_sample_rate = 44100 >> half;
        return 2 * (320 * bitrate / 147 + (frmsizecod & 1));
    case 0x80:
        *pi_sample_rate = 32000 >> half;
        return 6 * bitrate;
    default:
        return 0;
    }
}
