/*****************************************************************************
 * format.c : PCM format converter
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: float32tos16.c 8391 2004-08-06 17:28:36Z sam $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include "vlc_filter.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open ( vlc_object_t * );

static block_t *Float32toS16( filter_t *, block_t * );
static block_t *Float32toU16( filter_t *, block_t * );
static block_t *S16toFloat32( filter_t *, block_t * );
static block_t *S16Invert   ( filter_t *, block_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("audio filter for PCM format conversion") );
    set_capability( "audio filter2", 1 );
    set_callbacks( Open, NULL );
vlc_module_end();

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    if( p_filter->fmt_in.i_codec == VLC_FOURCC('f','l','3','2') &&
        p_filter->fmt_out.i_codec == AUDIO_FMT_S16_NE )
    {
        p_filter->pf_audio_filter = Float32toS16;
    }
    else if( p_filter->fmt_in.i_codec == VLC_FOURCC('f','l','3','2') &&
             p_filter->fmt_out.i_codec == AUDIO_FMT_U16_NE )
    {
        p_filter->pf_audio_filter = Float32toU16;
    }
    else if( p_filter->fmt_in.i_codec == AUDIO_FMT_S16_NE &&
             p_filter->fmt_out.i_codec == VLC_FOURCC('f','l','3','2') )
    {
        p_filter->pf_audio_filter = S16toFloat32;
    }
    else if( ( p_filter->fmt_in.i_codec == VLC_FOURCC('s','1','6','l') &&
               p_filter->fmt_out.i_codec == VLC_FOURCC('s','1','6','b') ) ||
             ( p_filter->fmt_in.i_codec == VLC_FOURCC('s','1','6','b') &&
               p_filter->fmt_out.i_codec == VLC_FOURCC('s','1','6','l') ) )
    {
        p_filter->pf_audio_filter = S16Invert;
    }
    else return VLC_EGENERIC;
    
    msg_Dbg( p_this, "%4.4s->%4.4s, bits per sample: %i",
             (char *)&p_filter->fmt_in.i_codec,
             (char *)&p_filter->fmt_out.i_codec,
             p_filter->fmt_in.audio.i_bitspersample );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Convert a buffer
 *****************************************************************************/
static block_t *Float32toS16( filter_t *p_filter, block_t *p_block )
{
    int i;
    float *p_in = (float *)p_block->p_buffer;
    int16_t *p_out = (int16_t *)p_in;

    for( i = p_block->i_buffer*8/p_filter->fmt_in.audio.i_bitspersample; i--; )
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
    int i;
    float *p_in = (float *)p_block->p_buffer;
    uint16_t *p_out = (uint16_t *)p_in;

    for( i = p_block->i_buffer*8/p_filter->fmt_in.audio.i_bitspersample; i--; )
    {
        if ( *p_in >= 1.0 ) *p_out = 65535;
        else if ( *p_in < -1.0 ) *p_out = 0;
        else *p_out = (uint16_t)(32768 + *p_in * 32768);
        p_in++; p_out++;
    }

    p_block->i_buffer /= 2;
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

    for( i = p_block->i_buffer*8/p_filter->fmt_in.audio.i_bitspersample; i--; )
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

    p_block->pf_release( p_block );
    return p_block_out;
}

static block_t *S16Invert( filter_t *p_filter, block_t *p_block )
{
    int i;
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
