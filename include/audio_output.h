/*****************************************************************************
 * audio_output.h : audio output interface
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: audio_output.h,v 1.72 2002/11/28 23:24:14 massiot Exp $
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
 * audio_sample_format_t
 *****************************************************************************
 * This structure defines a format for audio samples.
 *****************************************************************************/
struct audio_sample_format_t
{
    vlc_fourcc_t        i_format;
    unsigned int        i_rate;
    /* Describes the channels configuration of the samples (ie. number of
     * channels which are available in the buffer, and positions). */
    u32                 i_physical_channels;
    /* Describes from which original channels, before downmixing, the
     * buffer is derived. */
    u32                 i_original_channels;
    /* Optional - for A/52, SPDIF and DTS types : */
    /* Bytes used by one compressed frame, depends on bitrate. */
    unsigned int        i_bytes_per_frame;
    /* Number of sampleframes contained in one compressed frame. */
    unsigned int        i_frame_length;
    /* Please note that it may be completely arbitrary - buffers are not
     * obliged to contain a integral number of so-called "frames". It's
     * just here for the division :
     * buffer_size = i_nb_samples * i_bytes_per_frame / i_frame_length
     */
};

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
#else
#   define AOUT_FMT_S16_NE VLC_FOURCC('s','1','6','l')
#   define AOUT_FMT_U16_NE VLC_FOURCC('u','1','6','l')
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
#define AOUT_CHAN_LFE               0x100

/* Values available for original channels only */
#define AOUT_CHAN_DOLBYSTEREO       0x10000
#define AOUT_CHAN_DUALMONO          0x20000

#define AOUT_CHAN_PHYSMASK          0xFFFF


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
    int                     i_nb_samples;
    mtime_t                 start_date, end_date;

    struct aout_buffer_t *  p_next;
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

/* From dec.c : */
#define aout_DecNew(a, b, c) __aout_DecNew(VLC_OBJECT(a), b, c)
VLC_EXPORT( aout_input_t *, __aout_DecNew, ( vlc_object_t *, aout_instance_t **, audio_sample_format_t * ) );
VLC_EXPORT( int, aout_DecDelete, ( aout_instance_t *, aout_input_t * ) );
VLC_EXPORT( aout_buffer_t *, aout_DecNewBuffer, ( aout_instance_t *, aout_input_t *, size_t ) );
VLC_EXPORT( void, aout_DecDeleteBuffer, ( aout_instance_t *, aout_input_t *, aout_buffer_t * ) );
VLC_EXPORT( int, aout_DecPlay, ( aout_instance_t *, aout_input_t *, aout_buffer_t * ) );

/* From intf.c : */
VLC_EXPORT( int, aout_VolumeGet, ( aout_instance_t *, audio_volume_t * ) );
VLC_EXPORT( int, aout_VolumeSet, ( aout_instance_t *, audio_volume_t ) );
VLC_EXPORT( int, aout_VolumeInfos, ( aout_instance_t *, audio_volume_t * ) );
VLC_EXPORT( int, aout_VolumeUp, ( aout_instance_t *, int, audio_volume_t * ) );
VLC_EXPORT( int, aout_VolumeDown, ( aout_instance_t *, int, audio_volume_t * ) );
VLC_EXPORT( int, aout_Restart, ( aout_instance_t * p_aout ) );
VLC_EXPORT( void, aout_FindAndRestart, ( vlc_object_t * p_this ) );
VLC_EXPORT( int, aout_ChannelsRestart, ( vlc_object_t *, const char *, vlc_value_t, vlc_value_t, void * ) );

