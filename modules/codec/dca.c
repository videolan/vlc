/*****************************************************************************
 * dca.c: DTS Coherent Acoustics decoder plugin for VLC.
 *   This plugin makes use of libdca to do the actual decoding
 *   (http://developers.videolan.org/libdca.html).
 *****************************************************************************
 * Copyright (C) 2001, 2016 libdca VLC authors and VideoLAN
 *
 * Author: Gildas Bazin <gbazin@videolan.org>
 *         Thomas Guillem <thomas@gllm.fr>
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

#include <dca.h>                                       /* libdca header file */

#include <vlc_aout.h>
#include <vlc_block.h>
#include <vlc_codec.h>

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

typedef struct
{
    dca_state_t     *p_libdca; /* libdca internal structure */
    bool            b_dynrng; /* see below */
    int             i_flags; /* libdca flags, see dtsdec/doc/libdts.txt */
    bool            b_dontwarn;
    int             i_nb_channels; /* number of float32 per sample */

    uint8_t         pi_chan_table[AOUT_CHAN_MAX]; /* channel reordering */
    bool            b_synced;
} decoder_sys_t;

#define DYNRNG_TEXT N_("DTS dynamic range compression")
#define DYNRNG_LONGTEXT N_( \
    "Dynamic range compression makes the loud sounds softer, and the soft " \
    "sounds louder, so you can more easily listen to the stream in a noisy " \
    "environment without disturbing anyone. If you disable the dynamic range "\
    "compression the playback will be more adapted to a movie theater or a " \
    "listening room.")

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )
    set_shortname( "DCA" )
    set_description( N_("DTS Coherent Acoustics audio decoder") )
    add_bool( "dts-dynrng", true, DYNRNG_TEXT, DYNRNG_LONGTEXT, false )
    set_capability( "audio decoder", 60 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*
 * helper function to interleave channels
 */
static void Interleave( float * p_out, const float * p_in, int i_nb_channels,
                        uint8_t *pi_chan_table )
{
    /* We do not only have to interleave, but also reorder the channels. */

    int i, j;
    for ( j = 0; j < i_nb_channels; j++ )
    {
        for ( i = 0; i < 256; i++ )
        {
            p_out[i * i_nb_channels + pi_chan_table[j]] = p_in[j * 256 + i];
        }
    }
}

/*
 * helper function to duplicate a unique channel
 */
static void Duplicate( float * p_out, const float * p_in )
{
    for ( int i = 256; i--; )
    {
        *p_out++ = *p_in;
        *p_out++ = *p_in;
        p_in++;
    }
}

static int Decode( decoder_t *p_dec, block_t *p_in_buf )
{
    decoder_sys_t  *p_sys = p_dec->p_sys;

    if (p_in_buf == NULL) /* No Drain */
        return VLCDEC_SUCCESS;

    sample_t i_sample_level = 1;
    int  i_flags = p_sys->i_flags;
    size_t i_bytes_per_block = 256 * p_sys->i_nb_channels * sizeof(float);

    block_t *p_out_buf = block_Alloc( 6 * i_bytes_per_block );
    if( unlikely(p_out_buf == NULL) )
        goto out;

    /*
     * Do the actual decoding now.
     */

    /* Needs to be called so the decoder knows which type of bitstream it is
     * dealing with. */
    int i_sample_rate, i_bit_rate, i_frame_length;
    if( !dca_syncinfo( p_sys->p_libdca, p_in_buf->p_buffer, &i_flags,
                       &i_sample_rate, &i_bit_rate, &i_frame_length ) )
    {
        msg_Warn( p_dec, "libdca couldn't sync on frame" );
        p_out_buf->i_nb_samples = p_out_buf->i_buffer = 0;
        goto out;
    }

    i_flags = p_sys->i_flags;
    dca_frame( p_sys->p_libdca, p_in_buf->p_buffer,
               &i_flags, &i_sample_level, 0 );

    if ( (i_flags & DCA_CHANNEL_MASK) != (p_sys->i_flags & DCA_CHANNEL_MASK)
          && !p_sys->b_dontwarn )
    {
        msg_Warn( p_dec,
                  "libdca couldn't do the requested downmix 0x%x->0x%x",
                  p_sys->i_flags  & DCA_CHANNEL_MASK,
                  i_flags & DCA_CHANNEL_MASK );

        p_sys->b_dontwarn = 1;
    }

    for( int i = 0; i < dca_blocks_num(p_sys->p_libdca); i++ )
    {
        sample_t * p_samples;

        if( dca_block( p_sys->p_libdca ) )
        {
            msg_Warn( p_dec, "dca_block failed for block %d", i );
            break;
        }

        p_samples = dca_samples( p_sys->p_libdca );

        if ( (p_sys->i_flags & DCA_CHANNEL_MASK) == DCA_MONO
              && (p_dec->fmt_out.audio.i_physical_channels
                   & (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT)) )
        {
            Duplicate( (float *)(p_out_buf->p_buffer + i * i_bytes_per_block),
                       p_samples );
        }
        else
        {
            /* Interleave the *$£%ù samples. */
            Interleave( (float *)(p_out_buf->p_buffer + i * i_bytes_per_block),
                        p_samples, p_sys->i_nb_channels, p_sys->pi_chan_table);
        }
    }

    p_out_buf->i_nb_samples = dca_blocks_num(p_sys->p_libdca) * 256;
    p_out_buf->i_buffer = p_in_buf->i_nb_samples * sizeof(float) * p_sys->i_nb_channels;
    p_out_buf->i_dts = p_in_buf->i_dts;
    p_out_buf->i_pts = p_in_buf->i_pts;
    p_out_buf->i_length = p_in_buf->i_length;
out:
    if (p_out_buf != NULL)
        decoder_QueueAudio(p_dec, p_out_buf);
    block_Release( p_in_buf );
    return VLCDEC_SUCCESS;
}

static int channels_vlc2dca( const audio_format_t *p_audio, int *p_flags )
{
    int i_flags = 0;

    switch ( p_audio->i_physical_channels & ~AOUT_CHAN_LFE )
    {
    case AOUT_CHAN_CENTER:
        if ( (p_audio->i_physical_channels & AOUT_CHAN_CENTER)
              || (p_audio->i_physical_channels
                   & (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT)) )
            i_flags = DCA_MONO;
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT:
        if ( p_audio->i_chan_mode & AOUT_CHANMODE_DOLBYSTEREO )
            i_flags = DCA_DOLBY;
        else if ( p_audio->i_chan_mode & AOUT_CHANMODE_DUALMONO )
            i_flags = DCA_CHANNEL;
        else
            i_flags = DCA_STEREO;
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER:
        i_flags = DCA_3F;
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARCENTER:
        i_flags = DCA_2F1R;
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARCENTER:
        i_flags = DCA_3F1R;
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT:
        i_flags = DCA_2F2R;
        break;

    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT:
        i_flags = DCA_3F2R;
        break;

    default:
        return VLC_EGENERIC;
    }

    if ( p_audio->i_physical_channels & AOUT_CHAN_LFE )
        i_flags |= DCA_LFE;
    *p_flags = i_flags;
    return VLC_SUCCESS;
}

static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_DTS
     || p_dec->fmt_in.audio.i_rate == 0
     || p_dec->fmt_in.audio.i_physical_channels == 0
     || p_dec->fmt_in.audio.i_bytes_per_frame == 0
     || p_dec->fmt_in.audio.i_frame_length == 0 )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the module's structure */
    p_sys = p_dec->p_sys = malloc( sizeof(decoder_sys_t) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_sys->b_dynrng = var_InheritBool( p_this, "dts-dynrng" );
    p_sys->b_dontwarn = 0;

    /* We'll do our own downmixing, thanks. */
    p_sys->i_nb_channels = aout_FormatNbChannels( &p_dec->fmt_in.audio );
    if( channels_vlc2dca( &p_dec->fmt_in.audio, &p_sys->i_flags )
        != VLC_SUCCESS )
    {
        msg_Warn( p_this, "unknown sample format!" );
        free( p_sys );
        return VLC_EGENERIC;
    }
    //p_sys->i_flags |= DCA_ADJUST_LEVEL;

    /* Initialize libdca */
    p_sys->p_libdca = dca_init( 0 );
    if( p_sys->p_libdca == NULL )
    {
        msg_Err( p_this, "unable to initialize libdca" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* libdca channel order
     * libdca currently only decodes 5.1, even if you have a DTS-ES source. */
    static const uint32_t pi_channels_in[] = {
        AOUT_CHAN_CENTER, AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT,
        AOUT_CHAN_REARCENTER, AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,
        AOUT_CHAN_LFE, 0
    };

    aout_CheckChannelReorder( pi_channels_in, NULL,
                              p_dec->fmt_in.audio.i_physical_channels,
                              p_sys->pi_chan_table );

    p_dec->fmt_out.audio = p_dec->fmt_in.audio;
    p_dec->fmt_out.audio.i_format = VLC_CODEC_FL32;
    p_dec->fmt_out.i_codec = p_dec->fmt_out.audio.i_format;

    aout_FormatPrepare( &p_dec->fmt_out.audio );

    if( decoder_UpdateAudioFormat( p_dec ) )
    {
        Close( p_this );
        return VLC_EGENERIC;
    }

    p_dec->pf_decode = Decode;
    p_dec->pf_flush  = NULL;
    return VLC_SUCCESS;
}

static void Close( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    dca_free( p_sys->p_libdca );
    free( p_sys );
}
