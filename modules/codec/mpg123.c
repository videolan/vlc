/*****************************************************************************
 * mpg123.c: MPEG-1 & 2 audio layer I, II, III + MPEG 2.5 decoder
 *****************************************************************************
 * Copyright (C) 2001-2016 VLC authors and VideoLAN
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

#include <assert.h>

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

static unsigned int mpg123_refcount = 0;
static vlc_mutex_t mpg123_mutex = VLC_STATIC_MUTEX;

/*****************************************************************************
 * Local structures
 *****************************************************************************/
typedef struct
{
    mpg123_handle * p_handle;
    date_t          end_date;
    block_t       * p_out;
    bool            b_opened;
} decoder_sys_t;

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )
    set_description( N_("MPEG audio decoder using mpg123") )
    set_capability( "audio decoder", 100 )
    set_shortname( "mpg123" )
    set_callbacks( OpenDecoder, CloseDecoder )
vlc_module_end ()

/*****************************************************************************
 * MPG123Open
 *****************************************************************************/
static int MPG123Open( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Create our mpg123 handle */
    if( ( p_sys->p_handle = mpg123_new( NULL, NULL ) ) == NULL )
    {
        msg_Err( p_dec, "mpg123 error: can't create handle" );
        return VLC_EGENERIC;
    }

    /* Open a new bitstream */
    if( mpg123_open_feed( p_sys->p_handle ) != MPG123_OK )
    {
        msg_Err( p_dec, "mpg123 error: can't open feed" );
        mpg123_delete( p_sys->p_handle );
        return VLC_EGENERIC;
    }

    /* Disable resync stream after error */
    mpg123_param( p_sys->p_handle, MPG123_ADD_FLAGS, MPG123_NO_RESYNC, 0 );

    /* Setup output format */
    mpg123_format_none( p_sys->p_handle );

    int i_ret = MPG123_OK;
    if( p_dec->fmt_in.audio.i_rate != 0 )
    {
        i_ret =  mpg123_format( p_sys->p_handle, p_dec->fmt_in.audio.i_rate,
                                MPG123_MONO | MPG123_STEREO,
                                MPG123_ENC_FLOAT_32 );
    }
    else
    {
        /* The rate from the input is unknown. Tell mpg123 to accept all rates
         * to avoid conversion on their side */
        static const long mp3_rates[] = {
            8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000,
        };
        for( size_t i = 0;
            i < sizeof(mp3_rates) / sizeof(*mp3_rates) && i_ret == MPG123_OK;
            ++i )
        {
            i_ret =  mpg123_format( p_sys->p_handle, mp3_rates[i],
                                    MPG123_MONO | MPG123_STEREO,
                                    MPG123_ENC_FLOAT_32 );
        }
    }
    if( i_ret != MPG123_OK )
    {
        msg_Err( p_dec, "mpg123 error: %s",
                 mpg123_strerror( p_sys->p_handle ) );
        mpg123_close( p_sys->p_handle );
        mpg123_delete( p_sys->p_handle );
        return VLC_EGENERIC;
    }

    p_sys->b_opened = true;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Flush:
 *****************************************************************************/
static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    date_Set( &p_sys->end_date, VLC_TICK_INVALID );

    mpg123_close( p_sys->p_handle );
    mpg123_delete( p_sys->p_handle );
    p_sys->b_opened = false;
    MPG123Open( p_dec );
}

static int UpdateAudioFormat( decoder_t *p_dec )
{
    int i_err;
    decoder_sys_t *p_sys = p_dec->p_sys;
    struct mpg123_frameinfo frame_info;

    /* Get details about the stream */
    i_err = mpg123_info( p_sys->p_handle, &frame_info );
    if( i_err != MPG123_OK )
    {
        msg_Err( p_dec, "mpg123_info failed: %s",
                 mpg123_plain_strerror( i_err ) );
        return VLC_EGENERIC;
    }

    p_dec->fmt_out.i_bitrate = frame_info.bitrate * 1000;

    switch( frame_info.mode )
    {
        case MPG123_M_DUAL:
            p_dec->fmt_out.audio.i_chan_mode = AOUT_CHANMODE_DUALMONO;
            /* fall through */
        case MPG123_M_STEREO:
        case MPG123_M_JOINT:
            p_dec->fmt_out.audio.i_physical_channels =
                AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
            break;
        case MPG123_M_MONO:
            p_dec->fmt_out.audio.i_physical_channels = AOUT_CHAN_CENTER;
            break;
        default:
            return VLC_EGENERIC;
    }

    aout_FormatPrepare( &p_dec->fmt_out.audio );

    /* Date management */
    if( p_dec->fmt_out.audio.i_rate != (unsigned int)frame_info.rate )
    {
        p_dec->fmt_out.audio.i_rate = (unsigned int)frame_info.rate;
        date_Init( &p_sys->end_date, p_dec->fmt_out.audio.i_rate, 1 );
    }

    return decoder_UpdateAudioFormat( p_dec );
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************/
static int DecodeBlock( decoder_t *p_dec, block_t *p_block )
{
    int i_err;
    decoder_sys_t *p_sys = p_dec->p_sys;
    vlc_tick_t i_pts = VLC_TICK_INVALID;

    if( !p_sys->b_opened )
    {
        if( p_block )
            block_Release( p_block );
        return VLCDEC_ECRITICAL;
    }

    /* Feed input block */
    if( p_block != NULL )
    {
        i_pts = p_block->i_pts != VLC_TICK_INVALID ? p_block->i_pts : p_block->i_dts;

        if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
        {
            Flush( p_dec );
            if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
            {
                block_Release( p_block );
                return VLCDEC_SUCCESS;
            }
        }

        if( i_pts == VLC_TICK_INVALID &&
            date_Get( &p_sys->end_date ) == VLC_TICK_INVALID )
        {
            /* We've just started the stream, wait for the first PTS. */
            msg_Dbg( p_dec, "waiting for PTS" );
            block_Release( p_block );
            return VLCDEC_SUCCESS;
        }

        /* Feed mpg123 with raw data */
        i_err = mpg123_feed( p_sys->p_handle, p_block->p_buffer, p_block->i_buffer );
        block_Release( p_block );

        if( i_err != MPG123_OK )
        {
            msg_Err( p_dec, "mpg123_feed failed: %s",
                     mpg123_plain_strerror( i_err ) );
            return VLCDEC_SUCCESS;
        }
    }

    while( true )
    {
        /* Fetch a new output block (if possible) */
        if( !p_sys->p_out
            || p_sys->p_out->i_buffer != mpg123_outblock( p_sys->p_handle ) )
        {
            if( p_sys->p_out )
                block_Release( p_sys->p_out );

            /* Keep the output buffer for next calls in case it's not used (in case
             * of MPG123_NEED_MORE status) */
            p_sys->p_out = block_Alloc( mpg123_outblock( p_sys->p_handle ) );

            if( unlikely( !p_sys->p_out ) )
                return VLCDEC_SUCCESS;
        }

        /* Do the actual decoding now */
        size_t i_bytes = 0;

        /* Make mpg123 write directly into the VLC output buffer */
        i_err = mpg123_replace_buffer( p_sys->p_handle, p_sys->p_out->p_buffer,
                                       p_sys->p_out->i_buffer );
        if( i_err != MPG123_OK )
        {
            msg_Err( p_dec, "could not replace buffer: %s",
                     mpg123_plain_strerror( i_err ) );
            block_Release( p_sys->p_out );
            p_sys->p_out = NULL;
            break;
        }

        i_err = mpg123_decode_frame( p_sys->p_handle, NULL, NULL, &i_bytes );
        if( i_err != MPG123_OK && i_err != MPG123_NEED_MORE )
        {
            if( i_err == MPG123_NEW_FORMAT )
            {
                p_dec->fmt_out.audio.i_rate = 0;
            }
            else
            {
                msg_Err( p_dec, "mpg123_decode_frame error: %s",
                         mpg123_plain_strerror( i_err ) );
                date_Set( &p_sys->end_date, VLC_TICK_INVALID );
                break;
            }
        }

        if( i_bytes == 0 )
            break;

        if( p_dec->fmt_out.audio.i_rate == 0 )
        {
            if( UpdateAudioFormat( p_dec ) != VLC_SUCCESS )
            {
                date_Set( &p_sys->end_date, VLC_TICK_INVALID );
                break;
            }
        }

        block_t *p_out = p_sys->p_out;
        p_sys->p_out = NULL;

        if( date_Get( &p_sys->end_date ) == VLC_TICK_INVALID )
        {
            if( i_pts != VLC_TICK_INVALID )
            {
                date_Set( &p_sys->end_date, i_pts );
            }
            else if( p_out ) /* we need a valid date and that's not guaranteed on flush/error */
            {
                block_Release( p_out );
                break;
            }
        }

        if( p_out )
        {
            assert( p_dec->fmt_out.audio.i_rate != 0 );
            assert( p_out->i_buffer >= i_bytes );
            p_out->i_buffer = i_bytes;
            p_out->i_nb_samples = p_out->i_buffer * p_dec->fmt_out.audio.i_frame_length
                                / p_dec->fmt_out.audio.i_bytes_per_frame;

            /* Configure the buffer */
            p_out->i_dts = p_out->i_pts = date_Get( &p_sys->end_date );
            p_out->i_length = date_Increment( &p_sys->end_date, p_out->i_nb_samples )
                            - p_out->i_pts;
            decoder_QueueAudio( p_dec, p_out );
        }
    }

    return VLCDEC_SUCCESS;
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

    /* Initialize libmpg123 */
    if( InitMPG123() != MPG123_OK )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the module's structure */
    p_sys = p_dec->p_sys = malloc( sizeof(decoder_sys_t) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_sys->p_out = NULL;
    date_Set( &p_sys->end_date, VLC_TICK_INVALID );

    if( MPG123Open( p_dec ) )
        goto error;

    p_dec->fmt_out.i_codec = VLC_CODEC_FL32;
    p_dec->fmt_out.audio.i_rate = 0; /* So end_date gets initialized */
    p_dec->fmt_out.audio.i_format = p_dec->fmt_out.i_codec;
    p_dec->pf_decode = DecodeBlock;
    p_dec->pf_flush  = Flush;

    msg_Dbg( p_this, "%4.4s->%4.4s, bits per sample: %i",
             (char *)&p_dec->fmt_in.i_codec,
             (char *)&p_dec->fmt_out.i_codec,
             aout_BitsPerSample( p_dec->fmt_out.i_codec ) );

    return VLC_SUCCESS;
error:
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
    if( p_sys->p_out )
        block_Release( p_sys->p_out );
    free( p_sys );
}
