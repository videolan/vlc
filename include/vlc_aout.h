/*****************************************************************************
 * audio_output.h : audio output interface
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_AOUT_H
#define VLC_AOUT_H 1

/**
 * \file
 * This file defines functions, structures and macros for audio output object
 */

# ifdef __cplusplus
extern "C" {
# endif

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
#   define AOUT_FMT_S32_NE VLC_FOURCC('s','3','2','b')
#   define AOUT_FMT_SPDIF_NE VLC_FOURCC('s','p','d','b')
#else
#   define AOUT_FMT_S16_NE VLC_FOURCC('s','1','6','l')
#   define AOUT_FMT_U16_NE VLC_FOURCC('u','1','6','l')
#   define AOUT_FMT_S24_NE VLC_FOURCC('s','2','4','l')
#   define AOUT_FMT_S32_NE VLC_FOURCC('s','3','2','l')
#   define AOUT_FMT_SPDIF_NE VLC_FOURCC('s','p','d','i')
#endif

#define AOUT_FMT_NON_LINEAR( p_format )                                    \
    ( ((p_format)->i_format == VLC_FOURCC('s','p','d','i'))                \
       || ((p_format)->i_format == VLC_FOURCC('s','p','d','b'))            \
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
 * Main audio output structures
 *****************************************************************************/

/** audio output buffer */
struct aout_buffer_t
{
    uint8_t *               p_buffer;
    int                     i_alloc_type;
    /* i_size is the real size of the buffer (used for debug ONLY), i_nb_bytes
     * is the number of significative bytes in it. */
    size_t                  i_size, i_nb_bytes;
    unsigned int            i_nb_samples;
    mtime_t                 start_date, end_date;
    bool                    b_discontinuity; /* Set on discontinuity (for non pcm stream) */

    struct aout_buffer_t *  p_next;

    /** Private data (aout_buffer_t will disappear soon so no need for an
     * aout_buffer_sys_t type) */
    void * p_sys;

    /** This way the release can be overloaded */
    void (*pf_release)( aout_buffer_t * );
};

#define aout_BufferFree( p_buffer ) do {                                    \
    if( p_buffer != NULL && (p_buffer)->i_alloc_type == AOUT_ALLOC_HEAP )   \
    {                                                                       \
        free( p_buffer );                                                   \
    }                                                                       \
    p_buffer = NULL; } while(0)

/* Size of a frame for S/PDIF output. */
#define AOUT_SPDIF_SIZE 6144

/* Number of samples in an A/52 frame. */
#define A52_FRAME_NB 1536

/* Max input rate factor (1/4 -> 4) */
#define AOUT_MAX_INPUT_RATE (4)

/** date incrementation helper structure without long-term
 * rounding errors
 */
struct audio_date_t
{
    mtime_t  date;
    uint32_t i_divider;
    uint32_t i_remainder;
};

/** allocation of memory in the audio output */
typedef struct aout_alloc_t
{
    int                     i_alloc_type;
    int                     i_bytes_per_sec;
} aout_alloc_t;

#define AOUT_ALLOC_NONE     0
#define AOUT_ALLOC_STACK    1
#define AOUT_ALLOC_HEAP     2

/** audio output mixer */
typedef struct aout_mixer_t
{
    audio_sample_format_t   mixer;
    aout_alloc_t            output_alloc;

    module_t *              p_module;
    struct aout_mixer_sys_t * p_sys;
    void                 (* pf_do_work)( struct aout_instance_t *,
                                         struct aout_buffer_t * );

    /** If b_error == 1, there is no mixer. */
    bool              b_error;
    /** Multiplier used to raise or lower the volume of the sound in
     * software. Beware, this creates sound distortion and should be avoided
     * as much as possible. This isn't available for non-float32 mixer. */
    float                   f_multiplier;
} aout_mixer_t;

/** audio output buffer FIFO */
struct aout_fifo_t
{
    aout_buffer_t *         p_first;
    aout_buffer_t **        pp_last;
    audio_date_t            end_date;
};

/* */
typedef struct
{
    vout_thread_t  *(*pf_request_vout)( void *,
                                        vout_thread_t *, video_format_t *, bool b_recycle );
    void *p_private;
} aout_request_vout_t;

/** audio output filter */
typedef struct aout_filter_owner_sys_t aout_filter_owner_sys_t;
typedef struct aout_filter_sys_t aout_filter_sys_t;
struct aout_filter_t
{
    VLC_COMMON_MEMBERS

    audio_sample_format_t   input;
    audio_sample_format_t   output;
    aout_alloc_t            output_alloc;

    module_t *              p_module;
    aout_filter_sys_t       *p_sys;

    bool                    b_in_place;
    bool                    b_continuity;

    void                    (*pf_do_work)( aout_instance_t *, aout_filter_t *,
                                           aout_buffer_t *, aout_buffer_t * );

    /* Owner fieldS
     * XXX You MUST not use them directly */

    /* Vout callback
     * XXX use aout_filter_RequestVout */
    aout_request_vout_t request_vout;

    /* Private structure for the owner of the filter */
    aout_filter_owner_sys_t *p_owner;
};

#define AOUT_RESAMPLING_NONE     0
#define AOUT_RESAMPLING_UP       1
#define AOUT_RESAMPLING_DOWN     2
/** an input stream for the audio output */
struct aout_input_t
{
    /* When this lock is taken, the pipeline cannot be changed by a
     * third-party. */
    vlc_mutex_t             lock;

    audio_sample_format_t   input;
    aout_alloc_t            input_alloc;

    /* pre-filters */
    aout_filter_t *         pp_filters[AOUT_MAX_FILTERS];
    int                     i_nb_filters;

    aout_filter_t *         p_playback_rate_filter;

    /* resamplers */
    aout_filter_t *         pp_resamplers[AOUT_MAX_FILTERS];
    int                     i_nb_resamplers;
    int                     i_resampling_type;
    mtime_t                 i_resamp_start_date;
    int                     i_resamp_start_drift;

    aout_fifo_t             fifo;

    /* Mixer information */
    uint8_t *               p_first_byte_to_mix;
    audio_replay_gain_t     replay_gain;
    float                   f_multiplier;

    /* If b_restart == 1, the input pipeline will be re-created. */
    bool              b_restart;

    /* If b_error == 1, there is no input pipeline. */
    bool              b_error;

    /* Did we just change the output format? (expect buffer inconsistencies) */
    bool              b_changed;

    /* last rate from input */
    int               i_last_input_rate;

    /* */
    int               i_buffer_lost;

    /* */
    bool              b_paused;
    mtime_t           i_pause_date;

    /* */
    bool                b_recycle_vout;
    aout_request_vout_t request_vout;
 };

/** an output stream for the audio output */
typedef struct aout_output_t
{
    audio_sample_format_t   output;
    /* Indicates whether the audio output is currently starving, to avoid
     * printing a 1,000 "output is starving" messages. */
    bool              b_starving;

    /* post-filters */
    aout_filter_t *         pp_filters[AOUT_MAX_FILTERS];
    int                     i_nb_filters;

    aout_fifo_t             fifo;

    struct module_t *       p_module;
    struct aout_sys_t *     p_sys;
    void                 (* pf_play)( aout_instance_t * );
    int                  (* pf_volume_get )( aout_instance_t *, audio_volume_t * );
    int                  (* pf_volume_set )( aout_instance_t *, audio_volume_t );
    int                  (* pf_volume_infos )( aout_instance_t *, audio_volume_t * );
    int                     i_nb_samples;

    /* Current volume for the output - it's just a placeholder, the plug-in
     * may or may not use it. */
    audio_volume_t          i_volume;

    /* If b_error == 1, there is no audio output pipeline. */
    bool              b_error;
} aout_output_t;

/** audio output thread descriptor */
struct aout_instance_t
{
    VLC_COMMON_MEMBERS

    /* Locks : please note that if you need several of these locks, it is
     * mandatory (to avoid deadlocks) to take them in the following order :
     * mixer_lock, p_input->lock, output_fifo_lock, input_fifos_lock.
     * --Meuuh */
    /* When input_fifos_lock is taken, none of the p_input->fifo structures
     * can be read or modified by a third-party thread. */
    vlc_mutex_t             input_fifos_lock;
    /* When mixer_lock is taken, all decoder threads willing to mix a
     * buffer must wait until it is released. The output pipeline cannot
     * be modified. No input stream can be added or removed. */
    vlc_mutex_t             mixer_lock;
    /* When output_fifo_lock is taken, the p_aout->output.fifo structure
     * cannot be read or written  by a third-party thread. */
    vlc_mutex_t             output_fifo_lock;

    /* Input streams & pre-filters */
    aout_input_t *          pp_inputs[AOUT_MAX_INPUTS];
    int                     i_nb_inputs;

    /* Mixer */
    aout_mixer_t            mixer;

    /* Output plug-in */
    aout_output_t           output;
};

/**
 * It describes the audio channel order VLC except.
 */
static const uint32_t pi_vlc_chan_order_wg4[] =
{
    AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT,
    AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT,
    AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, AOUT_CHAN_REARCENTER,
    AOUT_CHAN_CENTER, AOUT_CHAN_LFE, 0
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/* From common.c : */
VLC_EXPORT( void, aout_DateInit, ( audio_date_t *, uint32_t ) );
VLC_EXPORT( void, aout_DateSet, ( audio_date_t *, mtime_t ) );
VLC_EXPORT( void, aout_DateMove, ( audio_date_t *, mtime_t ) );
VLC_EXPORT( mtime_t, aout_DateGet, ( const audio_date_t * ) LIBVLC_USED);
VLC_EXPORT( mtime_t, aout_DateIncrement, ( audio_date_t *, uint32_t ) );

VLC_EXPORT( aout_buffer_t *, aout_OutputNextBuffer, ( aout_instance_t *, mtime_t, bool ) LIBVLC_USED );

/**
 * This function computes the reordering needed to go from pi_chan_order_in to
 * pi_chan_order_out.
 * If pi_chan_order_in or pi_chan_order_out is NULL, it will assume that vlc
 * internal (WG4) order is requested.
 */
VLC_EXPORT( int, aout_CheckChannelReorder, ( const uint32_t *pi_chan_order_in, const uint32_t *pi_chan_order_out, uint32_t i_channel_mask, int i_channels, int *pi_chan_table ) );
VLC_EXPORT( void, aout_ChannelReorder, ( uint8_t *, int, int, const int *, int ) );

/**
 * This fonction will compute the extraction parameter into pi_selection to go
 * from i_channels with their type given by pi_order_src[] into the order
 * describe by pi_order_dst.
 * It will also set :
 * - *pi_channels as the number of channels that will be extracted which is
 * lower (in case of non understood channels type) or equal to i_channels.
 * - the layout of the channels (*pi_layout).
 *
 * It will return true if channel extraction is really needed, in which case
 * aout_ChannelExtract must be used
 *
 * XXX It must be used when the source may have channel type not understood
 * by VLC. In this case the channel type pi_order_src[] must be set to 0.
 * XXX It must also be used if multiple channels have the same type.
 */
VLC_EXPORT( bool, aout_CheckChannelExtraction, ( int *pi_selection, uint32_t *pi_layout, int *pi_channels, const uint32_t pi_order_dst[AOUT_CHAN_MAX], const uint32_t *pi_order_src, int i_channels ) );

/**
 * Do the actual channels extraction using the parameters created by
 * aout_CheckChannelExtraction.
 *
 * XXX this function does not work in place (p_dst and p_src must not overlap).
 * XXX Only 8, 16, 24, 32, 64 bits per sample are supported.
 */
VLC_EXPORT( void, aout_ChannelExtract, ( void *p_dst, int i_dst_channels, const void *p_src, int i_src_channels, int i_sample_count, const int *pi_selection, int i_bits_per_sample ) );

/* */
VLC_EXPORT( unsigned int, aout_FormatNbChannels, ( const audio_sample_format_t * p_format ) LIBVLC_USED );
VLC_EXPORT( unsigned int, aout_BitsPerSample, ( vlc_fourcc_t i_format ) LIBVLC_USED );
VLC_EXPORT( void, aout_FormatPrepare, ( audio_sample_format_t * p_format ) );
VLC_EXPORT( void, aout_FormatPrint, ( aout_instance_t * p_aout, const char * psz_text, const audio_sample_format_t * p_format ) );
VLC_EXPORT( const char *, aout_FormatPrintChannels, ( const audio_sample_format_t * ) LIBVLC_USED );

VLC_EXPORT( mtime_t, aout_FifoFirstDate, ( aout_instance_t *, aout_fifo_t * ) LIBVLC_USED );
VLC_EXPORT( aout_buffer_t *, aout_FifoPop, ( aout_instance_t * p_aout, aout_fifo_t * p_fifo ) LIBVLC_USED );

/* From intf.c : */
VLC_EXPORT( void, aout_VolumeSoftInit, ( aout_instance_t * ) );
VLC_EXPORT( void, aout_VolumeNoneInit, ( aout_instance_t * ) );
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
VLC_EXPORT( int, aout_FindAndRestart, ( vlc_object_t *, const char *, vlc_value_t, vlc_value_t, void * ) );
VLC_EXPORT( int, aout_ChannelsRestart, ( vlc_object_t *, const char *, vlc_value_t, vlc_value_t, void * ) );

VLC_EXPORT( void, aout_EnableFilter, (vlc_object_t *, const char *, bool ));

#define aout_VisualNext(a) aout_VisualChange( VLC_OBJECT(a),1 )
#define aout_VisualPrev(a) aout_VisualChange( VLC_OBJECT(a),-1 )

VLC_EXPORT( char *, aout_VisualChange, (vlc_object_t *, int ) );

/* */
VLC_EXPORT( vout_thread_t *, aout_filter_RequestVout, ( aout_filter_t *, vout_thread_t *p_vout, video_format_t *p_fmt ) );

# ifdef __cplusplus
}
# endif

#endif /* _VLC_AOUT_H */
