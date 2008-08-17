/*****************************************************************************
 * format.c : PCM format converter
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_aout.h>
#include <vlc_block.h>
#include "vlc_filter.h"

#ifdef WORDS_BIGENDIAN
#   define AOUT_FMT_S24_IE VLC_FOURCC('s','2','4','l')
#   define AOUT_FMT_S16_IE VLC_FOURCC('s','1','6','l')
#   define AOUT_FMT_U16_IE VLC_FOURCC('u','1','6','l')
#else
#   define AOUT_FMT_S24_IE VLC_FOURCC('s','2','4','b')
#   define AOUT_FMT_S16_IE VLC_FOURCC('s','1','6','b')
#   define AOUT_FMT_U16_IE VLC_FOURCC('u','1','6','b')
#endif


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open ( vlc_object_t * );

static block_t *Float32toS24( filter_t *, block_t * );
static block_t *Float32toS16( filter_t *, block_t * );
static block_t *Float32toU16( filter_t *, block_t * );
static block_t *Float32toS8 ( filter_t *, block_t * );
static block_t *Float32toU8 ( filter_t *, block_t * );

static block_t *S24toFloat32  ( filter_t *, block_t * );
static block_t *S24toS16      ( filter_t *, block_t * );
static block_t *S24toS16Invert( filter_t *, block_t * );

static block_t *S16toFloat32  ( filter_t *, block_t * );
static block_t *S16toS24      ( filter_t *, block_t * );
static block_t *S16toS24Invert( filter_t *, block_t * );
static block_t *S16toS8       ( filter_t *, block_t * );
static block_t *S16toU8       ( filter_t *, block_t * );
static block_t *S16toU16      ( filter_t *, block_t * );

static block_t *U16toFloat32( filter_t *, block_t * );
static block_t *U16toS8     ( filter_t *, block_t * );
static block_t *U16toU8     ( filter_t *, block_t * );
static block_t *U16toS16    ( filter_t *, block_t * );

static block_t *Float32toS24Invert( filter_t *, block_t * );
static block_t *Float32toS16Invert( filter_t *, block_t * );
static block_t *Float32toU16Invert( filter_t *, block_t * );

static block_t *S24InverttoFloat32  ( filter_t *, block_t * );
static block_t *S24InverttoS16      ( filter_t *, block_t * );
static block_t *S24InverttoS16Invert( filter_t *, block_t * );

static block_t *S16InverttoFloat32  ( filter_t *, block_t * );
static block_t *S16InverttoS24      ( filter_t *, block_t * );
static block_t *S16InverttoS24Invert( filter_t *, block_t * );
static block_t *S16InverttoS8       ( filter_t *, block_t * );
static block_t *S16InverttoU8       ( filter_t *, block_t * );
static block_t *S16InverttoU16      ( filter_t *, block_t * );

static block_t *U16InverttoFloat32( filter_t *, block_t * );
static block_t *U16InverttoS8     ( filter_t *, block_t * );
static block_t *U16InverttoU8     ( filter_t *, block_t * );
static block_t *U16InverttoS16    ( filter_t *, block_t * );

static block_t *S8toFloat32  ( filter_t *, block_t * );
static block_t *S8toS16      ( filter_t *, block_t * );
static block_t *S8toU16      ( filter_t *, block_t * );
static block_t *S8toU8       ( filter_t *, block_t * );
static block_t *S8toS16Invert( filter_t *, block_t * );
static block_t *S8toU16Invert( filter_t *, block_t * );

static block_t *U8toFloat32  ( filter_t *, block_t * );
static block_t *U8toFloat32  ( filter_t *, block_t * );
static block_t *U8toS16      ( filter_t *, block_t * );
static block_t *U8toU16      ( filter_t *, block_t * );
static block_t *U8toS8       ( filter_t *, block_t * );
static block_t *U8toS16Invert( filter_t *, block_t * );
static block_t *U8toU16Invert( filter_t *, block_t * );


static block_t *U8toS8( filter_t *, block_t * );
static block_t *S8toU8( filter_t *, block_t * );


static block_t *Swap16( filter_t *, block_t * );
static block_t *Swap24( filter_t *, block_t * );

static const struct
{
    vlc_fourcc_t i_src;
    vlc_fourcc_t i_dst;
    block_t *(*pf_convert)( filter_t *, block_t *);
} ConvertTable[] =
{
    /* From fl32 */
    { VLC_FOURCC('f','l','3','2'), AOUT_FMT_S24_NE, Float32toS24 },
    { VLC_FOURCC('f','l','3','2'), AOUT_FMT_S16_NE, Float32toS16 },
    { VLC_FOURCC('f','l','3','2'), AOUT_FMT_U16_NE, Float32toU16 },
    { VLC_FOURCC('f','l','3','2'), AOUT_FMT_S24_IE, Float32toS24Invert },
    { VLC_FOURCC('f','l','3','2'), AOUT_FMT_S16_IE, Float32toS16Invert },
    { VLC_FOURCC('f','l','3','2'), AOUT_FMT_U16_IE, Float32toU16Invert },
    { VLC_FOURCC('f','l','3','2'), VLC_FOURCC('s','8',' ',' '), Float32toS8 },
    { VLC_FOURCC('f','l','3','2'), VLC_FOURCC('u','8',' ',' '), Float32toU8 },

    /* From s24 invert */
    { AOUT_FMT_S24_NE, VLC_FOURCC('f','l','3','2'), S24toFloat32 },
    { AOUT_FMT_S24_NE, AOUT_FMT_S24_IE,             Swap24 },
    { AOUT_FMT_S24_NE, AOUT_FMT_S16_NE,             S24toS16 },
    { AOUT_FMT_S24_NE, AOUT_FMT_S16_IE,             S24toS16Invert },

    /* From s16 */
    { AOUT_FMT_S16_NE, VLC_FOURCC('f','l','3','2'), S16toFloat32 },
    { AOUT_FMT_S16_NE, AOUT_FMT_S24_NE,             S16toS24 },
    { AOUT_FMT_S16_NE, AOUT_FMT_S24_IE,             S16toS24Invert },
    { AOUT_FMT_S16_NE, AOUT_FMT_S16_IE,             Swap16 },
    { AOUT_FMT_S16_NE, AOUT_FMT_U16_IE,             S16toU16 },
    { AOUT_FMT_S16_NE, VLC_FOURCC('s','8',' ',' '), S16toS8 },
    { AOUT_FMT_S16_NE, VLC_FOURCC('u','8',' ',' '), S16toU8 },

    /* From u16 */
    { AOUT_FMT_U16_NE, VLC_FOURCC('f','l','3','2'), U16toFloat32 },
    { AOUT_FMT_U16_NE, AOUT_FMT_U16_IE,             Swap16 },
    { AOUT_FMT_U16_NE, AOUT_FMT_S16_IE,             U16toS16 },
    { AOUT_FMT_U16_NE, VLC_FOURCC('s','8',' ',' '), U16toS8 },
    { AOUT_FMT_U16_NE, VLC_FOURCC('u','8',' ',' '), U16toU8 },

    /* From s8 */
    { VLC_FOURCC('s','8',' ',' '), VLC_FOURCC('f','l','3','2'), S8toFloat32 },
    { VLC_FOURCC('s','8',' ',' '), AOUT_FMT_S16_NE,             S8toS16 },
    { VLC_FOURCC('s','8',' ',' '), AOUT_FMT_S16_IE,             S8toS16Invert },
    { VLC_FOURCC('s','8',' ',' '), AOUT_FMT_U16_NE,             S8toU16 },
    { VLC_FOURCC('s','8',' ',' '), AOUT_FMT_U16_IE,             S8toU16Invert },
    { VLC_FOURCC('s','8',' ',' '), VLC_FOURCC('u','8',' ',' '), S8toU8 },
 
    /* From u8 */
    { VLC_FOURCC('u','8',' ',' '), VLC_FOURCC('f','l','3','2'), U8toFloat32 },
    { VLC_FOURCC('u','8',' ',' '), AOUT_FMT_S16_NE,             U8toS16 },
    { VLC_FOURCC('u','8',' ',' '), AOUT_FMT_S16_IE,             U8toS16Invert },
    { VLC_FOURCC('u','8',' ',' '), AOUT_FMT_U16_NE,             U8toU16 },
    { VLC_FOURCC('u','8',' ',' '), AOUT_FMT_U16_IE,             U8toU16Invert },
    { VLC_FOURCC('u','8',' ',' '), VLC_FOURCC('s','8',' ',' '), U8toS8 },

    /* From s24 invert */
    { AOUT_FMT_S24_IE, VLC_FOURCC('f','l','3','2'), S24InverttoFloat32 },
    { AOUT_FMT_S24_IE, AOUT_FMT_S24_NE,             Swap24 },
    { AOUT_FMT_S24_IE, AOUT_FMT_S16_NE,             S24InverttoS16 },
    { AOUT_FMT_S24_IE, AOUT_FMT_S16_IE,             S24InverttoS16Invert },

    /* From s16 invert */
    { AOUT_FMT_S16_IE, VLC_FOURCC('f','l','3','2'), S16InverttoFloat32 },
    { AOUT_FMT_S16_IE, AOUT_FMT_S24_NE,             S16InverttoS24 },
    { AOUT_FMT_S16_IE, AOUT_FMT_S24_IE,             S16InverttoS24Invert },
    { AOUT_FMT_S16_IE, AOUT_FMT_S16_NE,             Swap16 },
    { AOUT_FMT_S16_IE, AOUT_FMT_U16_NE,             S16InverttoU16 },
    { AOUT_FMT_S16_IE, VLC_FOURCC('s','8',' ',' '), S16InverttoS8 },
    { AOUT_FMT_S16_IE, VLC_FOURCC('u','8',' ',' '), S16InverttoU8 },

    /* From u16 invert */
    { AOUT_FMT_U16_IE, VLC_FOURCC('f','l','3','2'), U16InverttoFloat32 },
    { AOUT_FMT_U16_IE, AOUT_FMT_U16_NE,             Swap16 },
    { AOUT_FMT_U16_IE, AOUT_FMT_S16_NE,             U16InverttoS16 },
    { AOUT_FMT_U16_IE, VLC_FOURCC('s','8',' ',' '), U16InverttoS8 },
    { AOUT_FMT_U16_IE, VLC_FOURCC('u','8',' ',' '), U16InverttoU8 },

    { 0, 0, NULL },
};


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( N_("Audio filter for PCM format conversion") );
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_MISC );
    set_capability( "audio filter2", 1 );
    set_callbacks( Open, NULL );
vlc_module_end();

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    int i;

    for( i = 0; ConvertTable[i].pf_convert != NULL; i++ )
    {
        if( ConvertTable[i].i_src == p_filter->fmt_in.i_codec &&
            ConvertTable[i].i_dst == p_filter->fmt_out.i_codec )
            break;
    }
    if( ConvertTable[i].pf_convert == NULL )
        return VLC_EGENERIC;

    p_filter->pf_audio_filter = ConvertTable[i].pf_convert;
    p_filter->fmt_out.audio = p_filter->fmt_in.audio;
    p_filter->fmt_out.audio.i_format = p_filter->fmt_out.i_codec;
    p_filter->fmt_out.audio.i_bitspersample =
        aout_BitsPerSample( p_filter->fmt_out.i_codec );

    msg_Dbg( p_this, "%4.4s->%4.4s, bits per sample: %i->%i",
             (char *)&p_filter->fmt_in.i_codec,
             (char *)&p_filter->fmt_out.i_codec,
             p_filter->fmt_in.audio.i_bitspersample,
             p_filter->fmt_out.audio.i_bitspersample );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Convert a buffer
 *****************************************************************************/
static block_t *Float32toS24( filter_t *p_filter, block_t *p_block )
{
    VLC_UNUSED(p_filter);
    int i;
    float *p_in = (float *)p_block->p_buffer;
    uint8_t *p_out = (uint8_t *)p_in;
    int32_t out;

    for( i = p_block->i_buffer / 4; i--; )
    {
        if ( *p_in >= 1.0 ) out = 8388607;
        else if ( *p_in < -1.0 ) out = -8388608;
        else out = *p_in * 8388608.0;

#ifdef WORDS_BIGENDIAN
    *((int16_t *)p_out) = out >> 8;
    p_out[2] = out & 0xFF;
#else
    *((int16_t *)(p_out+1)) = out >> 8;
    p_out[0] = out & 0xFF;
#endif

        p_in++; p_out += 3;
    }

    p_block->i_buffer = p_block->i_buffer * 3 / 4;
    return p_block;
}

static block_t *Float32toS16( filter_t *p_filter, block_t *p_block )
{
    VLC_UNUSED(p_filter);
    int i;
    float *p_in = (float *)p_block->p_buffer;
    int16_t *p_out = (int16_t *)p_in;

    for( i = p_block->i_buffer / 4; i--; )
    {
#if 0
        /* Slow version. */
        if ( *p_in >= 1.0 ) *p_out = 32767;
        else if ( *p_in < -1.0 ) *p_out = -32768;
        else *p_out = *p_in * 32768.0;
#else
        /* This is walken's trick based on IEEE float format. */
        union { float f; int32_t i; } u;
        u.f = *p_in + 384.0;
        if ( u.i > 0x43c07fff ) *p_out = 32767;
        else if ( u.i < 0x43bf8000 ) *p_out = -32768;
        else *p_out = u.i - 0x43c00000;
#endif
        p_in++; p_out++;
    }

    p_block->i_buffer /= 2;
    return p_block;
}

static block_t *Float32toU16( filter_t *p_filter, block_t *p_block )
{
    VLC_UNUSED(p_filter);
    int i;
    float *p_in = (float *)p_block->p_buffer;
    uint16_t *p_out = (uint16_t *)p_in;

    for( i = p_block->i_buffer / 4; i--; )
    {
        if ( *p_in >= 1.0 ) *p_out = 65535;
        else if ( *p_in < -1.0 ) *p_out = 0;
        else *p_out = (uint16_t)(32768 + *p_in * 32768);
        p_in++; p_out++;
    }

    p_block->i_buffer /= 2;
    return p_block;
}

static block_t *S24toFloat32( filter_t *p_filter, block_t *p_block )
{
    block_t *p_block_out;
    uint8_t *p_in;
    float *p_out;
    int i;

    p_block_out =
        p_filter->pf_audio_buffer_new( p_filter, p_block->i_buffer * 4 / 3 );
    if( !p_block_out )
    {
        msg_Warn( p_filter, "can't get output buffer" );
        return NULL;
    }

    p_in = p_block->p_buffer;
    p_out = (float *)p_block_out->p_buffer;

    for( i = p_block->i_buffer / 3; i--; )
    {
        /* FIXME: unaligned reads */
#ifdef WORDS_BIGENDIAN
        *p_out = ((float)( (((int32_t)*(int16_t *)(p_in)) << 8) + p_in[2]))
#else
        *p_out = ((float)( (((int32_t)*(int16_t *)(p_in+1)) << 8) + p_in[0]))
#endif
            / 8388608.0;

        p_in += 3; p_out++;
    }

    p_block_out->i_samples = p_block->i_samples;
    p_block_out->i_dts = p_block->i_dts;
    p_block_out->i_pts = p_block->i_pts;
    p_block_out->i_length = p_block->i_length;
    p_block_out->i_rate = p_block->i_rate;

    block_Release( p_block );
    return p_block_out;
}

static block_t *S24toS16( filter_t *p_filter, block_t *p_block )
{
    VLC_UNUSED(p_filter);
    int i;
    uint8_t *p_in = (uint8_t *)p_block->p_buffer;
    uint8_t *p_out = (uint8_t *)p_in;

    for( i = p_block->i_buffer / 3; i--; )
    {
#ifdef WORDS_BIGENDIAN
        *p_out++ = *p_in++;
        *p_out++ = *p_in++;
        p_in++;
#else
        p_in++;
        *p_out++ = *p_in++;
        *p_out++ = *p_in++;
#endif
    }

    p_block->i_buffer = p_block->i_buffer * 2 / 3;
    return p_block;
}

static block_t *S16toFloat32( filter_t *p_filter, block_t *p_block )
{
    block_t *p_block_out;
    int16_t *p_in;
    float *p_out;
    int i;

    p_block_out =
        p_filter->pf_audio_buffer_new( p_filter, p_block->i_buffer*2 );
    if( !p_block_out )
    {
        msg_Warn( p_filter, "can't get output buffer" );
        return NULL;
    }

    p_in = (int16_t *)p_block->p_buffer;
    p_out = (float *)p_block_out->p_buffer;

    for( i = p_block->i_buffer / 2; i--; )
    {
#if 0
        /* Slow version */
        *p_out = (float)*p_in / 32768.0;
#else
        /* This is walken's trick based on IEEE float format. On my PIII
         * this takes 16 seconds to perform one billion conversions, instead
         * of 19 seconds for the above division. */
        union { float f; int32_t i; } u;
        u.i = *p_in + 0x43c00000;
        *p_out = u.f - 384.0;
#endif

        p_in++; p_out++;
    }

    p_block_out->i_samples = p_block->i_samples;
    p_block_out->i_dts = p_block->i_dts;
    p_block_out->i_pts = p_block->i_pts;
    p_block_out->i_length = p_block->i_length;
    p_block_out->i_rate = p_block->i_rate;

    block_Release( p_block );
    return p_block_out;
}

static block_t *U16toFloat32( filter_t *p_filter, block_t *p_block )
{
    block_t *p_block_out;
    uint16_t *p_in;
    float *p_out;
    int i;

    p_block_out =
        p_filter->pf_audio_buffer_new( p_filter, p_block->i_buffer*2 );
    if( !p_block_out )
    {
        msg_Warn( p_filter, "can't get output buffer" );
        return NULL;
    }

    p_in = (uint16_t *)p_block->p_buffer;
    p_out = (float *)p_block_out->p_buffer;

    for( i = p_block->i_buffer / 2; i--; )
    {
        *p_out++ = (float)(*p_in++ - 32768) / 32768.0;
    }

    p_block_out->i_samples = p_block->i_samples;
    p_block_out->i_dts = p_block->i_dts;
    p_block_out->i_pts = p_block->i_pts;
    p_block_out->i_length = p_block->i_length;
    p_block_out->i_rate = p_block->i_rate;

    block_Release( p_block );
    return p_block_out;
}

static block_t *S16toS24( filter_t *p_filter, block_t *p_block )
{
    block_t *p_block_out;
    uint8_t *p_in, *p_out;
    int i;

    p_block_out =
        p_filter->pf_audio_buffer_new( p_filter, p_block->i_buffer*3/2 );
    if( !p_block_out )
    {
        msg_Warn( p_filter, "can't get output buffer" );
        return NULL;
    }

    p_in = (uint8_t *)p_block->p_buffer;
    p_out = (uint8_t *)p_block_out->p_buffer;

    for( i = p_block->i_buffer / 2; i--; )
    {
#ifdef WORDS_BIGENDIAN
        *p_out++ = *p_in++;
        *p_out++ = *p_in++;
        *p_out++ = 0;
#else
        *p_out++ = 0;
        *p_out++ = *p_in++;
        *p_out++ = *p_in++;
#endif
    }

    p_block_out->i_samples = p_block->i_samples;
    p_block_out->i_dts = p_block->i_dts;
    p_block_out->i_pts = p_block->i_pts;
    p_block_out->i_length = p_block->i_length;
    p_block_out->i_rate = p_block->i_rate;

    block_Release( p_block );
    return p_block_out;
}

static block_t *S16toS8( filter_t *p_filter, block_t *p_block )
{
    VLC_UNUSED(p_filter);
    int i;
    int16_t *p_in = (int16_t *)p_block->p_buffer;
    int8_t *p_out = (int8_t *)p_in;

    for( i = p_block->i_buffer / 2; i--; )
        *p_out++ = (*p_in++) >> 8;

    p_block->i_buffer /= 2;
    return p_block;
}
static block_t *S16toU8( filter_t *p_filter, block_t *p_block )
{
    VLC_UNUSED(p_filter);
    int i;
    int16_t *p_in = (int16_t *)p_block->p_buffer;
    uint8_t *p_out = (uint8_t *)p_in;

    for( i = p_block->i_buffer / 2; i--; )
        *p_out++ = ((*p_in++) + 32768) >> 8;

    p_block->i_buffer /= 2;
    return p_block;
}
static block_t *S16toU16( filter_t *p_filter, block_t *p_block )
{
    VLC_UNUSED(p_filter);
    int i;
    int16_t *p_in = (int16_t *)p_block->p_buffer;
    uint16_t *p_out = (uint16_t *)p_in;

    for( i = p_block->i_buffer / 2; i--; )
        *p_out++ = (*p_in++) + 32768;

    return p_block;
}

static block_t *U16toS8( filter_t *p_filter, block_t *p_block )
{
    VLC_UNUSED(p_filter);
    int i;
    uint16_t *p_in = (uint16_t *)p_block->p_buffer;
    int8_t *p_out = (int8_t *)p_in;

    for( i = p_block->i_buffer / 2; i--; )
        *p_out++ = ((int)(*p_in++) - 32768) >> 8;

    p_block->i_buffer /= 2;
    return p_block;
}
static block_t *U16toU8( filter_t *p_filter, block_t *p_block )
{
    VLC_UNUSED(p_filter);
    int i;
    uint16_t *p_in = (uint16_t *)p_block->p_buffer;
    uint8_t *p_out = (uint8_t *)p_in;

    for( i = p_block->i_buffer / 2; i--; )
        *p_out++ = (*p_in++) >> 8;

    p_block->i_buffer /= 2;
    return p_block;
}
static block_t *U16toS16( filter_t *p_filter, block_t *p_block )
{
    VLC_UNUSED(p_filter);
    int i;
    uint16_t *p_in = (uint16_t *)p_block->p_buffer;
    int16_t *p_out = (int16_t *)p_in;

    for( i = p_block->i_buffer / 2; i--; )
        *p_out++ = (int)(*p_in++) - 32768;

    return p_block;
}

static block_t *S8toU8( filter_t *p_filter, block_t *p_block )
{
    VLC_UNUSED(p_filter);
    int i;
    int8_t *p_in = (int8_t *)p_block->p_buffer;
    uint8_t *p_out = (uint8_t *)p_in;

    for( i = p_block->i_buffer; i--; )
        *p_out++ = ((*p_in++) + 128);

    return p_block;
}
static block_t *U8toS8( filter_t *p_filter, block_t *p_block )
{
    VLC_UNUSED(p_filter);
    int i;
    uint8_t *p_in = (uint8_t *)p_block->p_buffer;
    int8_t *p_out = (int8_t *)p_in;

    for( i = p_block->i_buffer; i--; )
        *p_out++ = ((*p_in++) - 128);

    return p_block;
}

/* */
static block_t *S8toU16( filter_t *p_filter, block_t *p_block )
{
    block_t *p_block_out;
    int8_t *p_in;
    uint16_t *p_out;
    int i;

    p_block_out =
        p_filter->pf_audio_buffer_new( p_filter, p_block->i_buffer*2 );
    if( !p_block_out )
    {
        msg_Warn( p_filter, "can't get output buffer" );
        return NULL;
    }

    p_in = (int8_t *)p_block->p_buffer;
    p_out = (uint16_t *)p_block_out->p_buffer;

    for( i = p_block->i_buffer; i--; )
        *p_out++ = ((*p_in++) + 128) << 8;

    p_block_out->i_samples = p_block->i_samples;
    p_block_out->i_dts = p_block->i_dts;
    p_block_out->i_pts = p_block->i_pts;
    p_block_out->i_length = p_block->i_length;
    p_block_out->i_rate = p_block->i_rate;

    block_Release( p_block );
    return p_block_out;
}

static block_t *U8toS16( filter_t *p_filter, block_t *p_block )
{
    block_t *p_block_out;
    uint8_t *p_in;
    int16_t *p_out;
    int i;

    p_block_out =
        p_filter->pf_audio_buffer_new( p_filter, p_block->i_buffer*2 );
    if( !p_block_out )
    {
        msg_Warn( p_filter, "can't get output buffer" );
        return NULL;
    }

    p_in = (uint8_t *)p_block->p_buffer;
    p_out = (int16_t *)p_block_out->p_buffer;

    for( i = p_block->i_buffer; i--; )
        *p_out++ = ((*p_in++) - 128) << 8;

    p_block_out->i_samples = p_block->i_samples;
    p_block_out->i_dts = p_block->i_dts;
    p_block_out->i_pts = p_block->i_pts;
    p_block_out->i_length = p_block->i_length;
    p_block_out->i_rate = p_block->i_rate;

    block_Release( p_block );
    return p_block_out;
}


static block_t *S8toS16( filter_t *p_filter, block_t *p_block )
{
    block_t *p_block_out;
    int8_t *p_in;
    int16_t *p_out;
    int i;

    p_block_out =
        p_filter->pf_audio_buffer_new( p_filter, p_block->i_buffer*2 );
    if( !p_block_out )
    {
        msg_Warn( p_filter, "can't get output buffer" );
        return NULL;
    }

    p_in = (int8_t *)p_block->p_buffer;
    p_out = (int16_t *)p_block_out->p_buffer;

    for( i = p_block->i_buffer; i--; )
        *p_out++ = (*p_in++) << 8;

    p_block_out->i_samples = p_block->i_samples;
    p_block_out->i_dts = p_block->i_dts;
    p_block_out->i_pts = p_block->i_pts;
    p_block_out->i_length = p_block->i_length;
    p_block_out->i_rate = p_block->i_rate;

    block_Release( p_block );
    return p_block_out;
}

static block_t *U8toU16( filter_t *p_filter, block_t *p_block )
{
    block_t *p_block_out;
    uint8_t *p_in;
    uint16_t *p_out;
    int i;

    p_block_out =
        p_filter->pf_audio_buffer_new( p_filter, p_block->i_buffer*2 );
    if( !p_block_out )
    {
        msg_Warn( p_filter, "can't get output buffer" );
        return NULL;
    }

    p_in = (uint8_t *)p_block->p_buffer;
    p_out = (uint16_t *)p_block_out->p_buffer;

    for( i = p_block->i_buffer; i--; )
        *p_out++ = (*p_in++) << 8;

    p_block_out->i_samples = p_block->i_samples;
    p_block_out->i_dts = p_block->i_dts;
    p_block_out->i_pts = p_block->i_pts;
    p_block_out->i_length = p_block->i_length;
    p_block_out->i_rate = p_block->i_rate;

    block_Release( p_block );
    return p_block_out;
}

/*****************************************************************************
 * Swap a buffer of words
 *****************************************************************************/
static block_t *Swap16( filter_t *p_filter, block_t *p_block )
{
    VLC_UNUSED(p_filter);
    size_t i;
    uint8_t *p_in = (uint8_t *)p_block->p_buffer;
    uint8_t tmp;

    for( i = 0; i < p_block->i_buffer / 2; i++ )
    {
        tmp = p_in[0];
        p_in[0] = p_in[1];
        p_in[1] = tmp;
        p_in += 2;
    }

    return p_block;
}

static block_t *Swap24( filter_t *p_filter, block_t *p_block )
{
    VLC_UNUSED(p_filter);
    size_t i;
    uint8_t *p_in = (uint8_t *)p_block->p_buffer;
    uint8_t tmp;

    for( i = 0; i < p_block->i_buffer / 3; i++ )
    {
        tmp = p_in[0];
        p_in[0] = p_in[2];
        p_in[2] = tmp;
        p_in += 3;
    }

    return p_block;
}

#define CONVERT_NN( func, f_in, f_out, b_pre_invert, b_post_invert, swapa, swapb ) \
static block_t *func( filter_t *p_filter, block_t *p_block ) \
{                                                   \
    if( b_pre_invert )                              \
        swapa( p_filter, p_block );                  \
                                                    \
    p_block = f_in##to##f_out( p_filter, p_block ); \
                                                    \
    if( b_post_invert )                             \
        swapb( p_filter, p_block );                  \
                                                    \
    return p_block;                                 \
}

CONVERT_NN( Float32toS24Invert, Float32, S24, 0, 1, Swap24, Swap24 )
CONVERT_NN( Float32toS16Invert, Float32, S16, 0, 1, Swap16, Swap16 )
CONVERT_NN( Float32toU16Invert, Float32, U16, 0, 1, Swap16, Swap16 )

CONVERT_NN( S24InverttoFloat32, S24, Float32, 1, 0, Swap24, Swap24 )
CONVERT_NN( S24InverttoS16,     S24, S16,     1, 0, Swap24, Swap16 )
CONVERT_NN( S24InverttoS16Invert, S24, S16,   1, 1, Swap24, Swap16 )
CONVERT_NN( S24toS16Invert,     S24, S16,     0, 1, Swap24, Swap16 )

CONVERT_NN( S16InverttoFloat32, S16, Float32, 1, 0, Swap16, Swap16 )
CONVERT_NN( S16InverttoS24,     S16, S24,     1, 0, Swap16, Swap24 )
CONVERT_NN( S16toS24Invert,     S16, S24,     0, 1, Swap16, Swap24 )
CONVERT_NN( S16InverttoS24Invert, S16, S24,   1, 1, Swap16, Swap24 )
CONVERT_NN( S16InverttoS8,      S16, S8,      1, 0, Swap16, Swap16 )
CONVERT_NN( S16InverttoU8,      S16, U8,      1, 0, Swap16, Swap16 )
CONVERT_NN( S16InverttoU16,     S16, U16,     1, 0, Swap16, Swap16 )

CONVERT_NN( U16InverttoFloat32, U16, Float32, 1, 0, Swap16, Swap16 )
CONVERT_NN( U16InverttoS8,      U16, S8,      1, 0, Swap16, Swap16 )
CONVERT_NN( U16InverttoU8,      U16, U8,      1, 0, Swap16, Swap16 )
CONVERT_NN( U16InverttoS16,     U16, S16,     1, 0, Swap16, Swap16 )

#undef CONVERT_NN

#define CONVERT_INDIRECT( func, f_in, f_mid, f_out )                    \
static block_t *func( filter_t *p_filter, block_t *p_block )            \
{                                                                       \
    return f_mid##to##f_out( p_filter,                                  \
                             f_in##to##f_mid( p_filter, p_block ) );    \
}

CONVERT_INDIRECT( Float32toS8,   Float32, S16, U8 )
CONVERT_INDIRECT( Float32toU8,   Float32, U16, U8 )
CONVERT_INDIRECT( S8toFloat32,   S8,      S16, Float32 )
CONVERT_INDIRECT( U8toFloat32,   U8,      U16, Float32 )

#define S16toS16Invert Swap16
#define U16toU16Invert Swap16

CONVERT_INDIRECT( U8toS16Invert, U8,      S16, S16Invert )
CONVERT_INDIRECT( S8toU16Invert, S8,      U16, U16Invert )

CONVERT_INDIRECT( U8toU16Invert, U8,      U16, U16Invert )
CONVERT_INDIRECT( S8toS16Invert, S8,      S16, S16Invert )

#undef CONVERT_INDIRECT
