/*****************************************************************************
 * decoder.c: AAC decoder using libfaad2
 *****************************************************************************
 * Copyright (C) 2001, 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>

#include <faad.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("AAC audio decoder (using libfaad2)") );
    set_capability( "decoder", 100 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACODEC );
    set_callbacks( Open, Close );
vlc_module_end();

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static aout_buffer_t *DecodeBlock( decoder_t *, block_t ** );
static void DoReordering( decoder_t *, uint32_t *, uint32_t *, int, int,
                          uint32_t * );

#define MAX_CHANNEL_POSITIONS 9

struct decoder_sys_t
{
    /* faad handler */
    faacDecHandle *hfaad;

    /* samples */
    audio_date_t date;

    /* temporary buffer */
    uint8_t *p_buffer;
    int     i_buffer;
    int     i_buffer_size;

    /* Channel positions of the current stream (for re-ordering) */
    uint32_t pi_channel_positions[MAX_CHANNEL_POSITIONS];
};

static const uint32_t pi_channels_in[MAX_CHANNEL_POSITIONS] =
    { FRONT_CHANNEL_CENTER, FRONT_CHANNEL_LEFT, FRONT_CHANNEL_RIGHT,
      SIDE_CHANNEL_LEFT, SIDE_CHANNEL_RIGHT,
      BACK_CHANNEL_LEFT, BACK_CHANNEL_RIGHT,
      BACK_CHANNEL_CENTER, LFE_CHANNEL };
static const uint32_t pi_channels_out[MAX_CHANNEL_POSITIONS] =
    { AOUT_CHAN_CENTER, AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT,
      AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT,
      AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,
      AOUT_CHAN_REARCENTER, AOUT_CHAN_LFE };
static const uint32_t pi_channels_ordered[MAX_CHANNEL_POSITIONS] =
    { AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT,
      AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT,
      AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,
      AOUT_CHAN_CENTER, AOUT_CHAN_REARCENTER, AOUT_CHAN_LFE
    };

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;
    faacDecConfiguration *cfg;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('m','p','4','a') )
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

    /* Open a faad context */
    if( ( p_sys->hfaad = faacDecOpen() ) == NULL )
    {
        msg_Err( p_dec, "cannot initialize faad" );
        return VLC_EGENERIC;
    }

    /* Misc init */
    aout_DateSet( &p_sys->date, 0 );
    p_dec->fmt_out.i_cat = AUDIO_ES;

    if (p_this->p_libvlc->i_cpu & CPU_CAPABILITY_FPU)
        p_dec->fmt_out.i_codec = VLC_FOURCC('f','l','3','2');
    else
        p_dec->fmt_out.i_codec = AOUT_FMT_S16_NE;
    p_dec->pf_decode_audio = DecodeBlock;

    p_dec->fmt_out.audio.i_physical_channels =
        p_dec->fmt_out.audio.i_original_channels = 0;

    if( p_dec->fmt_in.i_extra > 0 )
    {
        /* We have a decoder config so init the handle */
        unsigned long i_rate;
        unsigned char i_channels;

        if( faacDecInit2( p_sys->hfaad, p_dec->fmt_in.p_extra,
                          p_dec->fmt_in.i_extra,
                          &i_rate, &i_channels ) < 0 )
        {
            return VLC_EGENERIC;
        }

        p_dec->fmt_out.audio.i_rate = i_rate;
        p_dec->fmt_out.audio.i_channels = i_channels;
        aout_DateInit( &p_sys->date, i_rate );
    }
    else
    {
        /* Will be initalised from first frame */
        p_dec->fmt_out.audio.i_rate = 0;
        p_dec->fmt_out.audio.i_channels = 0;
    }

    /* Set the faad config */
    cfg = faacDecGetCurrentConfiguration( p_sys->hfaad );
    if (p_this->p_libvlc->i_cpu & CPU_CAPABILITY_FPU)
        cfg->outputFormat = FAAD_FMT_FLOAT;
    else
        cfg->outputFormat = FAAD_FMT_16BIT;
    faacDecSetConfiguration( p_sys->hfaad, cfg );

    /* buffer */
    p_sys->i_buffer = p_sys->i_buffer_size = 0;
    p_sys->p_buffer = 0;

    /* Faad2 can't deal with truncated data (eg. from MPEG TS) */
    p_dec->b_need_packetized = VLC_TRUE;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DecodeBlock:
 *****************************************************************************/
static aout_buffer_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;

    if( p_block->i_flags&(BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        block_Release( p_block );
        return NULL;
    }

    /* Append the block to the temporary buffer */
    if( p_sys->i_buffer_size < p_sys->i_buffer + p_block->i_buffer )
    {
        p_sys->i_buffer_size = p_sys->i_buffer + p_block->i_buffer;
        p_sys->p_buffer = realloc( p_sys->p_buffer, p_sys->i_buffer_size );
    }

    if( p_block->i_buffer )
    {
        memcpy( &p_sys->p_buffer[p_sys->i_buffer],
                p_block->p_buffer, p_block->i_buffer );
        p_sys->i_buffer += p_block->i_buffer;
        p_block->i_buffer = 0;
    }

    if( p_dec->fmt_out.audio.i_rate == 0 && p_dec->fmt_in.i_extra > 0 )
    {
        /* We have a decoder config so init the handle */
        unsigned long i_rate;
        unsigned char i_channels;

        if( faacDecInit2( p_sys->hfaad, p_dec->fmt_in.p_extra,
                          p_dec->fmt_in.i_extra,
                          &i_rate, &i_channels ) >= 0 )
        {
            p_dec->fmt_out.audio.i_rate = i_rate;
            p_dec->fmt_out.audio.i_channels = i_channels;
            aout_DateInit( &p_sys->date, i_rate );
        }
    }

    if( p_dec->fmt_out.audio.i_rate == 0 && p_sys->i_buffer )
    {
        unsigned long i_rate;
        unsigned char i_channels;

        /* Init faad with the first frame */
        if( faacDecInit( p_sys->hfaad,
                         p_sys->p_buffer, p_sys->i_buffer,
                         &i_rate, &i_channels ) < 0 )
        {
            block_Release( p_block );
            return NULL;
        }

        p_dec->fmt_out.audio.i_rate = i_rate;
        p_dec->fmt_out.audio.i_channels = i_channels;
        aout_DateInit( &p_sys->date, i_rate );
    }

    if( p_block->i_pts != 0 && p_block->i_pts != aout_DateGet( &p_sys->date ) )
    {
        aout_DateSet( &p_sys->date, p_block->i_pts );
    }
    else if( !aout_DateGet( &p_sys->date ) )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( p_block );
        p_sys->i_buffer = 0;
        return NULL;
    }

    /* Decode all data */
    if( p_sys->i_buffer )
    {
        void *samples;
        faacDecFrameInfo frame;
        aout_buffer_t *p_out;
        int i, j;

        samples = faacDecDecode( p_sys->hfaad, &frame,
                                 p_sys->p_buffer, p_sys->i_buffer );

        if( frame.error > 0 )
        {
            msg_Warn( p_dec, "%s", faacDecGetErrorMessage( frame.error ) );

            /* Flush the buffer */
            p_sys->i_buffer = 0;
            block_Release( p_block );
            return NULL;
        }

        if( frame.channels <= 0 || frame.channels > 8 || frame.channels == 7 )
        {
            msg_Warn( p_dec, "invalid channels count: %i", frame.channels );

            /* Flush the buffer */
            p_sys->i_buffer -= frame.bytesconsumed;
            if( p_sys->i_buffer > 0 )
            {
                memmove( p_sys->p_buffer,&p_sys->p_buffer[frame.bytesconsumed],
                         p_sys->i_buffer );
            }
            block_Release( p_block );
            return NULL;
        }

        if( frame.samples <= 0 )
        {
            msg_Warn( p_dec, "decoded zero sample" );

            /* Flush the buffer */
            p_sys->i_buffer -= frame.bytesconsumed;
            if( p_sys->i_buffer > 0 )
            {
                memmove( p_sys->p_buffer,&p_sys->p_buffer[frame.bytesconsumed],
                         p_sys->i_buffer );
            }
            block_Release( p_block );
            return NULL;
        }

        /* We decoded a valid frame */
        if( p_dec->fmt_out.audio.i_rate != frame.samplerate )
        {
            aout_DateInit( &p_sys->date, frame.samplerate );
            aout_DateSet( &p_sys->date, p_block->i_pts );
        }
        p_block->i_pts = 0;  /* PTS is valid only once */

        p_dec->fmt_out.audio.i_rate = frame.samplerate;
        p_dec->fmt_out.audio.i_channels = frame.channels;


        /* Convert frame.channel_position to our own channel values */
        for( i = 0; i < frame.channels; i++ )
        {
            /* Find the channel code */
            for( j = 0; j < MAX_CHANNEL_POSITIONS; j++ )
            {
                if( frame.channel_position[i] == pi_channels_in[j] )
                {
                    p_sys->pi_channel_positions[i] = pi_channels_out[j];
                    p_dec->fmt_out.audio.i_physical_channels |=
                        pi_channels_out[j];
                    break;
                }
            }

            if( j == MAX_CHANNEL_POSITIONS )
            {
                msg_Warn( p_dec, "unknown channel ordering" );
                block_Release( p_block );
                return NULL;
            }
        }
        p_dec->fmt_out.audio.i_original_channels =
            p_dec->fmt_out.audio.i_physical_channels;

        p_out = p_dec->pf_aout_buffer_new(p_dec, frame.samples/frame.channels);
        if( p_out == NULL )
        {
            p_sys->i_buffer = 0;
            block_Release( p_block );
            return NULL;
        }

        p_out->start_date = aout_DateGet( &p_sys->date );
        p_out->end_date = aout_DateIncrement( &p_sys->date,
                                              frame.samples / frame.channels );

        DoReordering( p_dec, (uint32_t *)p_out->p_buffer, samples,
                      frame.samples / frame.channels, frame.channels,
                      p_sys->pi_channel_positions );

        p_sys->i_buffer -= frame.bytesconsumed;
        if( p_sys->i_buffer > 0 )
        {
            memmove( p_sys->p_buffer, &p_sys->p_buffer[frame.bytesconsumed],
                     p_sys->i_buffer );
        }

        return p_out;
    }

    block_Release( p_block );
    return NULL;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    faacDecClose( p_sys->hfaad );
    free( p_sys );
}

/*****************************************************************************
 * DoReordering: do some channel re-ordering (the ac3 channel order is
 *   different from the aac one).
 *****************************************************************************/
static void DoReordering( decoder_t *p_dec,
                          uint32_t *p_out, uint32_t *p_in, int i_samples,
                          int i_nb_channels, uint32_t *pi_chan_positions )
{
    int pi_chan_table[MAX_CHANNEL_POSITIONS];
    int i, j, k;

    /* Find the channels mapping */
    for( i = 0, j = 0; i < MAX_CHANNEL_POSITIONS; i++ )
    {
        for( k = 0; k < i_nb_channels; k++ )
        {
            if( pi_channels_ordered[i] == pi_chan_positions[k] )
            {
                pi_chan_table[k] = j++;
                break;
            }
        }
    }

    /* Do the actual reordering */
    if( p_dec->p_libvlc->i_cpu & CPU_CAPABILITY_FPU )
        for( i = 0; i < i_samples; i++ )
            for( j = 0; j < i_nb_channels; j++ )
                p_out[i * i_nb_channels + pi_chan_table[j]] =
                    p_in[i * i_nb_channels + j];
    else
        for( i = 0; i < i_samples; i++ )
            for( j = 0; j < i_nb_channels; j++ )
                ((uint16_t *)p_out)[i * i_nb_channels + pi_chan_table[j]] =
                    ((uint16_t *)p_in)[i * i_nb_channels + j];
}
