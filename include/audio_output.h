/*****************************************************************************
 * audio_output.h : audio output interface
 *****************************************************************************
 * Copyright (C) 2002-2005 VideoLAN (Centrale Réseaux) and its contributors
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
#ifndef _VLC_AUDIO_OUTPUT_H
#define _VLC_AUDIO_OUTPUT_H 1

#include "vlc_es.h"

#define AOUT_FMTS_IDENTICAL( p_first, p_second ) (                          \
    ((p_first)->i_format == (p_second)->i_format)                           \
      && ((p_first)->i_rate == (p_second)->i_rate)                          \
      && ((p_first)->i_physical_channels == (p_second)->i_physical_channels)\
      && ((p_first)->i_original_channels == (p_second)->i_original_channels) )

/* Check if i_rate == i_rate and i_channels == i_channels */
#define AOUT_FMTS_SIMILAR( p_first, p_second ) (                            \
    ((p_first)->i_rate == (p_second)->i_rate)                               \
      && ((p_first)->i_physical_channels == (p_second)->i_physical_channels)\
      && ((p_first)->i_original_channels == (p_second)->i_original_channels) )

#ifdef WORDS_BIGENDIAN
#   define AOUT_FMT_S16_NE VLC_FOURCC('s','1','6','b')
#   define AOUT_FMT_U16_NE VLC_FOURCC('u','1','6','b')
#   define AOUT_FMT_S24_NE VLC_FOURCC('s','2','4','b')
#else
#   define AOUT_FMT_S16_NE VLC_FOURCC('s','1','6','l')
#   define AOUT_FMT_U16_NE VLC_FOURCC('u','1','6','l')
#   define AOUT_FMT_S24_NE VLC_FOURCC('s','2','4','l')
#endif

#define AOUT_FMT_NON_LINEAR( p_format )                                    \
    ( ((p_format)->i_format == VLC_FOURCC('s','p','d','i'))                \
       || ((p_format)->i_format == VLC_FOURCC('a','5','2',' '))            \
       || ((p_format)->i_format == VLC_FOURCC('d','t','s',' ')) )

/* This is heavily borrowed from libmad, by Robert Leslie <rob@mars.org> */
/*
 * Fixed-point format: 0xABBBBBBB
 * A == whole part      (sign + 3 bits)
 * B == fractional part (28 bits)
 *
 * Values are signed two's complement, so the effective range is:
 * 0x80000000 to 0x7fffffff
 *       -8.0 to +7.9999999962747097015380859375
 *
 * The smallest representable value is:
 * 0x00000001 == 0.0000000037252902984619140625 (i.e. about 3.725e-9)
 *
 * 28 bits of fractional accuracy represent about
 * 8.6 digits of decimal accuracy.
 *
 * Fixed-point numbers can be added or subtracted as normal
 * integers, but multiplication requires shifting the 64-bit result
 * from 56 fractional bits back to 28 (and rounding.)
 */
typedef int32_t vlc_fixed_t;
#define FIXED32_FRACBITS 28
#define FIXED32_MIN ((vlc_fixed_t) -0x80000000L)
#define FIXED32_MAX ((vlc_fixed_t) +0x7fffffffL)
#define FIXED32_ONE ((vlc_fixed_t) 0x10000000)


/*
 * Channels descriptions
 */

/* Values available for physical and original channels */
#define AOUT_CHAN_CENTER            0x1
#define AOUT_CHAN_LEFT              0x2
#define AOUT_CHAN_RIGHT             0x4
#define AOUT_CHAN_REARCENTER        0x10
#define AOUT_CHAN_REARLEFT          0x20
#define AOUT_CHAN_REARRIGHT         0x40
#define AOUT_CHAN_MIDDLELEFT        0x100
#define AOUT_CHAN_MIDDLERIGHT       0x200
#define AOUT_CHAN_LFE               0x1000

/* Values available for original channels only */
#define AOUT_CHAN_DOLBYSTEREO       0x10000
#define AOUT_CHAN_DUALMONO          0x20000
#define AOUT_CHAN_REVERSESTEREO     0x40000

#define AOUT_CHAN_PHYSMASK          0xFFFF
#define AOUT_CHAN_MAX               9

/* Values used for the audio-device and audio-channels object variables */
#define AOUT_VAR_MONO               1
#define AOUT_VAR_STEREO             2
#define AOUT_VAR_2F2R               4
#define AOUT_VAR_3F2R               5
#define AOUT_VAR_5_1                6
#define AOUT_VAR_6_1                7
#define AOUT_VAR_7_1                8
#define AOUT_VAR_SPDIF              10

#define AOUT_VAR_CHAN_STEREO        1
#define AOUT_VAR_CHAN_RSTEREO       2
#define AOUT_VAR_CHAN_LEFT          3
#define AOUT_VAR_CHAN_RIGHT         4
#define AOUT_VAR_CHAN_DOLBYS        5

/*****************************************************************************
 * aout_buffer_t : audio output buffer
 *****************************************************************************/
struct aout_buffer_t
{
    byte_t *                p_buffer;
    int                     i_alloc_type;
    /* i_size is the real size of the buffer (used for debug ONLY), i_nb_bytes
     * is the number of significative bytes in it. */
    size_t                  i_size, i_nb_bytes;
    unsigned int            i_nb_samples;
    mtime_t                 start_date, end_date;

    struct aout_buffer_t *  p_next;

    /** Private data (aout_buffer_t will disappear soon so no need for an
     * aout_buffer_sys_t type) */
    void * p_sys;

    /** This way the release can be overloaded */
    void (*pf_release)( aout_buffer_t * );
};

/* Size of a frame for S/PDIF output. */
#define AOUT_SPDIF_SIZE 6144

/* Number of samples in an A/52 frame. */
#define A52_FRAME_NB 1536

/*****************************************************************************
 * audio_date_t : date incrementation without long-term rounding errors
 *****************************************************************************/
struct audio_date_t
{
    mtime_t  date;
    uint32_t i_divider;
    uint32_t i_remainder;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
/* From common.c : */
#define aout_New(a) __aout_New(VLC_OBJECT(a))
VLC_EXPORT( aout_instance_t *, __aout_New, ( vlc_object_t * ) );
VLC_EXPORT( void, aout_Delete, ( aout_instance_t * ) );
VLC_EXPORT( void, aout_DateInit, ( audio_date_t *, uint32_t ) );
VLC_EXPORT( void, aout_DateSet, ( audio_date_t *, mtime_t ) );
VLC_EXPORT( void, aout_DateMove, ( audio_date_t *, mtime_t ) );
VLC_EXPORT( mtime_t, aout_DateGet, ( const audio_date_t * ) );
VLC_EXPORT( mtime_t, aout_DateIncrement, ( audio_date_t *, uint32_t ) );

VLC_EXPORT( int, aout_CheckChannelReorder, ( const uint32_t *, const uint32_t *, uint32_t, int, int * ) );
VLC_EXPORT( void, aout_ChannelReorder, ( uint8_t *, int, int, const int *, int ) );

/* From dec.c : */
#define aout_DecNew(a, b, c) __aout_DecNew(VLC_OBJECT(a), b, c)
VLC_EXPORT( aout_input_t *, __aout_DecNew, ( vlc_object_t *, aout_instance_t **, audio_sample_format_t * ) );
VLC_EXPORT( int, aout_DecDelete, ( aout_instance_t *, aout_input_t * ) );
VLC_EXPORT( aout_buffer_t *, aout_DecNewBuffer, ( aout_instance_t *, aout_input_t *, size_t ) );
VLC_EXPORT( void, aout_DecDeleteBuffer, ( aout_instance_t *, aout_input_t *, aout_buffer_t * ) );
VLC_EXPORT( int, aout_DecPlay, ( aout_instance_t *, aout_input_t *, aout_buffer_t * ) );

/* From intf.c : */
#define aout_VolumeGet(a, b) __aout_VolumeGet(VLC_OBJECT(a), b)
VLC_EXPORT( int, __aout_VolumeGet, ( vlc_object_t *, audio_volume_t * ) );
#define aout_VolumeSet(a, b) __aout_VolumeSet(VLC_OBJECT(a), b)
VLC_EXPORT( int, __aout_VolumeSet, ( vlc_object_t *, audio_volume_t ) );
#define aout_VolumeInfos(a, b) __aout_VolumeInfos(VLC_OBJECT(a), b)
VLC_EXPORT( int, __aout_VolumeInfos, ( vlc_object_t *, audio_volume_t * ) );
#define aout_VolumeUp(a, b, c) __aout_VolumeUp(VLC_OBJECT(a), b, c)
VLC_EXPORT( int, __aout_VolumeUp, ( vlc_object_t *, int, audio_volume_t * ) );
#define aout_VolumeDown(a, b, c) __aout_VolumeDown(VLC_OBJECT(a), b, c)
VLC_EXPORT( int, __aout_VolumeDown, ( vlc_object_t *, int, audio_volume_t * ) );
#define aout_VolumeMute(a, b) __aout_VolumeMute(VLC_OBJECT(a), b)
VLC_EXPORT( int, __aout_VolumeMute, ( vlc_object_t *, audio_volume_t * ) );
VLC_EXPORT( int, aout_Restart, ( aout_instance_t * p_aout ) );
VLC_EXPORT( int, aout_FindAndRestart, ( vlc_object_t *, const char *, vlc_value_t, vlc_value_t, void * ) );
VLC_EXPORT( int, aout_ChannelsRestart, ( vlc_object_t *, const char *, vlc_value_t, vlc_value_t, void * ) );

#endif /* _VLC_AUDIO_OUTPUT_H */
