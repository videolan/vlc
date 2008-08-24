/*****************************************************************************
 * twolame.c: libtwolame encoder (MPEG-1/2 layer II) module
 *            (using libtwolame from http://users.tpg.com.au/adslblvi/)
 *****************************************************************************
 * Copyright (C) 2004-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
#include <vlc_sout.h>
#include <vlc_aout.h>

#include <twolame.h>

#define MPEG_FRAME_SIZE 1152
#define MAX_CODED_FRAME_SIZE 1792

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int OpenEncoder   ( vlc_object_t * );
static void CloseEncoder ( vlc_object_t * );
static block_t *Encode   ( encoder_t *, aout_buffer_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ENC_CFG_PREFIX "sout-twolame-"

#define ENC_QUALITY_TEXT N_("Encoding quality")
#define ENC_QUALITY_LONGTEXT N_( \
  "Force a specific encoding quality between 0.0 (high) and 50.0 (low), " \
  "instead of specifying a particular bitrate. " \
  "This will produce a VBR stream." )
#define ENC_MODE_TEXT N_("Stereo mode")
#define ENC_MODE_LONGTEXT N_( "Handling mode for stereo streams" )
#define ENC_VBR_TEXT N_("VBR mode")
#define ENC_VBR_LONGTEXT N_( \
  "Use Variable BitRate. Default is to use Constant BitRate (CBR)." )
#define ENC_PSY_TEXT N_("Psycho-acoustic model")
#define ENC_PSY_LONGTEXT N_( \
  "Integer from -1 (no model) to 4." )

static const int pi_stereo_values[] = { 0, 1, 2 };
static const char *const ppsz_stereo_descriptions[] =
{ N_("Stereo"), N_("Dual mono"), N_("Joint stereo") };


vlc_module_begin();
    set_shortname( "Twolame");
    set_description( N_("Libtwolame audio encoder") );
    set_capability( "encoder", 50 );
    set_callbacks( OpenEncoder, CloseEncoder );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACODEC );

    add_float( ENC_CFG_PREFIX "quality", 0.0, NULL, ENC_QUALITY_TEXT,
               ENC_QUALITY_LONGTEXT, false );
    add_integer( ENC_CFG_PREFIX "mode", 0, NULL, ENC_MODE_TEXT,
                 ENC_MODE_LONGTEXT, false );
        change_integer_list( pi_stereo_values, ppsz_stereo_descriptions, NULL );
    add_bool( ENC_CFG_PREFIX "vbr", 0, NULL, ENC_VBR_TEXT,
              ENC_VBR_LONGTEXT, false );
    add_integer( ENC_CFG_PREFIX "psy", 3, NULL, ENC_PSY_TEXT,
                 ENC_PSY_LONGTEXT, false );
vlc_module_end();

static const char *const ppsz_enc_options[] = {
    "quality", "mode", "vbr", "psy", NULL
};

/*****************************************************************************
 * encoder_sys_t : twolame encoder descriptor
 *****************************************************************************/
struct encoder_sys_t
{
    /*
     * Input properties
     */
    int16_t p_buffer[MPEG_FRAME_SIZE * 2];
    int i_nb_samples;
    mtime_t i_pts;

    /*
     * libtwolame properties
     */
    twolame_options *p_twolame;
    unsigned char p_out_buffer[MAX_CODED_FRAME_SIZE];
};

/*****************************************************************************
 * OpenEncoder: probe the encoder and return score
 *****************************************************************************/
static const uint16_t mpa_bitrate_tab[2][15] =
{
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384},
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}
};

static const uint16_t mpa_freq_tab[6] =
{ 44100, 48000, 32000, 22050, 24000, 16000 };

static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;
    vlc_value_t val;
    int i_frequency;

    if( p_enc->fmt_out.i_codec != VLC_FOURCC('m','p','g','a') &&
        p_enc->fmt_out.i_codec != VLC_FOURCC('m','p','2','a') &&
        p_enc->fmt_out.i_codec != VLC_FOURCC('m','p','2',' ') &&
        !p_enc->b_force )
    {
        return VLC_EGENERIC;
    }

    if( p_enc->fmt_in.audio.i_channels > 2 )
    {
        msg_Err( p_enc, "doesn't support > 2 channels" );
        return VLC_EGENERIC;
    }

    for ( i_frequency = 0; i_frequency < 6; i_frequency++ )
    {
        if ( p_enc->fmt_out.audio.i_rate == mpa_freq_tab[i_frequency] )
            break;
    }
    if ( i_frequency == 6 )
    {
        msg_Err( p_enc, "MPEG audio doesn't support frequency=%d",
                 p_enc->fmt_out.audio.i_rate );
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_sys = (encoder_sys_t *)malloc(sizeof(encoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;
    p_enc->p_sys = p_sys;

    p_enc->pf_encode_audio = Encode;
    p_enc->fmt_in.i_codec = AOUT_FMT_S16_NE;
    p_enc->fmt_out.i_codec = VLC_FOURCC('m','p','g','a');

    config_ChainParse( p_enc, ENC_CFG_PREFIX, ppsz_enc_options, p_enc->p_cfg );

    p_sys->p_twolame = twolame_init();

    /* Set options */
    twolame_set_in_samplerate( p_sys->p_twolame, p_enc->fmt_out.audio.i_rate );
    twolame_set_out_samplerate( p_sys->p_twolame, p_enc->fmt_out.audio.i_rate );

    var_Get( p_enc, ENC_CFG_PREFIX "vbr", &val );
    if ( val.b_bool )
    {
        float i_quality;
        var_Get( p_enc, ENC_CFG_PREFIX "quality", &val );
        i_quality = val.i_int;
        if ( i_quality > 50.0 ) i_quality = 50.0;
        if ( i_quality < 0.0 ) i_quality = 0.0;
        twolame_set_VBR( p_sys->p_twolame, 1 );
        twolame_set_VBR_q( p_sys->p_twolame, i_quality );
    }
    else
    {
        int i;
        for ( i = 1; i < 14; i++ )
        {
            if ( p_enc->fmt_out.i_bitrate / 1000
                  <= mpa_bitrate_tab[i_frequency / 3][i] )
                break;
        }
        if ( p_enc->fmt_out.i_bitrate / 1000
              != mpa_bitrate_tab[i_frequency / 3][i] )
        {
            msg_Warn( p_enc, "MPEG audio doesn't support bitrate=%d, using %d",
                      p_enc->fmt_out.i_bitrate,
                      mpa_bitrate_tab[i_frequency / 3][i] * 1000 );
            p_enc->fmt_out.i_bitrate = mpa_bitrate_tab[i_frequency / 3][i]
                                        * 1000;
        }

        twolame_set_bitrate( p_sys->p_twolame,
                             p_enc->fmt_out.i_bitrate / 1000 );
    }

    if ( p_enc->fmt_in.audio.i_channels == 1 )
    {
        twolame_set_num_channels( p_sys->p_twolame, 1 );
        twolame_set_mode( p_sys->p_twolame, TWOLAME_MONO );
    }
    else
    {
        twolame_set_num_channels( p_sys->p_twolame, 2 );
        var_Get( p_enc, ENC_CFG_PREFIX "mode", &val );
        switch ( val.i_int )
        {
        case 1:
            twolame_set_mode( p_sys->p_twolame, TWOLAME_DUAL_CHANNEL );
            break;
        case 2:
            twolame_set_mode( p_sys->p_twolame, TWOLAME_JOINT_STEREO );
            break;
        case 0:
        default:
            twolame_set_mode( p_sys->p_twolame, TWOLAME_STEREO );
            break;
        }
    }

    var_Get( p_enc, ENC_CFG_PREFIX "psy", &val );
    twolame_set_psymodel( p_sys->p_twolame, val.i_int );

    if ( twolame_init_params( p_sys->p_twolame ) )
    {
        msg_Err( p_enc, "twolame initialization failed" );
        return -VLC_EGENERIC;
    }

    p_sys->i_nb_samples = 0;

    return VLC_SUCCESS;
}

/****************************************************************************
 * Encode: the whole thing
 ****************************************************************************
 * This function spits out MPEG packets.
 ****************************************************************************/
static void Bufferize( encoder_t *p_enc, int16_t *p_in, int i_nb_samples )
{
    int16_t *p_buffer = p_enc->p_sys->p_buffer
                         + (p_enc->p_sys->i_nb_samples
                             * p_enc->fmt_in.audio.i_channels);

    memcpy( p_buffer, p_in, i_nb_samples * p_enc->fmt_in.audio.i_channels
                             * sizeof(int16_t) );
}

static block_t *Encode( encoder_t *p_enc, aout_buffer_t *p_aout_buf )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    int16_t *p_buffer = (int16_t *)p_aout_buf->p_buffer;
    int i_nb_samples = p_aout_buf->i_nb_samples;
    block_t *p_chain = NULL;

    p_sys->i_pts = p_aout_buf->start_date -
                (mtime_t)1000000 * (mtime_t)p_sys->i_nb_samples /
                (mtime_t)p_enc->fmt_out.audio.i_rate;

    while ( p_sys->i_nb_samples + i_nb_samples >= MPEG_FRAME_SIZE )
    {
        int i_used;
        block_t *p_block;

        Bufferize( p_enc, p_buffer, MPEG_FRAME_SIZE - p_sys->i_nb_samples );
        i_nb_samples -= MPEG_FRAME_SIZE - p_sys->i_nb_samples;
        p_buffer += (MPEG_FRAME_SIZE - p_sys->i_nb_samples) * 2;

        i_used = twolame_encode_buffer_interleaved( p_sys->p_twolame,
                               p_sys->p_buffer, MPEG_FRAME_SIZE,
                               p_sys->p_out_buffer, MAX_CODED_FRAME_SIZE );
        p_sys->i_nb_samples = 0;
        p_block = block_New( p_enc, i_used );
        vlc_memcpy( p_block->p_buffer, p_sys->p_out_buffer, i_used );
        p_block->i_length = (mtime_t)1000000 *
                (mtime_t)MPEG_FRAME_SIZE / (mtime_t)p_enc->fmt_out.audio.i_rate;
        p_block->i_dts = p_block->i_pts = p_sys->i_pts;
        p_sys->i_pts += p_block->i_length;
        block_ChainAppend( &p_chain, p_block );
    }

    if ( i_nb_samples )
    {
        Bufferize( p_enc, p_buffer, i_nb_samples );
        p_sys->i_nb_samples += i_nb_samples;
    }

    return p_chain;
}

/*****************************************************************************
 * CloseEncoder: twolame encoder destruction
 *****************************************************************************/
static void CloseEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    twolame_close( &p_sys->p_twolame );

    free( p_sys );
}
