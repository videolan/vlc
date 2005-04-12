/*****************************************************************************
 * toolame.c: libtoolame encoder (MPEG-1/2 layer II) module
 *            (using libtoolame from http://users.tpg.com.au/adslblvi/)
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include <vlc/sout.h>
#include <vlc/aout.h>

#include <toolame.h>

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
#define ENC_CFG_PREFIX "sout-toolame-"

#define ENC_QUALITY_TEXT N_("Encoding quality")
#define ENC_QUALITY_LONGTEXT N_( \
  "Allows you to specify a quality between 0.0 (high) and 50.0 (low), " \
  "instead of specifying a particular bitrate. " \
  "This will produce a VBR stream." )
#define ENC_MODE_TEXT N_("Stereo mode")
#define ENC_MODE_LONGTEXT N_( \
  "[0=stereo, 1=dual-mono, 2=joint-stereo]" )
#define ENC_VBR_TEXT N_("VBR mode")
#define ENC_VBR_LONGTEXT N_( \
  "By default the encoding is CBR." )

vlc_module_begin();
    set_shortname( "toolame");
    set_description( _("Libtoolame audio encoder") );
    set_capability( "encoder", 50 );
    set_callbacks( OpenEncoder, CloseEncoder );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACODEC );

    add_float( ENC_CFG_PREFIX "quality", 0.0, NULL, ENC_QUALITY_TEXT,
               ENC_QUALITY_LONGTEXT, VLC_FALSE );
    add_integer( ENC_CFG_PREFIX "mode", 0, NULL, ENC_MODE_TEXT,
                 ENC_MODE_LONGTEXT, VLC_FALSE );
    add_bool( ENC_CFG_PREFIX "vbr", 0, NULL, ENC_VBR_TEXT,
              ENC_VBR_LONGTEXT, VLC_FALSE );
vlc_module_end();

static const char *ppsz_enc_options[] = {
    "quality", "mode", "vbr", NULL
};

/*****************************************************************************
 * encoder_sys_t : toolame encoder descriptor
 *****************************************************************************/
struct encoder_sys_t
{
    /*
     * Input properties
     */
    int16_t p_left[MPEG_FRAME_SIZE];
    int16_t p_right[MPEG_FRAME_SIZE];
    int i_nb_samples;
    audio_date_t pts;

    /*
     * libtoolame properties
     */
    toolame_options *p_toolame;
    unsigned char p_out_buffer[MAX_CODED_FRAME_SIZE];
};

/*****************************************************************************
 * OpenEncoder: probe the encoder and return score
 *****************************************************************************/
static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;
    vlc_value_t val;

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

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_sys = (encoder_sys_t *)malloc(sizeof(encoder_sys_t)) ) == NULL )
    {
        msg_Err( p_enc, "out of memory" );
        return VLC_EGENERIC;
    }
    p_enc->p_sys = p_sys;

    p_enc->pf_encode_audio = Encode;
    p_enc->fmt_in.i_codec = AOUT_FMT_S16_NE;
    p_enc->fmt_out.i_codec = VLC_FOURCC('m','p','g','a');

    sout_CfgParse( p_enc, ENC_CFG_PREFIX, ppsz_enc_options, p_enc->p_cfg );

    p_sys->p_toolame = toolame_init();

    /* Set options */
    toolame_setSampleFreq( p_sys->p_toolame, p_enc->fmt_out.audio.i_rate );

    var_Get( p_enc, ENC_CFG_PREFIX "vbr", &val );
    if ( val.b_bool )
    {
        FLOAT i_quality;
        var_Get( p_enc, ENC_CFG_PREFIX "quality", &val );
        i_quality = val.i_int;
        if ( i_quality > 50.0 ) i_quality = 50.0;
        if ( i_quality < 0.0 ) i_quality = 0.0;
        toolame_setVBR( p_sys->p_toolame, 1 );
        toolame_setVBRLevel( p_sys->p_toolame, i_quality );
    }
    else
    {
        toolame_setBitrate( p_sys->p_toolame, p_enc->fmt_out.i_bitrate / 1000 );
    }

    if ( p_enc->fmt_in.audio.i_channels == 1 )
    {
        toolame_setMode( p_sys->p_toolame, MPG_MD_MONO );
    }
    else
    {
        var_Get( p_enc, ENC_CFG_PREFIX "mode", &val );
        switch ( val.i_int )
        {
        case 1:
            toolame_setMode( p_sys->p_toolame, MPG_MD_DUAL_CHANNEL );
            break;
        case 2:
            toolame_setMode( p_sys->p_toolame, MPG_MD_JOINT_STEREO );
            break;
        case 0:
        default:
            toolame_setMode( p_sys->p_toolame, MPG_MD_STEREO );
            break;
        }
    }

    if ( toolame_init_params( p_sys->p_toolame ) )
    {
        msg_Err( p_enc, "toolame initialization failed" );
        return -VLC_EGENERIC;
    }

    p_sys->i_nb_samples = 0;
    aout_DateInit( &p_sys->pts, p_enc->fmt_out.audio.i_rate );

    return VLC_SUCCESS;
}

/****************************************************************************
 * Encode: the whole thing
 ****************************************************************************
 * This function spits out MPEG packets.
 ****************************************************************************/
static void Uninterleave( encoder_t *p_enc, int16_t *p_in, int i_nb_samples )
{
    int16_t *p_left = p_enc->p_sys->p_left + p_enc->p_sys->i_nb_samples;
    int16_t *p_right = p_enc->p_sys->p_right + p_enc->p_sys->i_nb_samples;

    while ( i_nb_samples > 0 )
    {
        *p_left++ = *p_in++;
        *p_right++ = *p_in++;
        i_nb_samples--;
    }
}

static block_t *Encode( encoder_t *p_enc, aout_buffer_t *p_aout_buf )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    int16_t *p_buffer = (int16_t *)p_aout_buf->p_buffer;
    int i_nb_samples = p_aout_buf->i_nb_samples;
    block_t *p_chain = NULL;
    mtime_t i_computed_pts = p_aout_buf->start_date -
                (mtime_t)1000000 * (mtime_t)p_sys->i_nb_samples /
                (mtime_t)p_enc->fmt_in.audio.i_rate;

    if ( aout_DateGet( &p_sys->pts ) - i_computed_pts > 10000 ||
         aout_DateGet( &p_sys->pts ) - i_computed_pts < -10000 )
    {
        msg_Dbg( p_enc, "resetting audio date" );
        aout_DateSet( &p_sys->pts, i_computed_pts );
    }

    while ( p_sys->i_nb_samples + i_nb_samples >= MPEG_FRAME_SIZE )
    {
        int i_used;
        block_t *p_block;

        Uninterleave( p_enc, p_buffer, MPEG_FRAME_SIZE - p_sys->i_nb_samples );
        i_nb_samples -= MPEG_FRAME_SIZE - p_sys->i_nb_samples;
        p_buffer += (MPEG_FRAME_SIZE - p_sys->i_nb_samples) * 2;

        toolame_encode_buffer( p_sys->p_toolame, p_sys->p_left,
                               p_sys->p_right, MPEG_FRAME_SIZE,
                               p_sys->p_out_buffer, MAX_CODED_FRAME_SIZE,
                               &i_used );
        p_sys->i_nb_samples = 0;
        p_block = block_New( p_enc, i_used );
        p_enc->p_vlc->pf_memcpy( p_block->p_buffer, p_sys->p_out_buffer,
                                 i_used );
        p_block->i_length = (mtime_t)1000000 *
                (mtime_t)MPEG_FRAME_SIZE / (mtime_t)p_enc->fmt_in.audio.i_rate;
        p_block->i_dts = p_block->i_pts = aout_DateGet( &p_sys->pts );
        aout_DateIncrement( &p_sys->pts, MPEG_FRAME_SIZE );
        block_ChainAppend( &p_chain, p_block );
    }

    if ( i_nb_samples )
    {
        Uninterleave( p_enc, p_buffer, i_nb_samples );
        p_sys->i_nb_samples += i_nb_samples;
    }

    return p_chain;
}

/*****************************************************************************
 * CloseEncoder: toolame encoder destruction
 *****************************************************************************/
static void CloseEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    toolame_deinit( p_sys->p_toolame );

    free( p_sys );
}
