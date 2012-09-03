/*****************************************************************************
 * audio.c: transcoding stream output module (audio)
 *****************************************************************************
 * Copyright (C) 2003-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Jean-Paul Saman <jpsaman #_at_# m2x dot nl>
 *          Antoine Cellerier <dionoea at videolan dot org>
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

#include "transcode.h"

#include <vlc_aout.h>
#include <vlc_meta.h>
#include <vlc_modules.h>

static const int pi_channels_maps[6] =
{
    0,
    AOUT_CHAN_CENTER,   AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT
};

static block_t *audio_new_buffer( decoder_t *p_dec, int i_samples )
{
    block_t *p_block;
    int i_size;

    if( p_dec->fmt_out.audio.i_bitspersample )
    {
        i_size = i_samples * p_dec->fmt_out.audio.i_bitspersample / 8 *
            p_dec->fmt_out.audio.i_channels;
    }
    else if( p_dec->fmt_out.audio.i_bytes_per_frame &&
             p_dec->fmt_out.audio.i_frame_length )
    {
        i_size = i_samples * p_dec->fmt_out.audio.i_bytes_per_frame /
            p_dec->fmt_out.audio.i_frame_length;
    }
    else
    {
        i_size = i_samples * 4 * p_dec->fmt_out.audio.i_channels;
    }

    p_block = block_New( p_dec, i_size );
    p_block->i_nb_samples = i_samples;
    return p_block;
}

static int transcode_audio_filter_allocation_init( filter_t *p_filter,
                                                   void *data )
{
    VLC_UNUSED(p_filter);
    VLC_UNUSED(data);
    return VLC_SUCCESS;
}

static bool transcode_audio_filter_needed( const es_format_t *p_fmt1, const es_format_t *p_fmt2 )
{
    if( p_fmt1->i_codec != p_fmt2->i_codec ||
        p_fmt1->audio.i_channels != p_fmt2->audio.i_channels ||
        p_fmt1->audio.i_rate != p_fmt2->audio.i_rate )
        return true;
    return false;
}
static int transcode_audio_filter_chain_build( sout_stream_t *p_stream, filter_chain_t *p_chain,
                                               const es_format_t *p_dst, const es_format_t *p_src )
{
    if( !transcode_audio_filter_needed( p_dst, p_src ) )
        return VLC_SUCCESS;

    es_format_t current = *p_src;

    msg_Dbg( p_stream, "Looking for filter "
             "(%4.4s->%4.4s, channels %d->%d, rate %d->%d)",
         (const char *)&p_src->i_codec,
         (const char *)&p_dst->i_codec,
         p_src->audio.i_channels,
         p_dst->audio.i_channels,
         p_src->audio.i_rate,
         p_dst->audio.i_rate );

    /* If any filter is needed, convert to fl32 */
    if( current.i_codec != VLC_CODEC_FL32 )
    {
        /* First step, convert to fl32 */
        current.i_codec =
        current.audio.i_format = VLC_CODEC_FL32;
        aout_FormatPrepare( &current.audio );

        if( !filter_chain_AppendFilter( p_chain, NULL, NULL, NULL, &current ) )
        {
            /* If that fails, try going through fi32 */
            current.i_codec =
                current.audio.i_format = VLC_CODEC_FI32;
            aout_FormatPrepare( &current.audio );

            if( !filter_chain_AppendFilter( p_chain, NULL, NULL, NULL, &current ) )
            {
                msg_Err( p_stream, "Failed to find conversion filter to fi32" );
                return VLC_EGENERIC;
            }
            current = *filter_chain_GetFmtOut( p_chain );
            current.i_codec =
                current.audio.i_format = VLC_CODEC_FL32;
            aout_FormatPrepare( &current.audio );
            if( !filter_chain_AppendFilter( p_chain, NULL, NULL, NULL, &current ) )
            {
                msg_Err( p_stream, "Failed to find conversion filter to fl32" );
                return VLC_EGENERIC;
            }
        }
        current = *filter_chain_GetFmtOut( p_chain );
    }

    /* Fix sample rate */
    if( current.audio.i_rate != p_dst->audio.i_rate )
    {
        current.audio.i_rate = p_dst->audio.i_rate;
        aout_FormatPrepare( &current.audio );
        if( !filter_chain_AppendFilter( p_chain, NULL, NULL, NULL, &current ) )
        {
            msg_Err( p_stream, "Failed to find conversion filter for resampling" );
            return VLC_EGENERIC;
        }
        current = *filter_chain_GetFmtOut( p_chain );
    }

    /* Fix channels */
    if( current.audio.i_channels != p_dst->audio.i_channels )
    {
        current.audio.i_channels = p_dst->audio.i_channels;
        current.audio.i_physical_channels = p_dst->audio.i_physical_channels;
        current.audio.i_original_channels = p_dst->audio.i_original_channels;

        if( ( !current.audio.i_physical_channels || !current.audio.i_original_channels ) &&
            current.audio.i_channels < 6 )
            current.audio.i_physical_channels =
            current.audio.i_original_channels = pi_channels_maps[current.audio.i_channels];

        aout_FormatPrepare( &current.audio );
        if( !filter_chain_AppendFilter( p_chain, NULL, NULL, NULL, &current ) )
        {
            msg_Err( p_stream, "Failed to find conversion filter for channel mixing" );
            return VLC_EGENERIC;
        }
        current = *filter_chain_GetFmtOut( p_chain );
    }
    /* And last step, convert to the requested codec */
    if( current.i_codec != p_dst->i_codec )
    {
        current.i_codec = p_dst->i_codec;
        aout_FormatPrepare( &current.audio );
        if( !filter_chain_AppendFilter( p_chain, NULL, NULL, NULL, &current ) )
        {
            msg_Err( p_stream, "Failed to find conversion filter to %4.4s",
                     (const char*)&p_dst->i_codec);
            return VLC_EGENERIC;
        }
        current = *filter_chain_GetFmtOut( p_chain );
    }

    if( transcode_audio_filter_needed( p_dst, &current ) )
    {
        /* Weird case, a filter has side effects, doomed */
        msg_Err( p_stream, "Failed to create a valid audio filter chain" );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_stream, "Got complete audio filter chain" );
    return VLC_SUCCESS;
}


int transcode_audio_new( sout_stream_t *p_stream,
                                sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    es_format_t fmt_last;

    /*
     * Open decoder
     */

    /* Initialization of decoder structures */
    id->p_decoder->fmt_out = id->p_decoder->fmt_in;
    id->p_decoder->fmt_out.i_extra = 0;
    id->p_decoder->fmt_out.p_extra = 0;
    id->p_decoder->pf_decode_audio = NULL;
    id->p_decoder->pf_aout_buffer_new = audio_new_buffer;
    /* id->p_decoder->p_cfg = p_sys->p_audio_cfg; */

    id->p_decoder->p_module =
        module_need( id->p_decoder, "decoder", "$codec", false );
    if( !id->p_decoder->p_module )
    {
        msg_Err( p_stream, "cannot find audio decoder" );
        return VLC_EGENERIC;
    }
    /* decoders don't set audio.i_format, but audio filters use it */
    id->p_decoder->fmt_out.audio.i_format = id->p_decoder->fmt_out.i_codec;
    id->p_decoder->fmt_out.audio.i_bitspersample =
        aout_BitsPerSample( id->p_decoder->fmt_out.i_codec );
    fmt_last = id->p_decoder->fmt_out;
    /* Fix AAC SBR changing number of channels and sampling rate */
    if( !(id->p_decoder->fmt_in.i_codec == VLC_CODEC_MP4A &&
        fmt_last.audio.i_rate != id->p_encoder->fmt_in.audio.i_rate &&
        fmt_last.audio.i_channels != id->p_encoder->fmt_in.audio.i_channels) )
        fmt_last.audio.i_rate = id->p_decoder->fmt_in.audio.i_rate;

    /*
     * Open encoder
     */

    /* Initialization of encoder format structures */
    es_format_Init( &id->p_encoder->fmt_in, id->p_decoder->fmt_in.i_cat,
                    id->p_decoder->fmt_out.i_codec );
    id->p_encoder->fmt_in.audio.i_format = id->p_decoder->fmt_out.i_codec;

    id->p_encoder->fmt_in.audio.i_rate = id->p_encoder->fmt_out.audio.i_rate;
    id->p_encoder->fmt_in.audio.i_physical_channels =
        id->p_encoder->fmt_out.audio.i_physical_channels;
    id->p_encoder->fmt_in.audio.i_original_channels =
        id->p_encoder->fmt_out.audio.i_original_channels;
    id->p_encoder->fmt_in.audio.i_channels =
        id->p_encoder->fmt_out.audio.i_channels;
    id->p_encoder->fmt_in.audio.i_bitspersample =
        aout_BitsPerSample( id->p_encoder->fmt_in.i_codec );

    id->p_encoder->p_cfg = p_stream->p_sys->p_audio_cfg;
    id->p_encoder->p_module =
        module_need( id->p_encoder, "encoder", p_sys->psz_aenc, true );
    if( !id->p_encoder->p_module )
    {
        msg_Err( p_stream, "cannot find audio encoder (module:%s fourcc:%4.4s). Take a look few lines earlier to see possible reason.",
                 p_sys->psz_aenc ? p_sys->psz_aenc : "any",
                 (char *)&p_sys->i_acodec );
        module_unneed( id->p_decoder, id->p_decoder->p_module );
        id->p_decoder->p_module = NULL;
        return VLC_EGENERIC;
    }
    id->p_encoder->fmt_in.audio.i_format = id->p_encoder->fmt_in.i_codec;
    id->p_encoder->fmt_in.audio.i_bitspersample =
        aout_BitsPerSample( id->p_encoder->fmt_in.i_codec );

    /* Load user specified audio filters */
    if( p_sys->psz_af )
    {
        es_format_t fmt_fl32 = fmt_last;
        fmt_fl32.i_codec =
        fmt_fl32.audio.i_format = VLC_CODEC_FL32;
        id->p_uf_chain = filter_chain_New( p_stream, "audio filter", false,
                                           transcode_audio_filter_allocation_init, NULL, NULL );
        filter_chain_Reset( id->p_uf_chain, &fmt_last, &fmt_fl32 );

        if( transcode_audio_filter_chain_build( p_stream, id->p_uf_chain,
                                                &fmt_fl32, &fmt_last ) )
        {
            transcode_audio_close( id );
            return VLC_EGENERIC;
        }
        fmt_last = fmt_fl32;

        if( filter_chain_AppendFromString( id->p_uf_chain, p_sys->psz_af ) > 0 )
            fmt_last = *filter_chain_GetFmtOut( id->p_uf_chain );
    }

    /* Load conversion filters */
    id->p_f_chain = filter_chain_New( p_stream, "audio filter", true,
                    transcode_audio_filter_allocation_init, NULL, NULL );
    filter_chain_Reset( id->p_f_chain, &fmt_last, &id->p_encoder->fmt_in );

    if( transcode_audio_filter_chain_build( p_stream, id->p_f_chain,
                                            &id->p_encoder->fmt_in, &fmt_last ) )
    {
        transcode_audio_close( id );
        return VLC_EGENERIC;
    }
    fmt_last = id->p_encoder->fmt_in;

    /* */
    id->p_encoder->fmt_out.i_codec =
        vlc_fourcc_GetCodec( AUDIO_ES, id->p_encoder->fmt_out.i_codec );

    return VLC_SUCCESS;
}

void transcode_audio_close( sout_stream_id_t *id )
{
    /* Close decoder */
    if( id->p_decoder->p_module )
        module_unneed( id->p_decoder, id->p_decoder->p_module );
    id->p_decoder->p_module = NULL;

    if( id->p_decoder->p_description )
        vlc_meta_Delete( id->p_decoder->p_description );
    id->p_decoder->p_description = NULL;

    /* Close encoder */
    if( id->p_encoder->p_module )
        module_unneed( id->p_encoder, id->p_encoder->p_module );
    id->p_encoder->p_module = NULL;

    /* Close filters */
    if( id->p_uf_chain )
        filter_chain_Delete( id->p_uf_chain );
    if( id->p_f_chain )
        filter_chain_Delete( id->p_f_chain );
}

int transcode_audio_process( sout_stream_t *p_stream,
                                    sout_stream_id_t *id,
                                    block_t *in, block_t **out )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    block_t *p_block, *p_audio_buf;
    *out = NULL;

    while( (p_audio_buf = id->p_decoder->pf_decode_audio( id->p_decoder,
                                                          &in )) )
    {
        if( p_sys->b_master_sync )
        {
            mtime_t i_pts = date_Get( &id->interpolated_pts ) + 1;
            mtime_t i_drift = p_audio_buf->i_pts - i_pts;
            if (i_drift > MASTER_SYNC_MAX_DRIFT || i_drift < -MASTER_SYNC_MAX_DRIFT)
            {
                msg_Dbg( p_stream,
                    "drift is too high (%"PRId64"), resetting master sync",
                    i_drift );
                date_Set( &id->interpolated_pts, p_audio_buf->i_pts );
                i_pts = p_audio_buf->i_pts + 1;
            }
            p_sys->i_master_drift = p_audio_buf->i_pts - i_pts;
            date_Increment( &id->interpolated_pts, p_audio_buf->i_nb_samples );
            p_audio_buf->i_pts = i_pts;
        }

        p_audio_buf->i_dts = p_audio_buf->i_pts;

        /* Run filter chain */
        if( id->p_uf_chain )
        {
            p_audio_buf = filter_chain_AudioFilter( id->p_uf_chain,
                                                    p_audio_buf );
            if( !p_audio_buf )
                abort();
        }

        p_audio_buf = filter_chain_AudioFilter( id->p_f_chain, p_audio_buf );
        if( !p_audio_buf )
            abort();

        p_audio_buf->i_dts = p_audio_buf->i_pts;

        p_block = id->p_encoder->pf_encode_audio( id->p_encoder, p_audio_buf );

        block_ChainAppend( out, p_block );
        block_Release( p_audio_buf );
    }

    return VLC_SUCCESS;
}

bool transcode_audio_add( sout_stream_t *p_stream, es_format_t *p_fmt, 
            sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    msg_Dbg( p_stream,
             "creating audio transcoding from fcc=`%4.4s' to fcc=`%4.4s'",
             (char*)&p_fmt->i_codec, (char*)&p_sys->i_acodec );

    /* Complete destination format */
    id->p_encoder->fmt_out.i_codec = p_sys->i_acodec;
    id->p_encoder->fmt_out.audio.i_rate = p_sys->i_sample_rate > 0 ?
        p_sys->i_sample_rate : p_fmt->audio.i_rate;
    id->p_encoder->fmt_out.i_bitrate = p_sys->i_abitrate;
    id->p_encoder->fmt_out.audio.i_bitspersample =
        p_fmt->audio.i_bitspersample;
    id->p_encoder->fmt_out.audio.i_channels = p_sys->i_channels > 0 ?
        p_sys->i_channels : p_fmt->audio.i_channels;
    /* Sanity check for audio channels */
    id->p_encoder->fmt_out.audio.i_channels =
        __MIN( id->p_encoder->fmt_out.audio.i_channels,
               id->p_decoder->fmt_in.audio.i_channels );
    id->p_encoder->fmt_out.audio.i_original_channels =
        id->p_decoder->fmt_in.audio.i_physical_channels;
    if( id->p_decoder->fmt_in.audio.i_channels ==
        id->p_encoder->fmt_out.audio.i_channels )
    {
        id->p_encoder->fmt_out.audio.i_physical_channels =
            id->p_decoder->fmt_in.audio.i_physical_channels;
    }
    else
    {
        id->p_encoder->fmt_out.audio.i_physical_channels =
            pi_channels_maps[id->p_encoder->fmt_out.audio.i_channels];
    }

    /* Build decoder -> filter -> encoder chain */
    if( transcode_audio_new( p_stream, id ) )
    {
        msg_Err( p_stream, "cannot create audio chain" );
        return false;
    }

    /* Open output stream */
    id->id = sout_StreamIdAdd( p_stream->p_next, &id->p_encoder->fmt_out );
    id->b_transcode = true;

    if( !id->id )
    {
        transcode_audio_close( id );
        return false;
    }

    date_Init( &id->interpolated_pts, p_fmt->audio.i_rate, 1 );

    return true;
}
