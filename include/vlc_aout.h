/*****************************************************************************
 * vlc_aout.h : audio output interface
 *****************************************************************************
 * Copyright (C) 2002-2011 VLC authors and VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

#ifndef VLC_AOUT_H
#define VLC_AOUT_H 1

/**
 * \file
 * This file defines functions, structures and macros for audio output object
 */

/* Buffers which arrive in advance of more than AOUT_MAX_ADVANCE_TIME
 * will be considered as bogus and be trashed */
#define AOUT_MAX_ADVANCE_TIME           (AOUT_MAX_PREPARE_TIME + CLOCK_FREQ)

/* Buffers which arrive in advance of more than AOUT_MAX_PREPARE_TIME
 * will cause the calling thread to sleep */
#define AOUT_MAX_PREPARE_TIME           (2 * CLOCK_FREQ)

/* Buffers which arrive after pts - AOUT_MIN_PREPARE_TIME will be trashed
 * to avoid too heavy resampling */
#define AOUT_MIN_PREPARE_TIME           AOUT_MAX_PTS_ADVANCE

/* Tolerance values from EBU Recommendation 37 */
/** Maximum advance of actual audio playback time to coded PTS,
 * above which downsampling will be performed */
#define AOUT_MAX_PTS_ADVANCE            (CLOCK_FREQ / 25)

/** Maximum delay of actual audio playback time from coded PTS,
 * above which upsampling will be performed */
#define AOUT_MAX_PTS_DELAY              (3 * CLOCK_FREQ / 50)

/* Max acceptable resampling (in %) */
#define AOUT_MAX_RESAMPLING             10

#include "vlc_es.h"

#define AOUT_FMTS_IDENTICAL( p_first, p_second ) (                          \
    ((p_first)->i_format == (p_second)->i_format)                           \
      && AOUT_FMTS_SIMILAR(p_first, p_second) )

/* Check if i_rate == i_rate and i_channels == i_channels */
#define AOUT_FMTS_SIMILAR( p_first, p_second ) (                            \
    ((p_first)->i_rate == (p_second)->i_rate)                               \
      && ((p_first)->i_physical_channels == (p_second)->i_physical_channels)\
      && ((p_first)->i_original_channels == (p_second)->i_original_channels) )

#define AOUT_FMT_LINEAR( p_format ) \
    (aout_BitsPerSample((p_format)->i_format) != 0)

#define VLC_CODEC_SPDIFL VLC_FOURCC('s','p','d','i')
#define VLC_CODEC_SPDIFB VLC_FOURCC('s','p','d','b')

#define AOUT_FMT_SPDIF( p_format ) \
    ( ((p_format)->i_format == VLC_CODEC_SPDIFL)       \
       || ((p_format)->i_format == VLC_CODEC_SPDIFB)   \
       || ((p_format)->i_format == VLC_CODEC_A52)       \
       || ((p_format)->i_format == VLC_CODEC_DTS) )

/* Values used for the audio-channels object variable */
#define AOUT_VAR_CHAN_UNSET         0 /* must be zero */
#define AOUT_VAR_CHAN_STEREO        1
#define AOUT_VAR_CHAN_RSTEREO       2
#define AOUT_VAR_CHAN_LEFT          3
#define AOUT_VAR_CHAN_RIGHT         4
#define AOUT_VAR_CHAN_DOLBYS        5

/*****************************************************************************
 * Main audio output structures
 *****************************************************************************/

/* Size of a frame for S/PDIF output. */
#define AOUT_SPDIF_SIZE 6144

/* Number of samples in an A/52 frame. */
#define A52_FRAME_NB 1536

/* FIXME to remove once aout.h is cleaned a bit more */
#include <vlc_block.h>

/** Audio output object */
struct audio_output
{
    VLC_COMMON_MEMBERS

    struct aout_sys_t *sys; /**< Private data for callbacks */

    int (*start)(audio_output_t *, audio_sample_format_t *fmt);
    /**< Starts a new stream (mandatory, cannot be NULL).
      * \param fmt input stream sample format upon entry,
      *            output stream sample format upon return [IN/OUT]
      * \return VLC_SUCCESS on success, non-zero on failure
      * \note No other stream may be already started when called.
      */
    void (*stop)(audio_output_t *);
    /**< Stops the existing stream (optional, may be NULL).
      * \note A stream must have been started when called.
      */
    int (*time_get)(audio_output_t *, mtime_t *delay);
    /**< Estimates playback buffer latency (optional, may be NULL).
      * \param delay pointer to the delay until the next sample to be written
      *              to the playback buffer is rendered [OUT]
      * \return 0 on success, non-zero on failure or lack of data
      * \note A stream must have been started when called.
      */
    void (*play)(audio_output_t *, block_t *);
    /**< Queues a block of samples for playback (mandatory, cannot be NULL).
      * \note A stream must have been started when called.
      */
    void (*pause)( audio_output_t *, bool pause, mtime_t date);
    /**< Pauses or resumes playback (optional, may be NULL).
      * \param pause pause if true, resume from pause if false
      * \param date timestamp when the pause or resume was requested
      * \note A stream must have been started when called.
      */
    void (*flush)( audio_output_t *, bool wait);
    /**< Flushes or drains the playback buffers (mandatory, cannot be NULL).
      * \param wait true to wait for playback of pending buffers (drain),
      *             false to discard pending buffers (flush)
      * \note A stream must have been started when called.
      */
    int (*volume_set)(audio_output_t *, float volume);
    /**< Changes playback volume (optional, may be NULL).
      * \param volume requested volume (0. = mute, 1. = nominal)
      * \note The volume is always a positive number.
      * \warning A stream may or may not have been started when called.
      */
    int (*mute_set)(audio_output_t *, bool mute);
    /**< Changes muting (optinal, may be NULL).
      * \param mute true to mute, false to unmute
      * \warning A stream may or may not have been started when called.
      */
    int (*device_select)(audio_output_t *, const char *id);
    /**< Selects an audio output device (optional, may be NULL).
      * \param id nul-terminated device unique identifier.
      * \return 0 on success, non-zero on failure.
      * \warning A stream may or may not have been started when called.
      */
    struct {
        void (*volume_report)(audio_output_t *, float);
        void (*mute_report)(audio_output_t *, bool);
        void (*policy_report)(audio_output_t *, bool);
        void (*device_report)(audio_output_t *, const char *);
        void (*hotplug_report)(audio_output_t *, const char *, const char *);
        int (*gain_request)(audio_output_t *, float);
        void (*restart_request)(audio_output_t *, unsigned);
    } event;
};

/**
 * It describes the audio channel order VLC expect.
 */
static const uint32_t pi_vlc_chan_order_wg4[] =
{
    AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT,
    AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT,
    AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, AOUT_CHAN_REARCENTER,
    AOUT_CHAN_CENTER, AOUT_CHAN_LFE, 0
};

#define AOUT_RESTART_FILTERS 1
#define AOUT_RESTART_OUTPUT  2
#define AOUT_RESTART_DECODER 4

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/**
 * This function computes the reordering needed to go from pi_chan_order_in to
 * pi_chan_order_out.
 * If pi_chan_order_in or pi_chan_order_out is NULL, it will assume that vlc
 * internal (WG4) order is requested.
 */
VLC_API unsigned aout_CheckChannelReorder( const uint32_t *, const uint32_t *,
                                           uint32_t mask, uint8_t *table );
VLC_API void aout_ChannelReorder(void *, size_t, unsigned, const uint8_t *, vlc_fourcc_t);

VLC_API void aout_Interleave(void *dst, const void *const *planes,
                             unsigned samples, unsigned channels,
                             vlc_fourcc_t fourcc);
VLC_API void aout_Deinterleave(void *dst, const void *src, unsigned samples,
                             unsigned channels, vlc_fourcc_t fourcc);

/**
 * This function will compute the extraction parameter into pi_selection to go
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
VLC_API bool aout_CheckChannelExtraction( int *pi_selection, uint32_t *pi_layout, int *pi_channels, const uint32_t pi_order_dst[AOUT_CHAN_MAX], const uint32_t *pi_order_src, int i_channels );

/**
 * Do the actual channels extraction using the parameters created by
 * aout_CheckChannelExtraction.
 *
 * XXX this function does not work in place (p_dst and p_src must not overlap).
 * XXX Only 8, 16, 24, 32, 64 bits per sample are supported.
 */
VLC_API void aout_ChannelExtract( void *p_dst, int i_dst_channels, const void *p_src, int i_src_channels, int i_sample_count, const int *pi_selection, int i_bits_per_sample );

/* */
static inline unsigned aout_FormatNbChannels(const audio_sample_format_t *fmt)
{
    return popcount(fmt->i_physical_channels);
}

VLC_API unsigned int aout_BitsPerSample( vlc_fourcc_t i_format ) VLC_USED;
VLC_API void aout_FormatPrepare( audio_sample_format_t * p_format );
VLC_API void aout_FormatPrint(vlc_object_t *, const char *,
                              const audio_sample_format_t *);
#define aout_FormatPrint(o, t, f) aout_FormatPrint(VLC_OBJECT(o), t, f)
VLC_API const char * aout_FormatPrintChannels( const audio_sample_format_t * ) VLC_USED;

VLC_API float aout_VolumeGet (audio_output_t *);
VLC_API int aout_VolumeSet (audio_output_t *, float);
VLC_API int aout_MuteGet (audio_output_t *);
VLC_API int aout_MuteSet (audio_output_t *, bool);
VLC_API char *aout_DeviceGet (audio_output_t *);
VLC_API int aout_DeviceSet (audio_output_t *, const char *);
VLC_API int aout_DevicesList (audio_output_t *, char ***, char ***);

/**
 * Report change of configured audio volume to the core and UI.
 */
static inline void aout_VolumeReport(audio_output_t *aout, float volume)
{
    aout->event.volume_report(aout, volume);
}

/**
 * Report change of muted flag to the core and UI.
 */
static inline void aout_MuteReport(audio_output_t *aout, bool mute)
{
    aout->event.mute_report(aout, mute);
}

/**
 * Report audio policy status.
 * \parm cork true to request a cork, false to undo any pending cork.
 */
static inline void aout_PolicyReport(audio_output_t *aout, bool cork)
{
    aout->event.policy_report(aout, cork);
}

/**
 * Report change of output device.
 */
static inline void aout_DeviceReport(audio_output_t *aout, const char *id)
{
    aout->event.device_report(aout, id);
}

/**
 * Report a device hot-plug event.
 * @param id device ID
 * @param name human-readable device name (NULL for hot unplug)
 */
static inline void aout_HotplugReport(audio_output_t *aout,
                                      const char *id, const char *name)
{
    aout->event.hotplug_report(aout, id, name);
}

/**
 * Request a change of software audio amplification.
 * \param gain linear amplitude gain (must be positive)
 * \warning Values in excess 1.0 may cause overflow and distorsion.
 */
static inline int aout_GainRequest(audio_output_t *aout, float gain)
{
    return aout->event.gain_request(aout, gain);
}

static inline void aout_RestartRequest(audio_output_t *aout, unsigned mode)
{
    aout->event.restart_request(aout, mode);
}

static inline int aout_ChannelsRestart (vlc_object_t *obj, const char *varname,
                            vlc_value_t oldval, vlc_value_t newval, void *data)
{
    audio_output_t *aout = (audio_output_t *)obj;
    (void)varname; (void)oldval; (void)newval; (void)data;

    aout_RestartRequest (aout, AOUT_RESTART_OUTPUT);
    return 0;
}

/* Audio output filters */
typedef struct aout_filters aout_filters_t;
typedef struct aout_request_vout aout_request_vout_t;

VLC_API aout_filters_t *aout_FiltersNew(vlc_object_t *,
                                        const audio_sample_format_t *,
                                        const audio_sample_format_t *,
                                        const aout_request_vout_t *) VLC_USED;
#define aout_FiltersNew(o,inf,outf,rv) \
        aout_FiltersNew(VLC_OBJECT(o),inf,outf,rv)
VLC_API void aout_FiltersDelete(vlc_object_t *, aout_filters_t *);
#define aout_FiltersDelete(o,f) \
        aout_FiltersDelete(VLC_OBJECT(o),f)
VLC_API bool aout_FiltersAdjustResampling(aout_filters_t *, int);
VLC_API block_t *aout_FiltersPlay(aout_filters_t *, block_t *, int rate);

VLC_API vout_thread_t * aout_filter_RequestVout( filter_t *, vout_thread_t *p_vout, video_format_t *p_fmt );

#endif /* VLC_AOUT_H */
