/*****************************************************************************
 * araw.c: Pseudo audio decoder; for raw pcm data
 *****************************************************************************
 * Copyright (C) 2001, 2003 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_aout.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  DecoderOpen ( vlc_object_t * );
static void DecoderClose( vlc_object_t * );

#ifdef ENABLE_SOUT
static int  EncoderOpen ( vlc_object_t * );
#endif

vlc_module_begin ()
    /* audio decoder module */
    set_description( N_("Raw/Log Audio decoder") )
    set_capability( "decoder", 100 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )
    set_callbacks( DecoderOpen, DecoderClose )

#ifdef ENABLE_SOUT
    /* audio encoder submodule */
    add_submodule ()
    set_description( N_("Raw audio encoder") )
    set_capability( "encoder", 150 )
    set_callbacks( EncoderOpen, NULL )
#endif
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t *DecodeBlock( decoder_t *, block_t ** );

struct decoder_sys_t
{
    void (*decode) (void *, const uint8_t *, unsigned);
    size_t framebits;
    date_t end_date;
};

static const uint16_t pi_channels_maps[] =
{
    0,
    AOUT_CHAN_CENTER, AOUT_CHANS_2_0, AOUT_CHANS_3_0,
    AOUT_CHANS_4_0,   AOUT_CHANS_5_0, AOUT_CHANS_5_1,
    AOUT_CHANS_7_0,   AOUT_CHANS_7_1, AOUT_CHANS_8_1,
};

static void S8Decode( void *, const uint8_t *, unsigned );
static void U16BDecode( void *, const uint8_t *, unsigned );
static void U16LDecode( void *, const uint8_t *, unsigned );
static void S16IDecode( void *, const uint8_t *, unsigned );
static void S20BDecode( void *, const uint8_t *, unsigned );
static void U24BDecode( void *, const uint8_t *, unsigned );
static void U24LDecode( void *, const uint8_t *, unsigned );
static void S24BDecode( void *, const uint8_t *, unsigned );
static void S24LDecode( void *, const uint8_t *, unsigned );
static void S24B32Decode( void *, const uint8_t *, unsigned );
static void S24L32Decode( void *, const uint8_t *, unsigned );
static void U32BDecode( void *, const uint8_t *, unsigned );
static void U32LDecode( void *, const uint8_t *, unsigned );
static void S32IDecode( void *, const uint8_t *, unsigned );
static void F32IDecode( void *, const uint8_t *, unsigned );
static void F64IDecode( void *, const uint8_t *, unsigned );
static void DAT12Decode( void *, const uint8_t *, unsigned );

/*****************************************************************************
 * DecoderOpen: probe the decoder and return score
 *****************************************************************************/
static int DecoderOpen( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    vlc_fourcc_t format = p_dec->fmt_in.i_codec;

    switch( p_dec->fmt_in.i_codec )
    {
    case VLC_FOURCC('a','r','a','w'):
    case VLC_FOURCC('a','f','l','t'):
    /* _signed_ big endian samples (mov) */
    case VLC_FOURCC('t','w','o','s'):
    /* _signed_ little endian samples (mov) */
    case VLC_FOURCC('s','o','w','t'):
        format =
            vlc_fourcc_GetCodecAudio( p_dec->fmt_in.i_codec,
                                      p_dec->fmt_in.audio.i_bitspersample );
        if( !format )
        {
            msg_Err( p_dec, "bad parameters(bits/sample)" );
            return VLC_EGENERIC;
        }
        break;
    }

    void (*decode) (void *, const uint8_t *, unsigned) = NULL;
    uint_fast8_t bits;

    switch( format )
    {
#ifdef WORDS_BIGENDIAN
    case VLC_CODEC_F64L:
#else
    case VLC_CODEC_F64B:
#endif
        format = VLC_CODEC_FL64;
        decode = F64IDecode;
    case VLC_CODEC_FL64:
        bits = 64;
        break;
#ifdef WORDS_BIGENDIAN
    case VLC_CODEC_F32L:
#else
    case VLC_CODEC_F32B:
#endif
        format = VLC_CODEC_FL32;
        decode = F32IDecode;
    case VLC_CODEC_FL32:
        bits = 32;
        break;
    case VLC_CODEC_U32B:
        format = VLC_CODEC_S32N;
        decode = U32BDecode;
        bits = 32;
        break;
    case VLC_CODEC_U32L:
        format = VLC_CODEC_S32N;
        decode = U32LDecode;
        bits = 32;
        break;
    case VLC_CODEC_S32I:
        format = VLC_CODEC_S32N;
        decode = S32IDecode;
    case VLC_CODEC_S32N:
        bits = 32;
        break;
    case VLC_CODEC_S24B32:
        format = VLC_CODEC_S32N;
        decode = S24B32Decode;
        bits = 32;
        break;
    case VLC_CODEC_S24L32:
        format = VLC_CODEC_S32N;
        decode = S24L32Decode;
        bits = 32;
        break;
    case VLC_CODEC_U24B:
        format = VLC_CODEC_S32N;
        decode = U24BDecode;
        bits = 24;
        break;
    case VLC_CODEC_U24L:
        format = VLC_CODEC_S32N;
        decode = U24LDecode;
        bits = 24;
        break;
    case VLC_CODEC_S24B:
        format = VLC_CODEC_S32N;
        decode = S24BDecode;
        bits = 24;
        break;
    case VLC_CODEC_S24L:
        format = VLC_CODEC_S32N;
        decode = S24LDecode;
        bits = 24;
        break;
    case VLC_CODEC_S20B:
        format = VLC_CODEC_S32N;
        decode = S20BDecode;
        bits = 20;
        break;
    case VLC_CODEC_U16B:
        format = VLC_CODEC_S16N;
        decode = U16BDecode;
        bits = 16;
        break;
    case VLC_CODEC_U16L:
        format = VLC_CODEC_S16N;
        decode = U16LDecode;
        bits = 16;
        break;
    case VLC_CODEC_S16I:
        format = VLC_CODEC_S16N;
        decode = S16IDecode;
    case VLC_CODEC_S16N:
        bits = 16;
        break;
    case VLC_CODEC_DAT12:
        format = VLC_CODEC_S16N;
        decode = DAT12Decode;
        bits = 12;
        break;
    case VLC_CODEC_S8:
        decode = S8Decode;
        format = VLC_CODEC_U8;
    case VLC_CODEC_U8:
        bits = 8;
        break;
    default:
        return VLC_EGENERIC;
    }

    if( p_dec->fmt_in.audio.i_channels <= 0 ||
        p_dec->fmt_in.audio.i_channels > AOUT_CHAN_MAX )
    {
        msg_Err( p_dec, "bad channels count (1-9): %i",
                 p_dec->fmt_in.audio.i_channels );
        return VLC_EGENERIC;
    }

    if( p_dec->fmt_in.audio.i_rate <= 0 )
    {
        msg_Err( p_dec, "bad samplerate: %d Hz", p_dec->fmt_in.audio.i_rate );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_dec, "samplerate:%dHz channels:%d bits/sample:%d",
             p_dec->fmt_in.audio.i_rate, p_dec->fmt_in.audio.i_channels,
             p_dec->fmt_in.audio.i_bitspersample );

    /* Allocate the memory needed to store the decoder's structure */
    decoder_sys_t *p_sys = malloc(sizeof(*p_sys));
    if( unlikely(p_sys == NULL) )
        return VLC_ENOMEM;

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;
    p_dec->fmt_out.i_codec = format;
    p_dec->fmt_out.audio.i_format = format;
    p_dec->fmt_out.audio.i_rate = p_dec->fmt_in.audio.i_rate;
    if( p_dec->fmt_in.audio.i_physical_channels )
        p_dec->fmt_out.audio.i_physical_channels =
                                       p_dec->fmt_in.audio.i_physical_channels;
    else
        p_dec->fmt_out.audio.i_physical_channels =
                              pi_channels_maps[p_dec->fmt_in.audio.i_channels];
    if( p_dec->fmt_in.audio.i_original_channels )
        p_dec->fmt_out.audio.i_original_channels =
                                       p_dec->fmt_in.audio.i_original_channels;
    else
        p_dec->fmt_out.audio.i_original_channels =
                                      p_dec->fmt_out.audio.i_physical_channels;
    aout_FormatPrepare( &p_dec->fmt_out.audio );

    p_sys->decode = decode;
    p_sys->framebits = bits * p_dec->fmt_out.audio.i_channels;
    assert( p_sys->framebits );

    date_Init( &p_sys->end_date, p_dec->fmt_out.audio.i_rate, 1 );
    date_Set( &p_sys->end_date, 0 );

    p_dec->pf_decode_audio = DecodeBlock;
    p_dec->p_sys = p_sys;

    return VLC_SUCCESS;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with whole samples (see nBlockAlign).
 ****************************************************************************/
static block_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( !pp_block || !*pp_block ) return NULL;

    block_t *p_block = *pp_block;

    if( p_block->i_pts > VLC_TS_INVALID &&
        p_block->i_pts != date_Get( &p_sys->end_date ) )
    {
        date_Set( &p_sys->end_date, p_block->i_pts );
    }
    else if( !date_Get( &p_sys->end_date ) )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( p_block );
        return NULL;
    }

    /* Don't re-use the same pts twice */
    p_block->i_pts = VLC_TS_INVALID;

    unsigned samples = (8 * p_block->i_buffer) / p_sys->framebits;
    if( samples == 0 )
    {
        block_Release( p_block );
        return NULL;
    }

    /* Create chunks of max 1024 samples */
    if( samples > 1024 ) samples = 1024;

    block_t *p_out = decoder_NewAudioBuffer( p_dec, samples );
    if( p_out == NULL )
    {
        block_Release( p_block );
        return NULL;
    }

    p_out->i_pts = date_Get( &p_sys->end_date );
    p_out->i_length = date_Increment( &p_sys->end_date, samples )
                      - p_out->i_pts;

    if( p_sys->decode != NULL )
        p_sys->decode( p_out->p_buffer, p_block->p_buffer,
                       samples * p_dec->fmt_in.audio.i_channels );
    else
        memcpy( p_out->p_buffer, p_block->p_buffer, p_out->i_buffer );

    samples = (samples * p_sys->framebits) / 8;
    p_block->p_buffer += samples;
    p_block->i_buffer -= samples;

    return p_out;
}

static void S8Decode( void *outp, const uint8_t *in, unsigned samples )
{
    uint8_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
        out[i] = in[i] ^ 0x80;
}

static void U16BDecode( void *outp, const uint8_t *in, unsigned samples )
{
    uint16_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
    {
        *(out++) = GetWBE( in ) - 0x8000;
        in += 2;
    }
}

static void U16LDecode( void *outp, const uint8_t *in, unsigned samples )
{
    uint16_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
    {
        *(out++) = GetWLE( in ) - 0x8000;
        in += 2;
    }
}

static void S16IDecode( void *out, const uint8_t *in, unsigned samples )
{
    swab( in, out, samples * 2 );
}

static void S20BDecode( void *outp, const uint8_t *in, unsigned samples )
{
    int32_t *out = outp;

    while( samples >= 2 )
    {
        uint32_t dw = U32_AT(in);
        in += 4;
        *(out++) = dw & ~0xFFF;
        *(out++) = (dw << 20) | (*in << 12);
        in++;
        samples -= 2;
    }

    /* No U32_AT() for the last odd sample: avoid off-by-one overflow! */
    if( samples )
        *(out++) = (U16_AT(in) << 16) | ((in[2] & 0xF0) << 8);
}

static void U24BDecode( void *outp, const uint8_t *in, unsigned samples )
{
    uint32_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
    {
        uint32_t s = ((in[0] << 24) | (in[1] << 16) | (in[2] << 8)) - 0x80000000;
        *(out++) = s;
        in += 3;
    }
}

static void U24LDecode( void *outp, const uint8_t *in, unsigned samples )
{
    uint32_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
    {
        uint32_t s = ((in[2] << 24) | (in[1] << 16) | (in[0] << 8)) - 0x80000000;
        *(out++) = s;
        in += 3;
    }
}

static void S24BDecode( void *outp, const uint8_t *in, unsigned samples )
{
    uint32_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
    {
        uint32_t s = ((in[0] << 24) | (in[1] << 16) | (in[2] << 8));
        *(out++) = s;
        in += 3;
    }
}

static void S24LDecode( void *outp, const uint8_t *in, unsigned samples )
{
    uint32_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
    {
        uint32_t s = ((in[2] << 24) | (in[1] << 16) | (in[0] << 8));
        *(out++) = s;
        in += 3;
    }
}

static void S24B32Decode( void *outp, const uint8_t *in, unsigned samples )
{
    uint32_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
    {
        *(out++) = GetDWBE( in ) << 8;
        in += 4;
    }
}

static void S24L32Decode( void *outp, const uint8_t *in, unsigned samples )
{
    uint32_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
    {
        *(out++) = GetDWLE( in ) << 8;
        in += 4;
    }
}

static void U32BDecode( void *outp, const uint8_t *in, unsigned samples )
{
    uint32_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
    {
        *(out++) = GetDWBE( in ) - 0x80000000;
        in += 4;
    }
}

static void U32LDecode( void *outp, const uint8_t *in, unsigned samples )
{
    uint32_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
    {
        *(out++) = GetDWLE( in ) - 0x80000000;
        in += 4;
    }
}

static void S32IDecode( void *outp, const uint8_t *in, unsigned samples )
{
    int32_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
    {
#ifdef WORDS_BIGENDIAN
        *(out++) = GetDWLE( in );
#else
        *(out++) = GetDWBE( in );
#endif
        in += 4;
    }
}

static void F32IDecode( void *outp, const uint8_t *in, unsigned samples )
{
    float *out = outp;

    for( size_t i = 0; i < samples; i++ )
    {
        union { float f; uint32_t u; } s;

#ifdef WORDS_BIGENDIAN
        s.u = GetDWLE( in );
#else
        s.u = GetDWBE( in );
#endif
        *(out++) = s.f;
        in += 4;
    }
}

static void F64IDecode( void *outp, const uint8_t *in, unsigned samples )
{
    double *out = outp;

    for( size_t i = 0; i < samples; i++ )
    {
        union { double d; uint64_t u; } s;

#ifdef WORDS_BIGENDIAN
        s.u = GetQWLE( in );
#else
        s.u = GetQWBE( in );
#endif
        *(out++) = s.d;
        in += 8;
    }
}

static int16_t dat12tos16( uint16_t y )
{
    static const uint16_t diff[16] = {
       0x0000, 0x0000, 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600,
       0x0A00, 0x0B00, 0x0C00, 0x0D00, 0x0E00, 0x0F00, 0x1000, 0x1000 };
    static const uint8_t shift[16] = {
        0, 0, 1, 2, 3, 4, 5, 6, 6, 5, 4, 3, 2, 1, 0, 0 };

    int d = y >> 8;
    return (y - diff[d]) << shift[d];
}

static void DAT12Decode( void *outp, const uint8_t *in, unsigned samples )
{
    int32_t *out = outp;

    while( samples >= 2 )
    {
        *(out++) = dat12tos16(U16_AT(in) >> 4);
        *(out++) = dat12tos16(U16_AT(in + 1) & ~0xF000);
        in += 3;
        samples -= 2;
    }

    if( samples )
        *(out++) = dat12tos16(U16_AT(in) >> 4);
}

/*****************************************************************************
 * DecoderClose: decoder destruction
 *****************************************************************************/
static void DecoderClose( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;

    free( p_dec->p_sys );
}

#ifdef ENABLE_SOUT
/* NOTE: Output buffers are always aligned since they are allocated by the araw plugin.
 * Contrary to the decoder, the encoder can also assume that input buffers are aligned,
 * since decoded audio blocks must always be aligned. */

static void U16IEncode( void *outp, const uint8_t *inp, unsigned samples )
{
    const uint16_t *in = (const uint16_t *)inp;
    uint16_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
        *(out++) =  bswap16( *(in++) + 0x8000 );
}

static void U16NEncode( void *outp, const uint8_t *inp, unsigned samples )
{
    const uint16_t *in = (const uint16_t *)inp;
    uint16_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
        *(out++) =  *(in++) + 0x8000;
}

static void U24BEncode( void *outp, const uint8_t *inp, unsigned samples )
{
    const uint32_t *in = (const uint32_t *)inp;
    uint8_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
    {
        uint32_t s = *(in++);
        *(out++) = (s >> 24) + 0x80;
        *(out++) = (s >> 16);
        *(out++) = (s >>  8);
    }
}

static void U24LEncode( void *outp, const uint8_t *inp, unsigned samples )
{
    const uint32_t *in = (const uint32_t *)inp;
    uint8_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
    {
        uint32_t s = *(in++);
        *(out++) = (s >>  8);
        *(out++) = (s >> 16);
        *(out++) = (s >> 24) + 0x80;
    }
}

static void S24BEncode( void *outp, const uint8_t *inp, unsigned samples )
{
    const uint32_t *in = (const uint32_t *)inp;
    uint8_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
    {
        uint32_t s = *(in++);
        *(out++) = (s >> 24);
        *(out++) = (s >> 16);
        *(out++) = (s >>  8);
    }
}

static void S24LEncode( void *outp, const uint8_t *inp, unsigned samples )
{
    const uint32_t *in = (const uint32_t *)inp;
    uint8_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
    {
        uint32_t s = *(in++);
        *(out++) = (s >>  8);
        *(out++) = (s >> 16);
        *(out++) = (s >> 24);
    }
}

static void U32IEncode( void *outp, const uint8_t *inp, unsigned samples )
{
    const uint32_t *in = (const uint32_t *)inp;
    uint32_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
        *(out++) =  bswap32( *(in++) + 0x80000000 );
}

static void U32NEncode( void *outp, const uint8_t *inp, unsigned samples )
{
    const uint32_t *in = (const uint32_t *)inp;
    uint32_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
        *(out++) =  *(in++) + 0x80000000;
}

static void S32IEncode( void *outp, const uint8_t *inp, unsigned samples )
{
    const int32_t *in = (const int32_t *)inp;
    int32_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
        *(out++) = bswap32( *(in++) );
}

static void F32IEncode( void *outp, const uint8_t *inp, unsigned samples )
{
    const float *in = (const float *)inp;
    uint8_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
    {
        union { float f; uint32_t u; char b[4]; } s;

        s.f = *(in++);
        s.u = bswap32( s.u );
        memcpy( out, s.b, 4 );
        out += 4;
    }
}

static void F64IEncode( void *outp, const uint8_t *inp, unsigned samples )
{
    const double *in = (const double *)inp;
    uint8_t *out = outp;

    for( size_t i = 0; i < samples; i++ )
    {
        union { double d; uint64_t u; char b[8]; } s;

        s.d = *(in++);
        s.u = bswap64( s.u );
        memcpy( out, s.b, 8 );
        out += 8;
    }
}

static block_t *Encode( encoder_t *enc, block_t *in )
{
    if( in == NULL )
        return NULL;

    block_t *out = block_Alloc( in->i_nb_samples
                                * enc->fmt_out.audio.i_bytes_per_frame );
    if( unlikely(out == NULL) )
        return NULL;

    out->i_flags      = in->i_flags;
    out->i_nb_samples = in->i_nb_samples;
    out->i_dts        = in->i_dts;
    out->i_pts        = in->i_pts;
    out->i_length     = in->i_length;
    out->i_nb_samples = in->i_nb_samples;

    void (*encode)(void *, const uint8_t *, unsigned) = (void *)enc->p_sys;
    if( encode != NULL )
        encode( out->p_buffer, in->p_buffer, in->i_nb_samples
                                             * enc->fmt_out.audio.i_channels );
    else
        memcpy( out->p_buffer, in->p_buffer, in->i_buffer );
    return out;
}

/**
 * Probes the PCM audio encoder.
 */
static int EncoderOpen( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    void (*encode)(void *, const uint8_t *, unsigned) = NULL;

    switch( p_enc->fmt_out.i_codec )
    {
    case VLC_CODEC_S8:
        encode = S8Decode;
    case VLC_CODEC_U8:
        p_enc->fmt_in.i_codec = VLC_CODEC_U8;
        p_enc->fmt_out.audio.i_bitspersample = 8;
        break;
    case VLC_CODEC_U16I:
        encode = U16IEncode;
        p_enc->fmt_in.i_codec = VLC_CODEC_S16N;
        p_enc->fmt_out.audio.i_bitspersample = 16;
        break;
    case VLC_CODEC_U16N:
        encode = U16NEncode;
        p_enc->fmt_in.i_codec = VLC_CODEC_S16N;
        p_enc->fmt_out.audio.i_bitspersample = 16;
        break;
    case VLC_CODEC_S16I:
        encode = S16IDecode;
    case VLC_CODEC_S16N:
        p_enc->fmt_in.i_codec = VLC_CODEC_S16N;
        p_enc->fmt_out.audio.i_bitspersample = 16;
        break;
    case VLC_CODEC_U24B:
        encode = U24BEncode;
        p_enc->fmt_in.i_codec = VLC_CODEC_S32N;
        p_enc->fmt_out.audio.i_bitspersample = 24;
        break;
    case VLC_CODEC_U24L:
        encode = U24LEncode;
        p_enc->fmt_in.i_codec = VLC_CODEC_S32N;
        p_enc->fmt_out.audio.i_bitspersample = 24;
        break;
    case VLC_CODEC_S24B:
        encode = S24BEncode;
        p_enc->fmt_in.i_codec = VLC_CODEC_S32N;
        p_enc->fmt_out.audio.i_bitspersample = 24;
        break;
    case VLC_CODEC_S24L:
        encode = S24LEncode;
        p_enc->fmt_in.i_codec = VLC_CODEC_S32N;
        p_enc->fmt_out.audio.i_bitspersample = 24;
        break;
    case VLC_CODEC_U32I:
        encode = U32IEncode;
        p_enc->fmt_in.i_codec = VLC_CODEC_S32N;
        p_enc->fmt_out.audio.i_bitspersample = 32;
        break;
    case VLC_CODEC_U32N:
        encode = U32NEncode;
        p_enc->fmt_in.i_codec = VLC_CODEC_S32N;
        p_enc->fmt_out.audio.i_bitspersample = 32;
        break;
    case VLC_CODEC_S32I:
        encode = S32IEncode;
    case VLC_CODEC_S32N:
        p_enc->fmt_in.i_codec = VLC_CODEC_S32N;
        p_enc->fmt_out.audio.i_bitspersample = 32;
        break;
#ifdef WORDS_BIGENDIAN
    case VLC_CODEC_F32L:
#else
    case VLC_CODEC_F32B:
#endif
        encode = F32IEncode;
    case VLC_CODEC_FL32:
        p_enc->fmt_in.i_codec = VLC_CODEC_FL32;
        p_enc->fmt_out.audio.i_bitspersample = 32;
        break;
#ifdef WORDS_BIGENDIAN
    case VLC_CODEC_F64L:
#else
    case VLC_CODEC_F64B:
#endif
        encode = F64IEncode;
    case VLC_CODEC_FL64:
        p_enc->fmt_in.i_codec = VLC_CODEC_FL64;
        p_enc->fmt_out.audio.i_bitspersample = 64;
        break;
    default:
        return VLC_EGENERIC;
    }

    p_enc->p_sys = (void *)encode;
    p_enc->pf_encode_audio = Encode;
    p_enc->fmt_out.audio.i_bytes_per_frame =
        (p_enc->fmt_out.audio.i_bitspersample / 8) *
        p_enc->fmt_in.audio.i_channels;
    p_enc->fmt_out.i_bitrate =
        p_enc->fmt_in.audio.i_channels *
        p_enc->fmt_in.audio.i_rate *
        p_enc->fmt_out.audio.i_bitspersample;

    msg_Dbg( p_enc, "samplerate:%dHz channels:%d bits/sample:%d",
             p_enc->fmt_out.audio.i_rate, p_enc->fmt_out.audio.i_channels,
             p_enc->fmt_out.audio.i_bitspersample );

    return VLC_SUCCESS;
}
#endif /* ENABLE_SOUT */
