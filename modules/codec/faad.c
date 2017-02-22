/*****************************************************************************
 * faad.c: AAC decoder using libfaad2
 *****************************************************************************
 * Copyright (C) 2001, 2003 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * NOTA BENE: this module requires the linking against a library which is
 * known to require licensing under the GNU General Public License version 2
 * (or later). Therefore, the result of compiling this module will normally
 * be subject to the terms of that later license.
 *****************************************************************************/


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_codec.h>
#include <vlc_cpu.h>

#include <neaacdec.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("AAC audio decoder (using libfaad2)") )
    set_capability( "decoder", 100 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )
    set_callbacks( Open, Close )
vlc_module_end ()

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int DecodeBlock( decoder_t *, block_t * );
static void Flush( decoder_t * );
static void DoReordering( uint32_t *, uint32_t *, int, int, uint32_t * );

#define MAX_CHANNEL_POSITIONS 9

struct decoder_sys_t
{
    /* faad handler */
    NeAACDecHandle *hfaad;

    /* samples */
    date_t date;

    /* temporary buffer */
    uint8_t *p_buffer;
    size_t  i_buffer;
    size_t  i_buffer_size;

    /* Channel positions of the current stream (for re-ordering) */
    uint32_t pi_channel_positions[MAX_CHANNEL_POSITIONS];

    bool b_sbr, b_ps;
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
static const uint32_t pi_channels_guessed[MAX_CHANNEL_POSITIONS] =
    { 0, AOUT_CHAN_CENTER, AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
      AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_LFE,
      AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
          | AOUT_CHAN_REARRIGHT,
      AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
          | AOUT_CHAN_REARRIGHT | AOUT_CHAN_CENTER,
      AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
          | AOUT_CHAN_REARRIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_LFE,
      AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_MIDDLELEFT
          | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
          | AOUT_CHAN_CENTER,
      AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_MIDDLELEFT
          | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
          | AOUT_CHAN_CENTER | AOUT_CHAN_LFE
    };

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;
    NeAACDecConfiguration *cfg;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_MP4A )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = malloc( sizeof(*p_sys) ) ) == NULL )
        return VLC_ENOMEM;

    /* Open a faad context */
    if( ( p_sys->hfaad = NeAACDecOpen() ) == NULL )
    {
        msg_Err( p_dec, "cannot initialize faad" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Misc init */
    date_Set( &p_sys->date, 0 );
    p_dec->fmt_out.i_cat = AUDIO_ES;

    p_dec->fmt_out.i_codec = HAVE_FPU ? VLC_CODEC_FL32 : VLC_CODEC_S16N;

    p_dec->fmt_out.audio.i_physical_channels =
        p_dec->fmt_out.audio.i_original_channels = 0;

    if( p_dec->fmt_in.i_extra > 0 )
    {
        /* We have a decoder config so init the handle */
        unsigned long i_rate;
        unsigned char i_channels;

        if( NeAACDecInit2( p_sys->hfaad, p_dec->fmt_in.p_extra,
                           p_dec->fmt_in.i_extra,
                           &i_rate, &i_channels ) < 0 )
        {
            msg_Err( p_dec, "Failed to initialize faad using extra data" );
            NeAACDecClose( p_sys->hfaad );
            free( p_sys );
            return VLC_EGENERIC;
        }

        p_dec->fmt_out.audio.i_rate = i_rate;
        p_dec->fmt_out.audio.i_channels = i_channels;
        p_dec->fmt_out.audio.i_physical_channels
            = p_dec->fmt_out.audio.i_original_channels
            = pi_channels_guessed[i_channels];
        date_Init( &p_sys->date, i_rate, 1 );
    }
    else
    {
        /* Will be initalised from first frame */
        p_dec->fmt_out.audio.i_rate = 0;
        p_dec->fmt_out.audio.i_channels = 0;
    }

    /* Set the faad config */
    cfg = NeAACDecGetCurrentConfiguration( p_sys->hfaad );
    if( p_dec->fmt_in.audio.i_rate )
        cfg->defSampleRate = p_dec->fmt_in.audio.i_rate;
    cfg->outputFormat = HAVE_FPU ? FAAD_FMT_FLOAT : FAAD_FMT_16BIT;
    NeAACDecSetConfiguration( p_sys->hfaad, cfg );

    /* buffer */
    p_sys->i_buffer = p_sys->i_buffer_size = 0;
    p_sys->p_buffer = NULL;

    p_sys->b_sbr = p_sys->b_ps = false;

    p_dec->pf_decode = DecodeBlock;
    p_dec->pf_flush  = Flush;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Flush:
 *****************************************************************************/
static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    date_Set( &p_sys->date, 0 );
}

/*****************************************************************************
 * FlushBuffer:
 *****************************************************************************/
static void FlushBuffer( decoder_sys_t *p_sys, size_t i_used )
{
    if( i_used > p_sys->i_buffer )
    {
        /* Drop padding */
        if(p_sys->p_buffer[i_used] == 0x00)
            i_used++;

        p_sys->i_buffer -= i_used;
        if( p_sys->i_buffer > 0 )
        {
            memmove( p_sys->p_buffer, &p_sys->p_buffer[i_used],
                     p_sys->i_buffer );
        }
    }
    else p_sys->i_buffer = 0;
}

/*****************************************************************************
 * DecodeBlock:
 *****************************************************************************/
static int DecodeBlock( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( !p_block ) /* No Drain */
        return VLCDEC_SUCCESS;

    if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY | BLOCK_FLAG_CORRUPTED) )
    {
        Flush( p_dec );
        if( p_block->i_flags & (BLOCK_FLAG_CORRUPTED) )
        {
            block_Release( p_block );
            return VLCDEC_SUCCESS;
        }
    }

    /* Remove ADTS header if we have decoder specific config */
    if( p_dec->fmt_in.i_extra && p_block->i_buffer > 7 )
    {
        if( p_block->p_buffer[0] == 0xff &&
            ( p_block->p_buffer[1] & 0xf0 ) == 0xf0 ) /* syncword */
        {   /* ADTS header present */
            size_t i_header_size; /* 7 bytes (+ 2 bytes for crc) */
            i_header_size = 7 + ( ( p_block->p_buffer[1] & 0x01 ) ? 0 : 2 );
            /* FIXME: multiple blocks per frame */
            if( p_block->i_buffer > i_header_size )
            {
                p_block->p_buffer += i_header_size;
                p_block->i_buffer -= i_header_size;
            }
        }
    }

    /* Append the block to the temporary buffer */
    if( p_sys->i_buffer_size < p_sys->i_buffer + p_block->i_buffer )
    {
        size_t  i_buffer_size = p_sys->i_buffer + p_block->i_buffer;
        uint8_t *p_buffer     = realloc( p_sys->p_buffer, i_buffer_size );
        if( p_buffer )
        {
            p_sys->i_buffer_size = i_buffer_size;
            p_sys->p_buffer      = p_buffer;
        }
        else
        {
            p_block->i_buffer = 0;
        }
    }

    if( p_block->i_buffer > 0 )
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

        if( NeAACDecInit2( p_sys->hfaad, p_dec->fmt_in.p_extra,
                           p_dec->fmt_in.i_extra,
                           &i_rate, &i_channels ) >= 0 )
        {
            p_dec->fmt_out.audio.i_rate = i_rate;
            p_dec->fmt_out.audio.i_channels = i_channels;
            p_dec->fmt_out.audio.i_physical_channels
                = p_dec->fmt_out.audio.i_original_channels
                = pi_channels_guessed[i_channels];

            date_Init( &p_sys->date, i_rate, 1 );
        }
    }

    if( p_dec->fmt_out.audio.i_rate == 0 && p_sys->i_buffer )
    {
        unsigned long i_rate;
        unsigned char i_channels;

        /* Init faad with the first frame */
        if( NeAACDecInit( p_sys->hfaad,
                          p_sys->p_buffer, p_sys->i_buffer,
                          &i_rate, &i_channels ) < 0 )
        {
            block_Release( p_block );
            return VLCDEC_SUCCESS;
        }

        p_dec->fmt_out.audio.i_rate = i_rate;
        p_dec->fmt_out.audio.i_channels = i_channels;
        p_dec->fmt_out.audio.i_physical_channels
            = p_dec->fmt_out.audio.i_original_channels
            = pi_channels_guessed[i_channels];
        date_Init( &p_sys->date, i_rate, 1 );
    }

    if( p_block->i_pts > VLC_TS_INVALID && p_block->i_pts != date_Get( &p_sys->date ) )
    {
        date_Set( &p_sys->date, p_block->i_pts );
    }
    else if( !date_Get( &p_sys->date ) )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( p_block );
        p_sys->i_buffer = 0;
        return VLCDEC_SUCCESS;
    }

    /* Decode all data */
    if( p_sys->i_buffer > 1)
    {
        void *samples;
        NeAACDecFrameInfo frame;
        block_t *p_out;

        samples = NeAACDecDecode( p_sys->hfaad, &frame,
                                  p_sys->p_buffer, p_sys->i_buffer );

        if( frame.error > 0 )
        {
            msg_Warn( p_dec, "%s", NeAACDecGetErrorMessage( frame.error ) );

            if( frame.error == 21 || frame.error == 12 )
            {
                /*
                 * Once an "Unexpected channel configuration change"
                 * or a "Invalid number of channels" error
                 * occurs, it will occurs afterwards, and we got no sound.
                 * Reinitialization of the decoder is required.
                 */
                unsigned long i_rate;
                unsigned char i_channels;
                NeAACDecHandle *hfaad;
                NeAACDecConfiguration *cfg,*oldcfg;

                oldcfg = NeAACDecGetCurrentConfiguration( p_sys->hfaad );
                hfaad = NeAACDecOpen();
                cfg = NeAACDecGetCurrentConfiguration( hfaad );
                if( oldcfg->defSampleRate )
                    cfg->defSampleRate = oldcfg->defSampleRate;
                cfg->defObjectType = oldcfg->defObjectType;
                cfg->outputFormat = oldcfg->outputFormat;
                NeAACDecSetConfiguration( hfaad, cfg );

                if( NeAACDecInit( hfaad, p_sys->p_buffer, p_sys->i_buffer,
                                  &i_rate,&i_channels ) < 0 )
                {
                    /* reinitialization failed */
                    NeAACDecClose( hfaad );
                    NeAACDecSetConfiguration( p_sys->hfaad, oldcfg );
                }
                else
                {
                    NeAACDecClose( p_sys->hfaad );
                    p_sys->hfaad = hfaad;
                    p_dec->fmt_out.audio.i_rate = i_rate;
                    p_dec->fmt_out.audio.i_channels = i_channels;
                    p_dec->fmt_out.audio.i_physical_channels
                        = p_dec->fmt_out.audio.i_original_channels
                        = pi_channels_guessed[i_channels];
                    date_Init( &p_sys->date, i_rate, 1 );
                }
            }

            /* Flush the buffer */
            p_sys->i_buffer = 0;
            block_Release( p_block );
            return VLCDEC_SUCCESS;
        }

        if( frame.channels <= 0 || frame.channels > 8 || frame.channels == 7 )
        {
            msg_Warn( p_dec, "invalid channels count: %i", frame.channels );
            FlushBuffer( p_sys, frame.bytesconsumed );
            block_Release( p_block );
            return VLCDEC_SUCCESS;
        }

        if( frame.samples <= 0 )
        {
            msg_Warn( p_dec, "decoded zero sample" );
            FlushBuffer( p_sys, frame.bytesconsumed );
            block_Release( p_block );
            return VLCDEC_SUCCESS;
        }

        /* We decoded a valid frame */
        if( p_dec->fmt_out.audio.i_rate != frame.samplerate )
        {
            date_Init( &p_sys->date, frame.samplerate, 1 );
            date_Set( &p_sys->date, p_block->i_pts );
        }
        p_block->i_pts = VLC_TS_INVALID;  /* PTS is valid only once */

        p_dec->fmt_out.audio.i_rate = frame.samplerate;
        p_dec->fmt_out.audio.i_channels = frame.channels;

        /* Adjust stream info when dealing with SBR/PS */
        bool b_sbr = (frame.sbr == 1) || (frame.sbr == 2);
        if( p_sys->b_sbr != b_sbr || p_sys->b_ps != frame.ps )
        {
            const char *psz_ext = (b_sbr && frame.ps) ? "SBR+PS" :
                                    b_sbr ? "SBR" : "PS";

            msg_Dbg( p_dec, "AAC %s (channels: %u, samplerate: %lu)",
                    psz_ext, frame.channels, frame.samplerate );

            if( !p_dec->p_description )
                p_dec->p_description = vlc_meta_New();
            if( p_dec->p_description )
                vlc_meta_AddExtra( p_dec->p_description, _("AAC extension"), psz_ext );

            p_sys->b_sbr = b_sbr;
            p_sys->b_ps = frame.ps;
        }

        /* Convert frame.channel_position to our own channel values */
        p_dec->fmt_out.audio.i_physical_channels = 0;
        const uint32_t nbChannels = frame.channels;
        unsigned j;
        for( unsigned i = 0; i < nbChannels; i++ )
        {
            /* Find the channel code */
            for( j = 0; j < MAX_CHANNEL_POSITIONS; j++ )
            {
                if( frame.channel_position[i] == pi_channels_in[j] )
                    break;
            }
            if( j >= MAX_CHANNEL_POSITIONS )
            {
                msg_Warn( p_dec, "unknown channel ordering" );
                /* Invent something */
                j = i;
            }
            /* */
            p_sys->pi_channel_positions[i] = pi_channels_out[j];
            if( p_dec->fmt_out.audio.i_physical_channels & pi_channels_out[j] )
                frame.channels--; /* We loose a duplicated channel */
            else
                p_dec->fmt_out.audio.i_physical_channels |= pi_channels_out[j];
        }
        if ( nbChannels != frame.channels )
        {
            p_dec->fmt_out.audio.i_physical_channels
                = p_dec->fmt_out.audio.i_original_channels
                = pi_channels_guessed[nbChannels];
        }
        else
        {
            p_dec->fmt_out.audio.i_original_channels =
                p_dec->fmt_out.audio.i_physical_channels;
        }
        p_dec->fmt_out.audio.i_channels = nbChannels;
        if( decoder_UpdateAudioFormat( p_dec ) )
            p_out = NULL;
        else
            p_out = decoder_NewAudioBuffer( p_dec, frame.samples / nbChannels );
        if( p_out == NULL )
        {
            p_sys->i_buffer = 0;
            block_Release( p_block );
            return VLCDEC_SUCCESS;
        }

        p_out->i_pts = date_Get( &p_sys->date );
        p_out->i_length = date_Increment( &p_sys->date,
                                          frame.samples / nbChannels )
                          - p_out->i_pts;

        DoReordering( (uint32_t *)p_out->p_buffer, samples,
                      frame.samples / nbChannels, nbChannels,
                      p_sys->pi_channel_positions );

        FlushBuffer( p_sys, frame.bytesconsumed );

        block_Release( p_block );
        decoder_QueueAudio( p_dec, p_out );
        return VLCDEC_SUCCESS;
    }
    else
    {
        /* Drop byte of padding */
        p_sys->i_buffer = 0;
    }

    block_Release( p_block );
    return VLCDEC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    NeAACDecClose( p_sys->hfaad );
    free( p_sys->p_buffer );
    free( p_sys );
}

/*****************************************************************************
 * DoReordering: do some channel re-ordering (the ac3 channel order is
 *   different from the aac one).
 *****************************************************************************/
static void DoReordering( uint32_t *p_out, uint32_t *p_in, int i_samples,
                          int i_nb_channels, uint32_t *pi_chan_positions )
{
    int pi_chan_table[MAX_CHANNEL_POSITIONS] = {0};
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
    if( HAVE_FPU )
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

