/*****************************************************************************
 * faad.c: AAC decoder using libfaad2
 *****************************************************************************
 * Copyright (C) 2001, 2003 VLC authors and VideoLAN
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
#include <vlc_input_item.h>
#include <vlc_codec.h>
#include <vlc_cpu.h>
#include <vlc_aout.h>

#include <neaacdec.h>
#include "../packetizer/mpeg4audio.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("AAC audio decoder (using libfaad2)") )
    set_capability( "audio decoder", 100 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )
    set_callbacks( Open, Close )
vlc_module_end ()

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int DecodeBlock( decoder_t *, block_t * );
static void Flush( decoder_t * );
static void DoReordering( uint32_t *, uint32_t *, int, int, uint8_t * );

typedef struct
{
    /* faad handler */
    NeAACDecHandle *hfaad;

    /* samples */
    date_t date;
    vlc_tick_t i_last_length;

    /* temporary buffer */
    block_t *p_block;

    /* Channel positions of the current stream (for re-ordering) */
    uint32_t pi_channel_positions[MPEG4_ASC_MAX_INDEXEDPOS];

    bool b_sbr, b_ps, b_discontinuity;
} decoder_sys_t;

#if MPEG4_ASC_MAX_INDEXEDPOS != LFE_CHANNEL
    #error MPEG4_ASC_MAX_INDEXEDPOS != LFE_CHANNEL
#endif

#define FAAD_CHANNEL_ID_COUNT (LFE_CHANNEL + 1)
static const uint32_t pi_tovlcmapping[FAAD_CHANNEL_ID_COUNT] =
{
    [UNKNOWN_CHANNEL]      = 0,
    [FRONT_CHANNEL_CENTER] = AOUT_CHAN_CENTER,
    [FRONT_CHANNEL_LEFT]   = AOUT_CHAN_LEFT,
    [FRONT_CHANNEL_RIGHT]  = AOUT_CHAN_RIGHT,
    [SIDE_CHANNEL_LEFT]    = AOUT_CHAN_MIDDLELEFT,
    [SIDE_CHANNEL_RIGHT]   = AOUT_CHAN_MIDDLERIGHT,
    [BACK_CHANNEL_LEFT]    = AOUT_CHAN_REARLEFT,
    [BACK_CHANNEL_RIGHT]   = AOUT_CHAN_REARRIGHT,
    [BACK_CHANNEL_CENTER]  = AOUT_CHAN_REARCENTER,
    [LFE_CHANNEL]          = AOUT_CHAN_LFE
};

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;
    NeAACDecConfiguration *cfg;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_MP4A ||
        p_dec->fmt_in.i_profile == AAC_PROFILE_ELD ||
        (p_dec->fmt_in.i_extra > 1 &&
         (GetWBE(p_dec->fmt_in.p_extra) & 0xffe0) == 0xf8e0)) /* ELD AOT */
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
    p_dec->fmt_out.audio.channel_type = p_dec->fmt_in.audio.channel_type;

    if( p_dec->fmt_in.i_extra > 0 )
    {
        /* We have a decoder config so init the handle */
        unsigned long i_rate;
        unsigned char i_channels;

        if( NeAACDecInit2( p_sys->hfaad, p_dec->fmt_in.p_extra,
                           p_dec->fmt_in.i_extra,
                           &i_rate, &i_channels ) < 0 ||
                i_channels >= MPEG4_ASC_MAX_INDEXEDPOS )
        {
            msg_Err( p_dec, "Failed to initialize faad using extra data" );
            NeAACDecClose( p_sys->hfaad );
            free( p_sys );
            return VLC_EGENERIC;
        }

        p_dec->fmt_out.audio.i_rate = i_rate;
        p_dec->fmt_out.audio.i_channels = i_channels;
        p_dec->fmt_out.audio.i_physical_channels
            = mpeg4_asc_channelsbyindex[i_channels];
        date_Init( &p_sys->date, i_rate, 1 );
    }
    else
    {
        p_dec->fmt_out.audio.i_physical_channels = 0;
        /* Will be initalised from first frame */
        p_dec->fmt_out.audio.i_rate = 0;
        p_dec->fmt_out.audio.i_channels = 0;
        date_Set( &p_sys->date, VLC_TICK_INVALID );
    }

    p_dec->fmt_out.i_codec = HAVE_FPU ? VLC_CODEC_FL32 : VLC_CODEC_S16N;
    p_dec->fmt_out.audio.i_chan_mode = p_dec->fmt_in.audio.i_chan_mode;

    /* Set the faad config */
    cfg = NeAACDecGetCurrentConfiguration( p_sys->hfaad );
    if( p_dec->fmt_in.audio.i_rate )
        cfg->defSampleRate = p_dec->fmt_in.audio.i_rate;
    cfg->outputFormat = HAVE_FPU ? FAAD_FMT_FLOAT : FAAD_FMT_16BIT;
    NeAACDecSetConfiguration( p_sys->hfaad, cfg );

    p_sys->i_last_length = 0;

    /* buffer */
    p_sys->p_block = NULL;

    p_sys->b_discontinuity =
    p_sys->b_sbr = p_sys->b_ps = false;

    p_dec->pf_decode = DecodeBlock;
    p_dec->pf_flush  = Flush;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * FlushBuffer:
 *****************************************************************************/
static void FlushBuffer( decoder_sys_t *p_sys, size_t i_used )
{
    block_t *p_block = p_sys->p_block;
    if( p_block )
    {
        if( i_used < p_block->i_buffer )
        {
            /* Drop padding */
            for( ; i_used < p_block->i_buffer; i_used++ )
                if( p_block->p_buffer[i_used] != 0x00 )
                    break;

            p_block->i_buffer -= i_used;
            p_block->p_buffer += i_used;
        }
        else p_block->i_buffer = 0;
        if( p_block->i_buffer == 0 )
        {
            block_Release( p_block );
            p_sys->p_block = NULL;
        }
    }
}

/*****************************************************************************
 * Flush:
 *****************************************************************************/
static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    date_Set( &p_sys->date, VLC_TICK_INVALID );
    FlushBuffer( p_sys, SIZE_MAX );
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

    const vlc_tick_t i_pts = p_block->i_pts;

    /* Append block as temporary buffer */
    if( p_sys->p_block == NULL )
    {
        p_sys->p_block = p_block;
    }
    else
    {
        p_sys->p_block->p_next = p_block;
        block_t *p_prev = p_sys->p_block;
        p_sys->p_block = block_ChainGather( p_sys->p_block );
        if( p_sys->p_block == NULL )
            block_ChainRelease( p_prev );
    }

    /* !Warn: do not use p_block beyond this point */

    if( p_dec->fmt_out.audio.i_rate == 0 )
    {
        unsigned long i_rate = 0;
        unsigned char i_channels;

        /* Init from DecoderConfig */
        if( p_dec->fmt_in.i_extra > 0 &&
            NeAACDecInit2( p_sys->hfaad, p_dec->fmt_in.p_extra,
                           p_dec->fmt_in.i_extra, &i_rate, &i_channels ) != 0 )
        {
            /* Failed, will try from data */
            i_rate = 0;
        }

        if( i_rate == 0 && p_sys->p_block && p_sys->p_block->i_buffer )
        {
            /* Init faad with the first frame */
            long i_read = NeAACDecInit( p_sys->hfaad,
                                        p_sys->p_block->p_buffer, p_sys->p_block->i_buffer,
                                        &i_rate, &i_channels );
            if( i_read < 0 || (size_t) i_read > p_sys->p_block->i_buffer )
                i_rate = 0;
            else
                FlushBuffer( p_sys, i_read );
        }

        if( i_rate == 0 )
        {
            /* Can not init decoder at all for now */
            FlushBuffer( p_sys, SIZE_MAX );
            return VLCDEC_SUCCESS;
        }

        /* Decoder Initialized */
        p_dec->fmt_out.audio.i_rate = i_rate;
        p_dec->fmt_out.audio.i_channels = i_channels;
        p_dec->fmt_out.audio.i_physical_channels
            = mpeg4_asc_channelsbyindex[i_channels];
        date_Init( &p_sys->date, i_rate, 1 );
    }

    if( i_pts != VLC_TICK_INVALID && i_pts != date_Get( &p_sys->date ) )
    {
        if( p_sys->i_last_length == 0 ||
            /* We need to be permissive and rebase dts when it's really way off */
            llabs( i_pts - date_Get( &p_sys->date ) ) > p_sys->i_last_length * 3 / 2  )
            date_Set( &p_sys->date, i_pts );
    }
    else if( date_Get( &p_sys->date ) == VLC_TICK_INVALID )
    {
        /* We've just started the stream, wait for the first PTS. */
        FlushBuffer( p_sys, SIZE_MAX );
        return VLCDEC_SUCCESS;
    }

    /* Decode all data */
    while( p_sys->p_block && p_sys->p_block->i_buffer > 0 )
    {
        void *samples;
        NeAACDecFrameInfo frame;
        block_t *p_out = NULL;

        samples = NeAACDecDecode( p_sys->hfaad, &frame,
                                  p_sys->p_block->p_buffer,
                                  p_sys->p_block->i_buffer );

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

                if( NeAACDecInit( hfaad,
                                  p_sys->p_block->p_buffer,
                                  p_sys->p_block->i_buffer,
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
                        = mpeg4_asc_channelsbyindex[i_channels];
                    date_Init( &p_sys->date, i_rate, 1 );
                }
            }

            Flush( p_dec );
            p_sys->b_discontinuity = true;

            continue;
        }

        if( frame.channels == 0 || frame.channels >= 64 )
        {
            msg_Warn( p_dec, "invalid channels count: %i", frame.channels );
            if( frame.channels == 0 )
                p_sys->b_discontinuity = true;
            FlushBuffer( p_sys, frame.bytesconsumed ? frame.bytesconsumed : SIZE_MAX );
            continue;
        }

        if( frame.samples == 0 )
        {
            msg_Warn( p_dec, "decoded zero sample" );
            FlushBuffer( p_sys, frame.bytesconsumed ? frame.bytesconsumed : SIZE_MAX );
            continue;
        }

        /* We decoded a valid frame */
        if( p_dec->fmt_out.audio.i_rate != frame.samplerate )
        {
            date_Init( &p_sys->date, frame.samplerate, 1 );
            date_Set( &p_sys->date, i_pts );
        }

        p_dec->fmt_out.audio.i_rate = frame.samplerate;

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

#ifndef FAAD2_VIDEOLAN_PATCHED
        /* PS Enabled FAAD PCA bug hotfix (contribs has patch) */
        if( frame.channels == 8 )
        {
            const uint8_t psbugconfig[3][8] = { { 2, 3, 2, 3, 2, 3, 6, 7 },  /* fdk 7.1 (4 Front) */
                                                { 2, 3, 2, 3, 2, 3, 4, 5 },  /* 7.1 */
                                                { 1, 2, 3, 4, 5, 6, 7, 9 } };/* fixed */
            for( size_t i=0; i<2; i++ )
            {
                if( !memcmp( frame.channel_position, psbugconfig[i], 8 ) )
                {
                    msg_Warn( p_dec, "Unpatched FAAD2 library with PS Bug. Trying to workaround !" );
                    memcpy( frame.channel_position, psbugconfig[2], 8 );
                    break;
                }
            }
        }

        /* Hotfix channels misdetection/repetition for FDK 7.1 */
        struct
        {
            const uint8_t chans;
            const uint8_t faulty[8];
            const uint8_t fixed[8];
        } const channel_repeat_fixes[] = {
            { 7, { 2, 3, 2, 3, 2, 3, 6 },    { 1, 2, 3, 6, 7, 8, 9 }    }, /* 3F 3R LFE #18273 */
            { 8, { 1, 2, 3, 6, 7, 6, 7, 9 }, { 1, 2, 3, 6, 7, 4, 5, 9 } }, /* FDK encoded 7.1 */
        };

        for( size_t i=0; i<ARRAY_SIZE(channel_repeat_fixes); i++ )
        {
            if( channel_repeat_fixes[i].chans == frame.channels &&
                !memcmp( frame.channel_position, channel_repeat_fixes[i].faulty,
                                                 channel_repeat_fixes[i].chans ) )
            {
                msg_Warn( p_dec, "Patching for Front channel repeat bug" );
                memcpy( frame.channel_position, channel_repeat_fixes[i].fixed,
                                                channel_repeat_fixes[i].chans );
                break;
            }
        }
#endif
        /* Handle > 1 local pair 5.1 setups.
           In case of more than 1 channel pair per area, faad will have repeats
           in channels sequence. We need to remap to available surround channels.
           Front > Middle > Rear:
           In case of 4 middle, it maps to 2F 2M if no previous front.
           In case of 4 rear, it maps to 2M 2R if no previous rear.
        */
        unsigned i_faadused = 0;
        for( unsigned i=0; i<frame.channels; i++ )
            if( frame.channel_position[i] > 0 )
                i_faadused |= 1 << frame.channel_position[i];

        for( size_t i=3; i<frame.channels; i++ )
        {
             if( frame.channel_position[i - 3] == frame.channel_position[i - 1] &&
                 frame.channel_position[i - 2] == frame.channel_position[i] &&
                 frame.channel_position[i - 1] >= SIDE_CHANNEL_LEFT &&
                 frame.channel_position[i - 1] <= BACK_CHANNEL_CENTER &&
                 frame.channel_position[i - 1] >= SIDE_CHANNEL_LEFT &&
                 frame.channel_position[i - 1] <= BACK_CHANNEL_CENTER )
             {
                if( ( (1 << (frame.channel_position[i - 3] - 2)) & i_faadused ) == 0 &&
                    ( (1 << (frame.channel_position[i - 2] - 2)) & i_faadused ) == 0 )
                {
                    frame.channel_position[i - 3] -= 2;
                    frame.channel_position[i - 2] -= 2;
                    i_faadused |= 1 << frame.channel_position[i - 3];
                    i_faadused |= 1 << frame.channel_position[i - 2];
                }
             }
        }

        /* Convert frame.channel_position to our own channel values */
        p_dec->fmt_out.audio.i_physical_channels = 0;

        uint8_t  pi_neworder_table[64] = {0};
        uint32_t pi_faad_channels_positions[64 + 1] = {0};

        bool b_reorder = false;
        if (p_dec->fmt_out.audio.channel_type == AUDIO_CHANNEL_TYPE_BITMAP)
        {
            for( size_t i = 0; i < frame.channels; i++ )
            {
                unsigned pos = frame.channel_position[i];
                if( likely(pos < FAAD_CHANNEL_ID_COUNT) )
                {
                    pi_faad_channels_positions[i] = pi_tovlcmapping[pos];
                    p_dec->fmt_out.audio.i_physical_channels |= pi_faad_channels_positions[i];
                }
                else pi_faad_channels_positions[i] = 0;
            }
        }
        else if (p_dec->fmt_out.audio.channel_type == AUDIO_CHANNEL_TYPE_AMBISONICS
            && frame.channels == 4)
        {
            pi_faad_channels_positions[0] = AOUT_CHAN_REARCENTER;
            pi_faad_channels_positions[1] = AOUT_CHAN_LEFT;
            pi_faad_channels_positions[2] = AOUT_CHAN_RIGHT;
            pi_faad_channels_positions[3] = AOUT_CHAN_CENTER;
            p_dec->fmt_out.audio.i_physical_channels =
                AOUT_CHAN_CENTER | AOUT_CHAN_LEFT
                | AOUT_CHAN_RIGHT | AOUT_CHAN_REARCENTER;
        }

        b_reorder = aout_CheckChannelReorder( pi_faad_channels_positions, NULL,
            p_dec->fmt_out.audio.i_physical_channels, pi_neworder_table );

        p_dec->fmt_out.audio.i_channels = vlc_popcount(p_dec->fmt_out.audio.i_physical_channels);

        if( !decoder_UpdateAudioFormat( p_dec ) && p_dec->fmt_out.audio.i_channels > 0 )
            p_out = decoder_NewAudioBuffer( p_dec, frame.samples / p_dec->fmt_out.audio.i_channels );

        if( p_out )
        {
            p_out->i_pts = date_Get( &p_sys->date );
            p_out->i_length = date_Increment( &p_sys->date,
                                              frame.samples / frame.channels )
                              - p_out->i_pts;
            p_sys->i_last_length = p_out->i_length;
            if ( p_dec->fmt_out.audio.channel_type == AUDIO_CHANNEL_TYPE_BITMAP )
            {
                /* Don't kill speakers if some weird mapping does not gets 1:1 */
                if( vlc_popcount(p_dec->fmt_out.audio.i_physical_channels) != frame.channels )
                    memset( p_out->p_buffer, 0, p_out->i_buffer );
            }

            /* FIXME: replace when aout_channel_reorder can take samples from a different buffer */
            if( b_reorder )
                DoReordering( (uint32_t *)p_out->p_buffer, samples,
                              frame.samples / frame.channels, frame.channels,
                              pi_neworder_table );
            else
                 memcpy( p_out->p_buffer, samples, p_out->i_buffer );

            if( p_sys->b_discontinuity )
            {
                p_out->i_flags |= BLOCK_FLAG_DISCONTINUITY;
                p_sys->b_discontinuity = false;
            }

            decoder_QueueAudio( p_dec, p_out );
        }
        else
        {
            date_Increment( &p_sys->date, frame.samples / frame.channels );
        }

        FlushBuffer( p_sys, frame.bytesconsumed ? frame.bytesconsumed : SIZE_MAX );

        if( p_sys->p_block && p_sys->p_block->i_buffer == 1 )
        {
            /* Drop byte of padding */
            FlushBuffer( p_sys, 0 );
        }
    }

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
    FlushBuffer( p_sys, SIZE_MAX );
    free( p_sys );
}

/*****************************************************************************
 * DoReordering: do some channel re-ordering (the ac3 channel order is
 *   different from the aac one).
 *****************************************************************************/
static void DoReordering( uint32_t *p_out, uint32_t *p_in, int i_samples,
                          int i_nb_channels, uint8_t *pi_chan_positions )
{
#if HAVE_FPU
    #define CAST_SAMPLE(a) a
#else
    #define CAST_SAMPLE(a) ((uint16_t *)a)
#endif
    /* Do the actual reordering */
    for( int i = 0; i < i_samples; i++ )
    {
        for( int j = 0; j < i_nb_channels; j++ )
        {
            CAST_SAMPLE(p_out)[i * i_nb_channels + pi_chan_positions[j]] =
                CAST_SAMPLE(p_in)[i * i_nb_channels + j];
        }
    }
}

