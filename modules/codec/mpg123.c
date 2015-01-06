/*****************************************************************************
 * mpg123.c: MPEG-1 & 2 audio layer I, II, III + MPEG 2.5 decoder
 *****************************************************************************
 * Copyright (C) 2001-2014 VLC authors and VideoLAN
 *
 * Authors: Ludovic Fauvet <etix@videolan.org>
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
* Preamble
*****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <mpg123.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_block.h>
#include <vlc_codec.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int      OpenDecoder( vlc_object_t * );
static void     CloseDecoder( vlc_object_t * );
static block_t *DecodeBlock( decoder_t *, block_t ** );
static int      InitMPG123( void );
static void     ExitMPG123( void );

static unsigned int mpg123_refcount = 0;
static vlc_mutex_t mpg123_mutex = VLC_STATIC_MUTEX;

/*****************************************************************************
 * Local structures
 *****************************************************************************/
struct decoder_sys_t
{
    mpg123_handle * p_handle;
    date_t          end_date;

    struct mpg123_frameinfo frame_info;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )
    set_description( N_("MPEG audio decoder using mpg123") )
    set_capability( "decoder", 99 )
    set_shortname( "mpg123" )
    set_callbacks( OpenDecoder, CloseDecoder )
vlc_module_end ()

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************/
static block_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    int i_err;
    block_t *p_block = pp_block ? *pp_block : NULL;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( !pp_block || !p_block )
        return NULL;

    if( p_block->i_buffer == 0 )
        return NULL;

    if( !date_Get( &p_sys->end_date ) && p_block->i_pts <= VLC_TS_INVALID )
    {
        /* We've just started the stream, wait for the first PTS. */
        msg_Dbg( p_dec, "waiting for PTS" );
        goto error;
    }

    if( p_block->i_flags & ( BLOCK_FLAG_DISCONTINUITY | BLOCK_FLAG_CORRUPTED ) )
    {
        date_Set( &p_sys->end_date, 0 );
        goto error;
    }

    /* Feed mpg123 with raw data */
    i_err = mpg123_feed( p_sys->p_handle, p_block->p_buffer,
                         p_block->i_buffer );

    if( i_err != MPG123_OK )
    {
        msg_Err( p_dec, "mpg123_feed failed: %s", mpg123_plain_strerror( i_err ) );
        goto error;
    }

    /* Get details about the stream */
    i_err = mpg123_info( p_sys->p_handle, &p_sys->frame_info );

    if( i_err == MPG123_NEED_MORE )
    {
        /* Need moar data */
        goto error;
    }
    else if( i_err != MPG123_OK )
    {
        msg_Err( p_dec, "mpg123_info failed: %s", mpg123_plain_strerror( i_err ) );
        goto error;
    }

    /* Configure the output */
    p_block->i_nb_samples = mpg123_spf( p_sys->p_handle );
    p_dec->fmt_out.i_bitrate = p_sys->frame_info.bitrate * 1000;

    switch( p_sys->frame_info.mode )
    {
        case MPG123_M_STEREO:
        case MPG123_M_JOINT:
            p_dec->fmt_out.audio.i_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
            break;
        case MPG123_M_DUAL:
            p_dec->fmt_out.audio.i_original_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
                                                     | AOUT_CHAN_DUALMONO;
            break;
        case MPG123_M_MONO:
            p_dec->fmt_out.audio.i_original_channels = AOUT_CHAN_CENTER;
            break;
        default:
            msg_Err( p_dec, "Unknown mode");
            goto error;
    }

    p_dec->fmt_out.audio.i_physical_channels =
        p_dec->fmt_out.audio.i_original_channels & AOUT_CHAN_PHYSMASK;

    /* Date management */
    if( p_dec->fmt_out.audio.i_rate != p_sys->frame_info.rate )
    {
        p_dec->fmt_out.audio.i_rate = p_sys->frame_info.rate;
        date_Init( &p_sys->end_date, p_dec->fmt_out.audio.i_rate, 1 );
        date_Set( &p_sys->end_date, 0 );
    }

    if( p_block->i_pts > VLC_TS_INVALID &&
        p_block->i_pts != date_Get( &p_sys->end_date ) )
    {
        date_Set( &p_sys->end_date, p_block->i_pts );
    }

    /* Request a new audio buffer */
    block_t *p_out = decoder_NewAudioBuffer( p_dec, p_block->i_nb_samples );
    if( unlikely( !p_out ) )
        goto error;

    /* Configure the buffer */
    p_out->i_nb_samples = p_block->i_nb_samples;
    p_out->i_dts = p_out->i_pts = date_Get( &p_sys->end_date );
    p_out->i_length = date_Increment( &p_sys->end_date, p_block->i_nb_samples )
        - p_out->i_pts;

    /* Make mpg123 write directly into the VLC output buffer */
    i_err = mpg123_replace_buffer( p_sys->p_handle, p_out->p_buffer, p_out->i_buffer );
    if( i_err != MPG123_OK )
    {
        msg_Err( p_dec, "could not replace buffer: %s", mpg123_plain_strerror( i_err ) );
        block_Release( p_out );
        goto error;
    }

    *pp_block = NULL; /* avoid being fed the same packet again */

    /* Do the actual decoding now */
    i_err = mpg123_decode_frame( p_sys->p_handle, NULL, NULL, NULL );
    if( i_err != MPG123_OK )
    {
        if( i_err != MPG123_NEW_FORMAT )
            msg_Err( p_dec, "mpg123_decode_frame error: %s", mpg123_plain_strerror( i_err ) );
        block_Release( p_out );
        goto error;
    }

    block_Release( p_block );
    return p_out;

error:
    block_Release( p_block );
    return NULL;
}

/*****************************************************************************
 * OpenDecoder :
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_MPGA &&
        p_dec->fmt_in.i_codec != VLC_CODEC_MP3 )
        return VLC_EGENERIC;

    p_dec->fmt_out.i_cat = AUDIO_ES;
    p_dec->fmt_out.i_codec = VLC_CODEC_FL32;

    /* Initialize libmpg123 */
    if( InitMPG123() != MPG123_OK )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the module's structure */
    p_sys = p_dec->p_sys = malloc( sizeof(decoder_sys_t) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    /* Create our mpg123 handle */
    if( ( p_sys->p_handle = mpg123_new( NULL, NULL ) ) == NULL )
        goto error;

    /* Open a new bitstream */
    if( mpg123_open_feed( p_sys->p_handle ) != MPG123_OK )
    {
        msg_Err( p_this, "mpg123 error: can't open feed" );
        goto error;
    }

    /* Disable resync stream after error */
    mpg123_param( p_sys->p_handle, MPG123_ADD_FLAGS, MPG123_NO_RESYNC, 0 );

    /* Setup output format */
    mpg123_format_none( p_sys->p_handle );

    if( MPG123_OK != mpg123_format( p_sys->p_handle,
                                    p_dec->fmt_in.audio.i_rate,
                                    MPG123_MONO | MPG123_STEREO,
                                    MPG123_ENC_FLOAT_32 ) )
    {
        msg_Err( p_this, "mpg123 error: %s",
                mpg123_strerror( p_sys->p_handle ) );
        mpg123_close( p_sys->p_handle );
        goto error;
    }

    p_dec->fmt_out.audio.i_rate = 0; /* So end_date gets initialized */
    p_dec->fmt_out.audio.i_format = p_dec->fmt_out.i_codec;
    p_dec->pf_decode_audio = DecodeBlock;

    msg_Dbg( p_this, "%4.4s->%4.4s, bits per sample: %i",
             (char *)&p_dec->fmt_in.i_codec,
             (char *)&p_dec->fmt_out.i_codec,
             aout_BitsPerSample( p_dec->fmt_out.i_codec ) );

    return VLC_SUCCESS;
error:
    mpg123_delete( p_sys->p_handle );
    ExitMPG123();
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * CloseDecoder : deallocate data structures
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    mpg123_close( p_sys->p_handle );
    mpg123_delete( p_sys->p_handle );
    ExitMPG123();
    free( p_sys );
}

/*****************************************************************************
 * InitMPG123 : initialize the mpg123 library (reentrant)
 *****************************************************************************/
static int InitMPG123( void )
{
    int i_ret;
    vlc_mutex_lock( &mpg123_mutex );
    if( mpg123_refcount > 0 )
    {
        mpg123_refcount++;
        vlc_mutex_unlock( &mpg123_mutex );
        return MPG123_OK;
    }
    if( ( i_ret = mpg123_init() ) == MPG123_OK )
        mpg123_refcount++;
    vlc_mutex_unlock( &mpg123_mutex );
    return i_ret;
}

/*****************************************************************************
 * ExitMPG123 : close down the mpg123 library (reentrant)
 *****************************************************************************/
static void ExitMPG123( void )
{
    vlc_mutex_lock( &mpg123_mutex );
    mpg123_refcount--;
    if( mpg123_refcount == 0 )
        mpg123_exit();
    vlc_mutex_unlock( &mpg123_mutex );
}
