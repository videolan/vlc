/*****************************************************************************
 * decoder.c: AAC decoder using libfaad2
 *****************************************************************************
 * Copyright (C) 2001, 2003 VideoLAN
 * $Id: faad.c,v 1.2 2003/11/04 02:23:11 fenrir Exp $
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

#include <stdlib.h>

#include <vlc/aout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>

#include <faad.h>
#include "codecs.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open( vlc_object_t * );

vlc_module_begin();
    set_description( _("AAC audio decoder (using libfaad2)") );
    set_capability( "decoder", 60 );
    set_callbacks( Open, NULL );
vlc_module_end();


/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  InitDecoder   ( decoder_t * );
static int  RunDecoder    ( decoder_t *, block_t * );
static int  EndDecoder    ( decoder_t * );

struct decoder_sys_t
{
    /* faad handler */
    faacDecHandle *hfaad;

    /* audio output */
    audio_sample_format_t output_format;
    aout_instance_t *     p_aout;                                  /* opaque */
    aout_input_t *        p_aout_input;                            /* opaque */

    /* samples */
    audio_date_t          date;

    /* temporary buffer */
    uint8_t               *p_buffer;
    int                   i_buffer;
    int                   i_buffer_size;
};

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int  Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    if( p_dec->p_fifo->i_fourcc != VLC_FOURCC('m','p','4','a') )
    {
        return VLC_EGENERIC;
    }

    p_dec->pf_init = InitDecoder;
    p_dec->pf_decode = RunDecoder;
    p_dec->pf_end = EndDecoder;

    p_dec->p_sys  = malloc( sizeof( decoder_sys_t ) );

    return VLC_SUCCESS;
}

static unsigned int pi_channels_maps[7] =
{
    0,
    AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
     /* FIXME */
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_REARCENTER
};

/*****************************************************************************
 * InitDecoder:
 *****************************************************************************/
static int  InitDecoder   ( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    WAVEFORMATEX    wf, *p_wf;
    faacDecConfiguration *cfg;

    if( ( p_wf = (WAVEFORMATEX*)p_dec->p_fifo->p_waveformatex ) == NULL )
    {
        p_wf = &wf;
        memset( p_wf, 0, sizeof( WAVEFORMATEX ) );
    }

    /* Open a faad context */
    if( ( p_sys->hfaad = faacDecOpen() ) == NULL )
    {
        msg_Err( p_dec, "Cannot initialize faad" );
        return VLC_EGENERIC;
    }

    if( p_wf->cbSize > 0 )
    {
        /* We have a decoder config so init the handle */
        unsigned long   i_rate;
        unsigned char   i_channels;

        if( faacDecInit2( p_sys->hfaad,
                          (uint8_t*)&p_wf[1], p_wf->cbSize,
                          &i_rate, &i_channels ) < 0 )
        {
            return VLC_EGENERIC;
        }

        p_sys->output_format.i_rate = i_rate;
        p_sys->output_format.i_physical_channels =
        p_sys->output_format.i_original_channels =
            pi_channels_maps[i_channels];
    }
    else
    {
        /* Will be initalised from first frame */
        p_sys->output_format.i_rate = 0;
        p_sys->output_format.i_physical_channels =
        p_sys->output_format.i_original_channels = 0;
    }
    p_sys->output_format.i_format = VLC_FOURCC('f','l','3','2');

    p_sys->p_aout = NULL;
    p_sys->p_aout_input = NULL;

    /* set the faad config */
    cfg = faacDecGetCurrentConfiguration( p_sys->hfaad );
    cfg->outputFormat = FAAD_FMT_FLOAT;
    faacDecSetConfiguration( p_sys->hfaad, cfg );

    /* buffer */
    p_sys->i_buffer = 0;
    p_sys->i_buffer_size = 10000;
    p_sys->p_buffer = malloc( p_sys->i_buffer_size );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * InitDecoder:
 *****************************************************************************/
static int  RunDecoder    ( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    int          i_used = 0;
    mtime_t      i_pts = p_block->i_pts;

    /* Append the block to the temporary buffer */
    if( p_sys->i_buffer_size < p_sys->i_buffer + p_block->i_buffer )
    {
        p_sys->i_buffer_size += p_block->i_buffer;
        p_sys->p_buffer = realloc( p_sys->p_buffer, p_sys->i_buffer_size );
    }
    memcpy( &p_sys->p_buffer[p_sys->i_buffer], p_block->p_buffer, p_block->i_buffer );
    p_sys->i_buffer += p_block->i_buffer;

    if( p_sys->output_format.i_rate == 0 )
    {
        unsigned long   i_rate;
        unsigned char   i_channels;

        /* Init faad with the first frame */
        if( faacDecInit( p_sys->hfaad,
                         p_sys->p_buffer, p_sys->i_buffer,
                         &i_rate, &i_channels ) < 0 )
        {
            return VLC_EGENERIC;
        }
        p_sys->output_format.i_rate = i_rate;
        p_sys->output_format.i_physical_channels =
        p_sys->output_format.i_original_channels =
            pi_channels_maps[i_channels];
    }

    /* Decode all data */
    while( i_used < p_sys->i_buffer )
    {
        void *samples;
        faacDecFrameInfo frame;
        aout_buffer_t   *out;

        samples = faacDecDecode( p_sys->hfaad, &frame,
                                 &p_sys->p_buffer[i_used], p_sys->i_buffer - i_used );

        if( frame.error > 0 )
        {
            msg_Warn( p_dec, "%s", faacDecGetErrorMessage( frame.error ) );
            /* flush the buffer */
            p_sys->i_buffer = 0;
            return VLC_SUCCESS;
        }
        if( frame.channels <= 0 || frame.channels > 6 )
        {
            msg_Warn( p_dec, "invalid channels count" );
            /* flush the buffer */
            p_sys->i_buffer = 0;
            return VLC_SUCCESS;
        }
        if( frame.samples <= 0 )
        {
            msg_Warn( p_dec, "decoded zero samples" );
            /* flush the buffer */
            p_sys->i_buffer = 0;
            return VLC_SUCCESS;
        }

        /* we have decoded a valid frame */
        /* msg_Dbg( p_dec, "consumed %d for %dHz %dc %lld", frame.bytesconsumed, frame.samplerate, frame.channels, i_pts ); */
        i_used += frame.bytesconsumed;


        /* create/recreate the output */
        if( p_sys->p_aout_input &&
            ( p_sys->output_format.i_original_channels != pi_channels_maps[frame.channels] ||
              p_sys->output_format.i_rate != frame.samplerate ) )
        {
            aout_DecDelete( p_sys->p_aout, p_sys->p_aout_input );
            p_sys->p_aout_input = NULL;
        }

        if( p_sys->p_aout_input == NULL )
        {
            p_sys->output_format.i_physical_channels =
            p_sys->output_format.i_original_channels = pi_channels_maps[frame.channels];
            p_sys->output_format.i_rate = frame.samplerate;

            aout_DateInit( &p_sys->date, p_sys->output_format.i_rate );
            aout_DateSet( &p_sys->date, 0 );

            p_sys->p_aout_input = aout_DecNew( p_dec, &p_sys->p_aout, &p_sys->output_format );
        }

        if( p_sys->p_aout_input == NULL )
        {
            msg_Err( p_dec, "cannot create aout" );
            return VLC_EGENERIC;
        }

        if( i_pts != 0 && i_pts != aout_DateGet( &p_sys->date ) )
        {
            aout_DateSet( &p_sys->date, i_pts );
        }
        else if( !aout_DateGet( &p_sys->date ) )
        {
            /* Wait for a dated packet */
            msg_Dbg( p_dec, "no date" );
            p_sys->i_buffer = 0;
            return VLC_SUCCESS;
        }
        i_pts = 0;  /* PTS is valid only once */

        if( ( out = aout_DecNewBuffer( p_sys->p_aout, p_sys->p_aout_input,
                                       frame.samples / frame.channels ) ) == NULL )
        {
            msg_Err( p_dec, "cannot get a new buffer" );
            return VLC_EGENERIC;
        }
        out->start_date = aout_DateGet( &p_sys->date );
        out->end_date   = aout_DateIncrement( &p_sys->date,
                                              frame.samples / frame.channels );
        memcpy( out->p_buffer, samples, out->i_nb_bytes );

        aout_DecPlay( p_sys->p_aout, p_sys->p_aout_input, out );
    }

    p_sys->i_buffer -= i_used;
    if( p_sys->i_buffer > 0 )
    {
        memmove( &p_sys->p_buffer[0], &p_sys->p_buffer[i_used], p_sys->i_buffer );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * InitDecoder:
 *****************************************************************************/
static int  EndDecoder    ( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_aout_input )
    {
        aout_DecDelete( p_sys->p_aout, p_sys->p_aout_input );
    }

    faacDecClose( p_sys->hfaad );
    free( p_sys );

    return VLC_SUCCESS;
}



