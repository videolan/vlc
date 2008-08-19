/*****************************************************************************
 * float.c: Floating point audio format conversions
 *****************************************************************************
 * Copyright (C) 2002, 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Xavier Maillard <zedek@fxgsproject.org>
 *          Henri Fallon <henri@videolan.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
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

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#ifdef HAVE_ALLOCA_H
#   include <alloca.h>
#endif

#include <vlc_aout.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create_F32ToFL32 ( vlc_object_t * );
static void Do_F32ToFL32( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                           aout_buffer_t * );
static void Do_FL32ToF32 ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                           aout_buffer_t * );

static int  Create_FL32ToS16 ( vlc_object_t * );
static void Do_FL32ToS16( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                           aout_buffer_t * );

static int  Create_FL32ToS8 ( vlc_object_t * );
static void Do_FL32ToS8( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                           aout_buffer_t * );

static int  Create_FL32ToU16 ( vlc_object_t * );
static void Do_FL32ToU16( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                           aout_buffer_t * );

static int  Create_FL32ToU8 ( vlc_object_t * );
static void Do_FL32ToU8( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                           aout_buffer_t * );

static int  Create_S16ToFL32( vlc_object_t * );
static void Do_S16ToFL32( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                           aout_buffer_t * );
static void Do_S24ToFL32( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                           aout_buffer_t * );
static void Do_S32ToFL32( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                           aout_buffer_t * );

static int  Create_S16ToFL32_SW( vlc_object_t * );
static void Do_S16ToFL32_SW( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                           aout_buffer_t * );
static void Do_S24ToFL32_SW( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                           aout_buffer_t * );
static void Do_S32ToFL32_SW( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                           aout_buffer_t * );

static int  Create_S8ToFL32( vlc_object_t * );
static void Do_S8ToFL32( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                           aout_buffer_t * );

static int  Create_U8ToFL32( vlc_object_t * );
static void Do_U8ToFL32( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                           aout_buffer_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( N_("Floating-point audio format conversions") );
    add_submodule();
        set_capability( "audio filter", 10 );
        set_callbacks( Create_F32ToFL32, NULL );
    add_submodule();
        set_capability( "audio filter", 1 );
        set_callbacks( Create_FL32ToS16, NULL );
    add_submodule();
        set_capability( "audio filter", 1 );
        set_callbacks( Create_FL32ToS8, NULL );
    add_submodule();
        set_capability( "audio filter", 1 );
        set_callbacks( Create_FL32ToU16, NULL );
    add_submodule();
        set_capability( "audio filter", 1 );
        set_callbacks( Create_FL32ToU8, NULL );
    add_submodule();
        set_capability( "audio filter", 1 );
        set_callbacks( Create_S16ToFL32, NULL );
    add_submodule();
        set_capability( "audio filter", 1 );
        set_callbacks( Create_S16ToFL32_SW, NULL ); /* Endianness conversion*/
    add_submodule();
        set_capability( "audio filter", 1 );
        set_callbacks( Create_S8ToFL32, NULL );
    add_submodule();
        set_capability( "audio filter", 1 );
        set_callbacks( Create_U8ToFL32, NULL );
vlc_module_end();

/*****************************************************************************
 * Fixed 32 to Float 32 and backwards
 *****************************************************************************/
static int Create_F32ToFL32( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if( ( p_filter->input.i_format != VLC_FOURCC('f','i','3','2')
           || p_filter->output.i_format != VLC_FOURCC('f','l','3','2') )
      && ( p_filter->input.i_format != VLC_FOURCC('f','l','3','2')
            || p_filter->output.i_format != VLC_FOURCC('f','i','3','2') ) )
    {
        return -1;
    }

    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        return -1;
    }

    if( p_filter->input.i_format == VLC_FOURCC('f','i','3','2') )
    {
        p_filter->pf_do_work = Do_F32ToFL32;
    }
    else
    {
        p_filter->pf_do_work = Do_FL32ToF32;
    }

    p_filter->b_in_place = 1;

    return 0;
}

static void Do_F32ToFL32( aout_instance_t * p_aout, aout_filter_t * p_filter,
                          aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    int i;
    vlc_fixed_t * p_in = (vlc_fixed_t *)p_in_buf->p_buffer;
    float * p_out = (float *)p_out_buf->p_buffer;

    for ( i = p_in_buf->i_nb_samples
               * aout_FormatNbChannels( &p_filter->input ) ; i-- ; )
    {
        *p_out++ = (float)*p_in++ / (float)FIXED32_ONE;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes;
}

static void Do_FL32ToF32( aout_instance_t * p_aout, aout_filter_t * p_filter,
                          aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    int i;
    float * p_in = (float *)p_in_buf->p_buffer;
    vlc_fixed_t * p_out = (vlc_fixed_t *)p_out_buf->p_buffer;

    for ( i = p_in_buf->i_nb_samples
               * aout_FormatNbChannels( &p_filter->input ) ; i-- ; )
    {
        *p_out++ = (vlc_fixed_t)( *p_in++ * (float)FIXED32_ONE );
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes;
}

/*****************************************************************************
 * FL32 To S16
 *****************************************************************************/
static int Create_FL32ToS16( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( p_filter->input.i_format != VLC_FOURCC('f','l','3','2')
          || p_filter->output.i_format != AOUT_FMT_S16_NE )
    {
        return -1;
    }

    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        return -1;
    }

    p_filter->pf_do_work = Do_FL32ToS16;
    p_filter->b_in_place = 1;

    return 0;
}

static void Do_FL32ToS16( aout_instance_t * p_aout, aout_filter_t * p_filter,
                          aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    int i;
    float * p_in = (float *)p_in_buf->p_buffer;
    int16_t * p_out = (int16_t *)p_out_buf->p_buffer;

    for ( i = p_in_buf->i_nb_samples
               * aout_FormatNbChannels( &p_filter->input ); i-- ; )
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

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes / 2;
}

/*****************************************************************************
 * FL32 To S8
 *****************************************************************************/
static int Create_FL32ToS8( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( p_filter->input.i_format != VLC_FOURCC('f','l','3','2')
          || p_filter->output.i_format != VLC_FOURCC('s','8',' ',' ') )
    {
        return -1;
    }

    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        return -1;
    }

    p_filter->pf_do_work = Do_FL32ToS8;
    p_filter->b_in_place = 1;

    return 0;
}

static void Do_FL32ToS8( aout_instance_t * p_aout, aout_filter_t * p_filter,
                         aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    int i;
    float * p_in = (float *)p_in_buf->p_buffer;
    int8_t * p_out = (int8_t *)p_out_buf->p_buffer;

    for ( i = p_in_buf->i_nb_samples
               * aout_FormatNbChannels( &p_filter->input ); i-- ; )
    {
        if ( *p_in >= 1.0 ) *p_out = 127;
        else if ( *p_in < -1.0 ) *p_out = -128;
        else *p_out = (int8_t)(*p_in * 128);
        p_in++; p_out++;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes / 4;
}

/*****************************************************************************
 * FL32 To U16
 *****************************************************************************/
static int Create_FL32ToU16( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( p_filter->input.i_format != VLC_FOURCC('f','l','3','2')
          || p_filter->output.i_format != AOUT_FMT_U16_NE )
    {
        return -1;
    }

    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        return -1;
    }

    p_filter->pf_do_work = Do_FL32ToU16;
    p_filter->b_in_place = 1;

    return 0;
}

static void Do_FL32ToU16( aout_instance_t * p_aout, aout_filter_t * p_filter,
                          aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    int i;
    float * p_in = (float *)p_in_buf->p_buffer;
    uint16_t * p_out = (uint16_t *)p_out_buf->p_buffer;

    for ( i = p_in_buf->i_nb_samples
               * aout_FormatNbChannels( &p_filter->input ); i-- ; )
    {
        if ( *p_in >= 1.0 ) *p_out = 65535;
        else if ( *p_in < -1.0 ) *p_out = 0;
        else *p_out = (uint16_t)(32768 + *p_in * 32768);
        p_in++; p_out++;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes / 2;
}

/*****************************************************************************
 * FL32 To U8
 *****************************************************************************/
static int Create_FL32ToU8( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( p_filter->input.i_format != VLC_FOURCC('f','l','3','2')
          || p_filter->output.i_format != VLC_FOURCC('u','8',' ',' ') )
    {
        return -1;
    }

    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        return -1;
    }

    p_filter->pf_do_work = Do_FL32ToU8;
    p_filter->b_in_place = 1;

    return 0;
}

static void Do_FL32ToU8( aout_instance_t * p_aout, aout_filter_t * p_filter,
                         aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    int i;
    float * p_in = (float *)p_in_buf->p_buffer;
    uint8_t * p_out = (uint8_t *)p_out_buf->p_buffer;

    for ( i = p_in_buf->i_nb_samples
               * aout_FormatNbChannels( &p_filter->input ); i-- ; )
    {
        if ( *p_in >= 1.0 ) *p_out = 255;
        else if ( *p_in < -1.0 ) *p_out = 0;
        else *p_out = (uint8_t)(128 + *p_in * 128);
        p_in++; p_out++;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes / 4;
}

/*****************************************************************************
 * S16 To Float32
 *****************************************************************************/
static int Create_S16ToFL32( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( ( p_filter->input.i_format != AOUT_FMT_S16_NE &&
           p_filter->input.i_format != AOUT_FMT_S24_NE &&
           p_filter->input.i_format != AOUT_FMT_S32_NE )
          || p_filter->output.i_format != VLC_FOURCC('f','l','3','2') )
    {
        return -1;
    }

    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        return -1;
    }

    if( p_filter->input.i_format == AOUT_FMT_S32_NE )
        p_filter->pf_do_work = Do_S32ToFL32;
    else if( p_filter->input.i_format == AOUT_FMT_S24_NE )
        p_filter->pf_do_work = Do_S24ToFL32;
    else
        p_filter->pf_do_work = Do_S16ToFL32;

    p_filter->b_in_place = true;

    return 0;
}

static void Do_S16ToFL32( aout_instance_t * p_aout, aout_filter_t * p_filter,
                          aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    int i = p_in_buf->i_nb_samples * aout_FormatNbChannels( &p_filter->input );

    /* We start from the end because b_in_place is true */
    int16_t * p_in = (int16_t *)p_in_buf->p_buffer + i - 1;
    float * p_out = (float *)p_out_buf->p_buffer + i - 1;

    while( i-- )
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

        p_in--; p_out--;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes * 4 / 2;
}

static void Do_S24ToFL32( aout_instance_t * p_aout, aout_filter_t * p_filter,
                          aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    int i = p_in_buf->i_nb_samples * aout_FormatNbChannels( &p_filter->input );

    /* We start from the end because b_in_place is true */
    uint8_t * p_in = (uint8_t *)p_in_buf->p_buffer + (i - 1) * 3;
    float * p_out = (float *)p_out_buf->p_buffer + i - 1;

    while( i-- )
    {
#ifdef WORDS_BIGENDIAN
        *p_out = ((float)( (((int32_t)*(int16_t *)(p_in)) << 8) + p_in[2]))
#else
        *p_out = ((float)( (((int32_t)*(int16_t *)(p_in+1)) << 8) + p_in[0]))
#endif
            / 8388608.0;

        p_in -= 3; p_out--;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes * 4 / 3;
}

static void Do_S32ToFL32( aout_instance_t * p_aout, aout_filter_t * p_filter,
                          aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    int i = p_in_buf->i_nb_samples * aout_FormatNbChannels( &p_filter->input );

    /* We start from the end because b_in_place is true */
    int32_t * p_in = (int32_t *)p_in_buf->p_buffer + i - 1;
    float * p_out = (float *)p_out_buf->p_buffer + i - 1;

    while( i-- )
    {
        *p_out-- = (float)*p_in-- / 2147483648.0;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes * 4 / 4;
}

/*****************************************************************************
 * S16 To Float32 with endianness conversion
 *****************************************************************************/
static int Create_S16ToFL32_SW( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        return -1;
    }

    if ( (p_filter->input.i_format == VLC_FOURCC('s','1','6','l') ||
         p_filter->input.i_format == VLC_FOURCC('s','1','6','b'))
         && p_filter->output.i_format == VLC_FOURCC('f','l','3','2')
         && p_filter->input.i_format != AOUT_FMT_S16_NE )
    {
        p_filter->pf_do_work = Do_S16ToFL32_SW;
        p_filter->b_in_place = true;

        return 0;
    }

    if ( (p_filter->input.i_format == VLC_FOURCC('s','2','4','l') ||
         p_filter->input.i_format == VLC_FOURCC('s','2','4','b'))
         && p_filter->output.i_format == VLC_FOURCC('f','l','3','2')
         && p_filter->input.i_format != AOUT_FMT_S24_NE )
    {
        p_filter->pf_do_work = Do_S24ToFL32_SW;
        p_filter->b_in_place = true;

        return 0;
    }

    if ( (p_filter->input.i_format == VLC_FOURCC('s','3','2','l') ||
         p_filter->input.i_format == VLC_FOURCC('s','3','2','b'))
         && p_filter->output.i_format == VLC_FOURCC('f','l','3','2')
         && p_filter->input.i_format != AOUT_FMT_S32_NE )
    {
        p_filter->pf_do_work = Do_S32ToFL32_SW;
        p_filter->b_in_place = true;

        return 0;
    }

    return -1;
}

static void Do_S16ToFL32_SW( aout_instance_t * p_aout, aout_filter_t * p_filter,
                             aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    int i = p_in_buf->i_nb_samples * aout_FormatNbChannels( &p_filter->input );

    /* We start from the end because b_in_place is true */
    int16_t * p_in;
    float * p_out = (float *)p_out_buf->p_buffer + i - 1;

#ifdef HAVE_SWAB
#   ifdef HAVE_ALLOCA
    int16_t * p_swabbed = alloca( i * sizeof(int16_t) );
#   else
    int16_t * p_swabbed = malloc( i * sizeof(int16_t) );
#   endif

    swab( p_in_buf->p_buffer, (void *)p_swabbed, i * sizeof(int16_t) );
    p_in = p_swabbed + i - 1;

#else
    uint8_t p_tmp[2];
    p_in = (int16_t *)p_in_buf->p_buffer + i - 1;
#endif

    while( i-- )
    {
#ifndef HAVE_SWAB
        p_tmp[0] = ((uint8_t *)p_in)[1];
        p_tmp[1] = ((uint8_t *)p_in)[0];
        *p_out = (float)( *(int16_t *)p_tmp ) / 32768.0;
#else
        *p_out = (float)*p_in / 32768.0;
#endif
        p_in--; p_out--;
    }

#ifdef HAVE_SWAB
#   ifndef HAVE_ALLOCA
    free( p_swabbed );
#   endif
#endif

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes * 4 / 2;
}

static void Do_S24ToFL32_SW( aout_instance_t * p_aout, aout_filter_t * p_filter,
                             aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    int i = p_in_buf->i_nb_samples * aout_FormatNbChannels( &p_filter->input );

    /* We start from the end because b_in_place is true */
    uint8_t * p_in = (uint8_t *)p_in_buf->p_buffer + (i - 1) * 3;
    float * p_out = (float *)p_out_buf->p_buffer + i - 1;

    uint8_t p_tmp[3];

    while( i-- )
    {
        p_tmp[0] = p_in[2];
        p_tmp[1] = p_in[1];
        p_tmp[2] = p_in[0];

#ifdef WORDS_BIGENDIAN
        *p_out = ((float)( (((int32_t)*(int16_t *)(p_tmp)) << 8) + p_tmp[2]))
#else
        *p_out = ((float)( (((int32_t)*(int16_t *)(p_tmp+1)) << 8) + p_tmp[0]))
#endif
            / 8388608.0;

        p_in -= 3; p_out--;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes * 4 / 3;
}

static void Do_S32ToFL32_SW( aout_instance_t * p_aout, aout_filter_t * p_filter,
                             aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    int i = p_in_buf->i_nb_samples * aout_FormatNbChannels( &p_filter->input );

    /* We start from the end because b_in_place is true */
    int32_t * p_in = (int32_t *)p_in_buf->p_buffer + i - 1;
    float * p_out = (float *)p_out_buf->p_buffer + i - 1;

    while( i-- )
    {
        *p_out-- = (float)*p_in-- / 2147483648.0;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes * 4 / 4;
}


/*****************************************************************************
 * S8 To FL32
 *****************************************************************************/
static int Create_S8ToFL32( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( p_filter->input.i_format != VLC_FOURCC('s','8',' ',' ')
          || p_filter->output.i_format != VLC_FOURCC('f','l','3','2') )
    {
        return -1;
    }

    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        return -1;
    }

    p_filter->pf_do_work = Do_S8ToFL32;
    p_filter->b_in_place = true;

    return 0;
}

static void Do_S8ToFL32( aout_instance_t * p_aout, aout_filter_t * p_filter,
                         aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    int i = p_in_buf->i_nb_samples * aout_FormatNbChannels( &p_filter->input );

    /* We start from the end because b_in_place is true */
    int8_t * p_in = (int8_t *)p_in_buf->p_buffer + i - 1;
    float * p_out = (float *)p_out_buf->p_buffer + i - 1;

    while( i-- )
    {
        *p_out = (float)(*p_in) / 128.0;
        p_in--; p_out--;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes * sizeof(float);
}

/*****************************************************************************
 * U8 To FL32
 *****************************************************************************/
static int Create_U8ToFL32( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;

    if ( p_filter->input.i_format != VLC_FOURCC('u','8',' ',' ')
          || p_filter->output.i_format != VLC_FOURCC('f','l','3','2') )
    {
        return -1;
    }

    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        return -1;
    }

    p_filter->pf_do_work = Do_U8ToFL32;
    p_filter->b_in_place = true;

    return 0;
}

static void Do_U8ToFL32( aout_instance_t * p_aout, aout_filter_t * p_filter,
                         aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    VLC_UNUSED(p_aout);
    int i = p_in_buf->i_nb_samples * aout_FormatNbChannels( &p_filter->input );

    /* We start from the end because b_in_place is true */
    uint8_t * p_in = (uint8_t *)p_in_buf->p_buffer + i - 1;
    float * p_out = (float *)p_out_buf->p_buffer + i - 1;

    while( i-- )
    {
        *p_out = ((float)*p_in -128) / 128.0;
        p_in--; p_out--;
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes * sizeof(float);
}
