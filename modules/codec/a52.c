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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_aout.h>
#include <vlc_block_helper.h>
#include <vlc_bits.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  OpenPacketizer( vlc_object_t * );
static void CloseCommon   ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("A/52 parser") )
    set_capability( "decoder", 100 )
    set_callbacks( OpenDecoder, CloseCommon )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )

    add_submodule ()
    set_description( N_("A/52 audio packetizer") )
    set_capability( "packetizer", 10 )
    set_callbacks( OpenPacketizer, CloseCommon )
vlc_module_end ()

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/

#define A52_HEADER_SIZE 7

struct decoder_sys_t
{
    /* Module mode */
    bool b_packetizer;

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
static void *DecodeBlock  ( decoder_t *, block_t ** );

static int  SyncInfo      ( const uint8_t *, unsigned int *, unsigned int *,
                            unsigned int *, int * );

static uint8_t       *GetOutBuffer ( decoder_t *, void ** );
static aout_buffer_t *GetAoutBuffer( decoder_t * );
static block_t       *GetSoutBuffer( decoder_t * );

/*****************************************************************************
 * OpenCommon: probe the decoder/packetizer and return score
 *****************************************************************************/
static int OpenCommon( vlc_object_t *p_this, bool b_packetizer )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;
    vlc_fourcc_t i_codec;

    switch( p_dec->fmt_in.i_codec )
    {
    case VLC_FOURCC('a','5','2',' '):
    case VLC_FOURCC('a','5','2','b'):
        i_codec = VLC_FOURCC('a','5','2',' ');
        break;
    case VLC_FOURCC('e','a','c','3'):
        /* XXX ugly hack, a52 does not support eac3 so no eac3 pass-through
         * support */
        if( !b_packetizer )
            return VLC_EGENERIC;
        i_codec = VLC_FOURCC('e','a','c','3');
        break;
    default:
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;

    /* Misc init */
    p_sys->b_packetizer = b_packetizer;
    p_sys->i_state = STATE_NOSYNC;
    aout_DateSet( &p_sys->end_date, 0 );

    p_sys->bytestream = block_BytestreamInit();

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;
    p_dec->fmt_out.i_codec = i_codec;
    p_dec->fmt_out.audio.i_rate = 0; /* So end_date gets initialized */

    /* Set callback */
    if( b_packetizer )
        p_dec->pf_packetize    = (block_t *(*)(decoder_t *, block_t **))
            DecodeBlock;
    else
        p_dec->pf_decode_audio = (aout_buffer_t *(*)(decoder_t *, block_t **))
            DecodeBlock;
    return VLC_SUCCESS;
}

static int OpenDecoder( vlc_object_t *p_this )
{
    return OpenCommon( p_this, false );
}

static int OpenPacketizer( vlc_object_t *p_this )
{
    return OpenCommon( p_this, true );
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

    if( (*pp_block)->i_flags&(BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        if( (*pp_block)->i_flags&BLOCK_FLAG_CORRUPTED )
        {
            p_sys->i_state = STATE_NOSYNC;
            block_BytestreamEmpty( &p_sys->bytestream );
        }
        aout_DateSet( &p_sys->end_date, 0 );
        block_Release( *pp_block );
        return NULL;
    }

    if( !aout_DateGet( &p_sys->end_date ) && !(*pp_block)->i_pts )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( *pp_block );
        return NULL;
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
                //p_dec->b_error = true;
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
 * CloseCommon: clean up the decoder
 *****************************************************************************/
static void CloseCommon( vlc_object_t *p_this )
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

    p_buf = decoder_NewAudioBuffer( p_dec, A52_FRAME_NB  );
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

    p_block->i_length =
        aout_DateIncrement( &p_sys->end_date, A52_FRAME_NB ) - p_block->i_pts;

    return p_block;
}

/* Tables */
static const struct
{
    unsigned int i_count;
    unsigned int i_configuration;
} p_acmod[8] = {
    { 2, AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_DUALMONO },   /* Dual-channel 1+1 */
    { 1, AOUT_CHAN_CENTER },                                        /* Mono 1/0 */
    { 2, AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT },                        /* Stereo 2/0 */
    { 3, AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER },     /* 3F 3/0 */
    { 3, AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARCENTER }, /* 2F1R 2/1 */
    { 4, AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER |
         AOUT_CHAN_REARCENTER },                                    /* 3F1R 3/1 */
    { 5, AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT |
         AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT },                /* 2F2R 2/2 */
    { 6, AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER |
         AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT },                /* 3F2R 3/2 */
};

/**
 * It parse AC3 sync info.
 *
 * This code is borrowed from liba52 by Aaron Holtzman & Michel Lespinasse,
 * since we don't want to oblige S/PDIF people to use liba52 just to get
 * their SyncInfo...
 */
static int SyncInfoAC3( const uint8_t *p_buf,
                        unsigned int *pi_channels,
                        unsigned int *pi_channels_conf,
                        unsigned int *pi_sample_rate, int *pi_bit_rate )
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

    /* */
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
    else
    {
        *pi_channels      = p_acmod[acmod].i_count;
        *pi_channels_conf = p_acmod[acmod].i_configuration;
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

/**
 * It parse E-AC3 sync info
 */
static int SyncInfoEAC3( const uint8_t *p_buf,
                         unsigned int *pi_channels,
                         unsigned int *pi_channels_conf,
                         unsigned int *pi_sample_rate, int *pi_bit_rate )
{
    static const int pi_samplerate[3] = { 48000, 44100, 32000 };
    bs_t s;
    int i_frame_size;
    int i_fscod, i_fscod2;
    int i_numblkscod;
    int i_acmod, i_lfeon;
    int i_bytes;


    bs_init( &s, (void*)p_buf, A52_HEADER_SIZE );
    bs_skip( &s, 16 +   /* start code */
                 2 +    /* stream type */
                 3 );   /* substream id */
    i_frame_size = bs_read( &s, 11 );
    if( i_frame_size < 2 )
        return 0;
    i_bytes = 2 * ( i_frame_size + 1 );

    i_fscod = bs_read( &s, 2 );
    if( i_fscod == 0x03 )
    {
        i_fscod2 = bs_read( &s, 2 );
        if( i_fscod2 == 0X03 )
            return 0;
        *pi_sample_rate = pi_samplerate[i_fscod2] / 2;
        i_numblkscod = 6;
    }
    else
    {
        static const int pi_blocks[4] = { 1, 2, 3, 6 };

        *pi_sample_rate = pi_samplerate[i_fscod];
        i_numblkscod = pi_blocks[bs_read( &s, 2 )];
    }

    i_acmod = bs_read( &s, 3 );
    i_lfeon = bs_read1( &s );

    *pi_channels      = p_acmod[i_acmod].i_count + i_lfeon;
    *pi_channels_conf = p_acmod[i_acmod].i_configuration | ( i_lfeon ? AOUT_CHAN_LFE : 0);
    *pi_bit_rate = 8 * i_bytes * (*pi_sample_rate) / (i_numblkscod * 256);

    return i_bytes;
}

static int SyncInfo( const uint8_t *p_buf,
                     unsigned int *pi_channels,
                     unsigned int *pi_channels_conf,
                     unsigned int *pi_sample_rate, int *pi_bit_rate )
{
    int bsid;

    /* Check synword */
    if( p_buf[0] != 0x0b || p_buf[1] != 0x77 )
        return 0;

    /* Check bsid */
    bsid = p_buf[5] >> 3;
    if( bsid > 16 )
        return 0;

    if( bsid <= 10 )
        return SyncInfoAC3( p_buf, pi_channels, pi_channels_conf,
                            pi_sample_rate, pi_bit_rate );
    else
        return SyncInfoEAC3( p_buf, pi_channels, pi_channels_conf,
                             pi_sample_rate, pi_bit_rate );
}

