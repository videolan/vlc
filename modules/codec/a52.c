/*****************************************************************************
 * a52.c: parse A/52 audio sync info and packetize the stream
 *****************************************************************************
 * Copyright (C) 2001-2002 the VideoLAN team
 * $Id$
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <vlc/decoder.h>

#include "vlc_block_helper.h"

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
    int i_state;

    block_bytestream_t bytestream;

    /*
     * Common properties
     */
    audio_date_t   end_date;

    mtime_t i_pts;
    int i_frame_size, i_bit_rate;
    unsigned int i_rate, i_channels, i_channels_conf;

};

enum {

    STATE_NOSYNC,
    STATE_SYNC,
    STATE_HEADER,
    STATE_NEXT_SYNC,
    STATE_GET_DATA,
    STATE_SEND_DATA
};

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  OpenPacketizer( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );
static void *DecodeBlock  ( decoder_t *, block_t ** );

static int  SyncInfo      ( const byte_t *, unsigned int *, unsigned int *,
                            unsigned int *, int * );

static uint8_t       *GetOutBuffer ( decoder_t *, void ** );
static aout_buffer_t *GetAoutBuffer( decoder_t * );
static block_t       *GetSoutBuffer( decoder_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("A/52 parser") );
    set_capability( "decoder", 100 );
    set_callbacks( OpenDecoder, CloseDecoder );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACODEC );

    add_submodule();
    set_description( _("A/52 audio packetizer") );
    set_capability( "packetizer", 10 );
    set_callbacks( OpenPacketizer, CloseDecoder );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('a','5','2',' ')
          && p_dec->fmt_in.i_codec != VLC_FOURCC('a','5','2','b') )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return VLC_EGENERIC;
    }

    /* Misc init */
    p_sys->b_packetizer = VLC_FALSE;
    p_sys->i_state = STATE_NOSYNC;
    aout_DateSet( &p_sys->end_date, 0 );

    p_sys->bytestream = block_BytestreamInit( p_dec );

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;
    p_dec->fmt_out.i_codec = VLC_FOURCC('a','5','2',' ');
    p_dec->fmt_out.audio.i_rate = 0; /* So end_date gets initialized */

    /* Set callback */
    p_dec->pf_decode_audio = (aout_buffer_t *(*)(decoder_t *, block_t **))
        DecodeBlock;
    p_dec->pf_packetize    = (block_t *(*)(decoder_t *, block_t **))
        DecodeBlock;

    return VLC_SUCCESS;
}

static int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    int i_ret = OpenDecoder( p_this );

    if( i_ret == VLC_SUCCESS ) p_dec->p_sys->b_packetizer = VLC_TRUE;

    return i_ret;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function is called just after the thread is launched.
 ****************************************************************************/
static void *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t p_header[A52_HEADER_SIZE];
    uint8_t *p_buf;
    void *p_out_buffer;

    if( !pp_block || !*pp_block ) return NULL;

    if( !aout_DateGet( &p_sys->end_date ) && !(*pp_block)->i_pts )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( *pp_block );
        return NULL;
    }

    if( (*pp_block)->i_flags&(BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        p_sys->i_state = STATE_NOSYNC;
    }

    block_BytestreamPush( &p_sys->bytestream, *pp_block );

    while( 1 )
    {
        switch( p_sys->i_state )
        {
        case STATE_NOSYNC:
            while( block_PeekBytes( &p_sys->bytestream, p_header, 2 )
                   == VLC_SUCCESS )
            {
                if( p_header[0] == 0x0b && p_header[1] == 0x77 )
                {
                    p_sys->i_state = STATE_SYNC;
                    break;
                }
                block_SkipByte( &p_sys->bytestream );
            }
            if( p_sys->i_state != STATE_SYNC )
            {
                block_BytestreamFlush( &p_sys->bytestream );

                /* Need more data */
                return NULL;
            }

        case STATE_SYNC:
            /* New frame, set the Presentation Time Stamp */
            p_sys->i_pts = p_sys->bytestream.p_block->i_pts;
            if( p_sys->i_pts != 0 &&
                p_sys->i_pts != aout_DateGet( &p_sys->end_date ) )
            {
                aout_DateSet( &p_sys->end_date, p_sys->i_pts );
            }
            p_sys->i_state = STATE_HEADER;

        case STATE_HEADER:
            /* Get A/52 frame header (A52_HEADER_SIZE bytes) */
            if( block_PeekBytes( &p_sys->bytestream, p_header,
                                 A52_HEADER_SIZE ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }

            /* Check if frame is valid and get frame info */
            p_sys->i_frame_size = SyncInfo( p_header,
                                            &p_sys->i_channels,
                                            &p_sys->i_channels_conf,
                                            &p_sys->i_rate,
                                            &p_sys->i_bit_rate );
            if( !p_sys->i_frame_size )
            {
                msg_Dbg( p_dec, "emulated sync word" );
                block_SkipByte( &p_sys->bytestream );
                p_sys->i_state = STATE_NOSYNC;
                break;
            }
            p_sys->i_state = STATE_NEXT_SYNC;

        case STATE_NEXT_SYNC:
            /* TODO: If pp_block == NULL, flush the buffer without checking the
             * next sync word */

            /* Check if next expected frame contains the sync word */
            if( block_PeekOffsetBytes( &p_sys->bytestream,
                                       p_sys->i_frame_size, p_header, 2 )
                != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }

            if( p_sys->b_packetizer &&
                p_header[0] == 0 && p_header[1] == 0 )
            {
                /* A52 wav files and audio CD's use stuffing */
                p_sys->i_state = STATE_GET_DATA;
                break;
            }

            if( p_header[0] != 0x0b || p_header[1] != 0x77 )
            {
                msg_Dbg( p_dec, "emulated sync word "
                         "(no sync on following frame)" );
                p_sys->i_state = STATE_NOSYNC;
                block_SkipByte( &p_sys->bytestream );
                break;
            }
            p_sys->i_state = STATE_SEND_DATA;
            break;

        case STATE_GET_DATA:
            /* Make sure we have enough data.
             * (Not useful if we went through NEXT_SYNC) */
            if( block_WaitBytes( &p_sys->bytestream,
                                 p_sys->i_frame_size ) != VLC_SUCCESS )
            {
                /* Need more data */
                return NULL;
            }
            p_sys->i_state = STATE_SEND_DATA;

        case STATE_SEND_DATA:
            if( !(p_buf = GetOutBuffer( p_dec, &p_out_buffer )) )
            {
                //p_dec->b_error = VLC_TRUE;
                return NULL;
            }

            /* Copy the whole frame into the buffer. When we reach this point
             * we already know we have enough data available. */
            block_GetBytes( &p_sys->bytestream, p_buf, p_sys->i_frame_size );

            /* Make sure we don't reuse the same pts twice */
            if( p_sys->i_pts == p_sys->bytestream.p_block->i_pts )
                p_sys->i_pts = p_sys->bytestream.p_block->i_pts = 0;

            /* So p_block doesn't get re-added several times */
            *pp_block = block_BytestreamPop( &p_sys->bytestream );

            p_sys->i_state = STATE_NOSYNC;

            return p_out_buffer;
        }
    }

    return NULL;
}

/*****************************************************************************
 * CloseDecoder: clean up the decoder
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_BytestreamRelease( &p_sys->bytestream );

    free( p_sys );
}

/*****************************************************************************
 * GetOutBuffer:
 *****************************************************************************/
static uint8_t *GetOutBuffer( decoder_t *p_dec, void **pp_out_buffer )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t *p_buf;

    if( p_dec->fmt_out.audio.i_rate != p_sys->i_rate )
    {
        msg_Info( p_dec, "A/52 channels:%d samplerate:%d bitrate:%d",
                  p_sys->i_channels, p_sys->i_rate, p_sys->i_bit_rate );

        aout_DateInit( &p_sys->end_date, p_sys->i_rate );
        aout_DateSet( &p_sys->end_date, p_sys->i_pts );
    }

    p_dec->fmt_out.audio.i_rate     = p_sys->i_rate;
    p_dec->fmt_out.audio.i_channels = p_sys->i_channels;
    p_dec->fmt_out.audio.i_bytes_per_frame = p_sys->i_frame_size;
    p_dec->fmt_out.audio.i_frame_length = A52_FRAME_NB;

    p_dec->fmt_out.audio.i_original_channels = p_sys->i_channels_conf;
    p_dec->fmt_out.audio.i_physical_channels =
        p_sys->i_channels_conf & AOUT_CHAN_PHYSMASK;

    p_dec->fmt_out.i_bitrate = p_sys->i_bit_rate;

    if( p_sys->b_packetizer )
    {
        block_t *p_sout_buffer = GetSoutBuffer( p_dec );
        p_buf = p_sout_buffer ? p_sout_buffer->p_buffer : NULL;
        *pp_out_buffer = p_sout_buffer;
    }
    else
    {
        aout_buffer_t *p_aout_buffer = GetAoutBuffer( p_dec );
        p_buf = p_aout_buffer ? p_aout_buffer->p_buffer : NULL;
        *pp_out_buffer = p_aout_buffer;
    }

    return p_buf;
}

/*****************************************************************************
 * GetAoutBuffer:
 *****************************************************************************/
static aout_buffer_t *GetAoutBuffer( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    aout_buffer_t *p_buf;

    p_buf = p_dec->pf_aout_buffer_new( p_dec, A52_FRAME_NB  );
    if( p_buf == NULL ) return NULL;

    p_buf->start_date = aout_DateGet( &p_sys->end_date );
    p_buf->end_date = aout_DateIncrement( &p_sys->end_date, A52_FRAME_NB );

    return p_buf;
}

/*****************************************************************************
 * GetSoutBuffer:
 *****************************************************************************/
static block_t *GetSoutBuffer( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;

    p_block = block_New( p_dec, p_sys->i_frame_size );
    if( p_block == NULL ) return NULL;

    p_block->i_pts = p_block->i_dts = aout_DateGet( &p_sys->end_date );

    p_block->i_length = aout_DateIncrement( &p_sys->end_date, A52_FRAME_NB ) -
        p_block->i_pts;

    return p_block;
}

/*****************************************************************************
 * SyncInfo: parse A/52 sync info
 *****************************************************************************
 * This code is borrowed from liba52 by Aaron Holtzman & Michel Lespinasse,
 * since we don't want to oblige S/PDIF people to use liba52 just to get
 * their SyncInfo...
 *****************************************************************************/
static int SyncInfo( const byte_t * p_buf,
                     unsigned int * pi_channels,
                     unsigned int * pi_channels_conf,
                     unsigned int * pi_sample_rate, int * pi_bit_rate )
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
