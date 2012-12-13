/*****************************************************************************
 * shine_mod.c: MP3 encoder using Shine, a fixed point implementation
 *****************************************************************************
 * Copyright (C) 2008-2009 M2X
 *
 * Authors: Rafaël Carré <rcarre@m2x.nl>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_block.h>
#include <vlc_block_helper.h>
#include <vlc_bits.h>

#include <assert.h>
#include <inttypes.h>

/* shine.c uses a lot of static variables, so we include the C file to keep
 * the scope.
 * Note that it makes this decoder non reentrant, this is why we have the
 * struct entrant below */
#include "shine.c"

struct encoder_sys_t
{
    block_fifo_t *p_fifo;

    unsigned int i_buffer;
    uint8_t *p_buffer;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenEncoder   ( vlc_object_t * );
static void CloseEncoder  ( vlc_object_t * );

static block_t *EncodeFrame  ( encoder_t *, block_t * );

vlc_module_begin();
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACODEC );
    set_description( _("MP3 fixed point audio encoder") );
    set_capability( "encoder", 50 );
    set_callbacks( OpenEncoder, CloseEncoder );
vlc_module_end();

static struct
{
    bool busy;
    vlc_mutex_t lock;
} entrant = { false, VLC_STATIC_MUTEX, };

static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t*)p_this;
    encoder_sys_t *p_sys;

    /* shine is an 'MP3' encoder */
    if( (p_enc->fmt_out.i_codec != VLC_CODEC_MP3 && p_enc->fmt_out.i_codec != VLC_CODEC_MPGA) ||
        p_enc->fmt_out.audio.i_channels > 2 )
        return VLC_EGENERIC;

    /* Shine is strict on its input */
    if( p_enc->fmt_in.audio.i_channels != 2 )
    {
        msg_Err( p_enc, "Only stereo input is accepted, rejecting %d channels",
            p_enc->fmt_in.audio.i_channels );
        return VLC_EGENERIC;
    }

    if( p_enc->fmt_out.i_bitrate <= 0 )
    {
        msg_Err( p_enc, "unknown bitrate" );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_enc, "bitrate %d, samplerate %d, channels %d",
             p_enc->fmt_out.i_bitrate, p_enc->fmt_out.audio.i_rate,
             p_enc->fmt_out.audio.i_channels );

    vlc_mutex_lock( &entrant.lock );
    if( entrant.busy )
    {
        msg_Err( p_enc, "encoder already in progress" );
        vlc_mutex_unlock( &entrant.lock );
        return VLC_EGENERIC;
    }
    entrant.busy = true;
    vlc_mutex_unlock( &entrant.lock );

    p_enc->p_sys = p_sys = calloc( 1, sizeof( *p_sys ) );
    if( !p_sys )
        goto enomem;

    if( !( p_sys->p_fifo = block_FifoNew() ) )
    {
        free( p_sys );
        goto enomem;
    }

    init_mp3_encoder_engine( p_enc->fmt_out.audio.i_rate,
        p_enc->fmt_out.audio.i_channels, p_enc->fmt_out.i_bitrate / 1000 );

    p_enc->pf_encode_audio = EncodeFrame;
    p_enc->fmt_out.i_cat = AUDIO_ES;

    return VLC_SUCCESS;

enomem:
    vlc_mutex_lock( &entrant.lock );
    entrant.busy = false;
    vlc_mutex_unlock( &entrant.lock );
    return VLC_ENOMEM;
}

/* We split/pack PCM blocks to a fixed size: pcm_chunk_size bytes */
static block_t *GetPCM( encoder_t *p_enc, block_t *p_block )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    block_t *p_pcm_block;

    if( !p_block ) goto buffered; /* just return a block if we can */

    /* Put the PCM samples sent by VLC in the Fifo */
    while( p_sys->i_buffer + p_block->i_buffer >= pcm_chunk_size )
    {
        unsigned int i_buffer = 0;
        p_pcm_block = block_Alloc( pcm_chunk_size );
        if( !p_pcm_block )
            break;

        if( p_sys->i_buffer )
        {
            memcpy( p_pcm_block->p_buffer, p_sys->p_buffer, p_sys->i_buffer );

            i_buffer = p_sys->i_buffer;
            p_sys->i_buffer = 0;
            free( p_sys->p_buffer );
        }

        memcpy( p_pcm_block->p_buffer + i_buffer,
                    p_block->p_buffer, pcm_chunk_size - i_buffer );
        p_block->p_buffer += pcm_chunk_size - i_buffer;

        p_block->i_buffer -= pcm_chunk_size - i_buffer;

        block_FifoPut( p_sys->p_fifo, p_pcm_block );
    }

    /* We hadn't enough data to make a block, put it in standby */
    if( p_block->i_buffer )
    {
        uint8_t *p_tmp;

        if( p_sys->i_buffer > 0 )
            p_tmp = realloc( p_sys->p_buffer, p_block->i_buffer + p_sys->i_buffer );
        else
            p_tmp = malloc( p_block->i_buffer );

        if( !p_tmp )
        {
            p_sys->i_buffer = 0;
            free( p_sys->p_buffer );
            p_sys->p_buffer = NULL;
            return NULL;
        }
        p_sys->p_buffer = p_tmp;
        memcpy( p_sys->p_buffer + p_sys->i_buffer,
                    p_block->p_buffer, p_block->i_buffer );

        p_sys->i_buffer += p_block->i_buffer;
        p_block->i_buffer = 0;
    }

buffered:
    /* and finally get a block back */
    return block_FifoCount( p_sys->p_fifo ) > 0 ? block_FifoGet( p_sys->p_fifo ) : NULL;
}

static block_t *EncodeFrame( encoder_t *p_enc, block_t *p_block )
{
    block_t *p_pcm_block;
    block_t *p_chain = NULL;
    unsigned int i_samples = p_block->i_buffer >> 2 /* s16l stereo */;
    mtime_t start_date = p_block->i_pts;
    start_date -= (mtime_t)i_samples * (mtime_t)1000000 / (mtime_t)p_enc->fmt_out.audio.i_rate;

    VLC_UNUSED(p_enc);

    do {
        p_pcm_block = GetPCM( p_enc, p_block );
        if( !p_pcm_block )
            break;

        p_block = NULL; /* we don't need it anymore */

        uint32_t enc_buffer[16384]; /* storage for 65536 Bytes XXX: too much */
        struct enc_chunk_hdr *chunk = (void*) enc_buffer;
        chunk->enc_data = ENC_CHUNK_SKIP_HDR(chunk->enc_data, chunk);

        encode_frame( (char*)p_pcm_block->p_buffer, chunk );
        block_Release( p_pcm_block );

        block_t *p_mp3_block = block_Alloc( chunk->enc_size );
        if( !p_mp3_block )
            break;

        memcpy( p_mp3_block->p_buffer, chunk->enc_data, chunk->enc_size );

        /* date management */
        p_mp3_block->i_length = SAMP_PER_FRAME1 * 1000000 /
            p_enc->fmt_out.audio.i_rate;

        start_date += p_mp3_block->i_length;
        p_mp3_block->i_dts = p_mp3_block->i_pts = start_date;

        p_mp3_block->i_nb_samples = SAMP_PER_FRAME1;

        block_ChainAppend( &p_chain, p_mp3_block );

    } while( p_pcm_block );

    return p_chain;
}

static void CloseEncoder( vlc_object_t *p_this )
{
    encoder_sys_t *p_sys = ((encoder_t*)p_this)->p_sys;

    vlc_mutex_lock( &entrant.lock );
    entrant.busy = false;
    vlc_mutex_unlock( &entrant.lock );

    /* TODO: we should send the last PCM block padded with 0
     * But we don't know if other blocks will come before it's too late */
    if( p_sys->i_buffer )
        free( p_sys->p_buffer );

    block_FifoRelease( p_sys->p_fifo );
    free( p_sys );
}
