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

#include <assert.h>

/**
 * \defgroup audio_output Audio output
 * \ingroup output
 * @{
 * \file
 * Audio output modules interface
 */

/* Buffers which arrive in advance of more than AOUT_MAX_ADVANCE_TIME
 * will be considered as bogus and be trashed */
#define AOUT_MAX_ADVANCE_TIME           (AOUT_MAX_PREPARE_TIME + VLC_TICK_FROM_SEC(1))

/* Buffers which arrive in advance of more than AOUT_MAX_PREPARE_TIME
 * will cause the calling thread to sleep */
#define AOUT_MAX_PREPARE_TIME           VLC_TICK_FROM_SEC(2)

/* Buffers which arrive after pts - AOUT_MIN_PREPARE_TIME will be trashed
 * to avoid too heavy resampling */
#define AOUT_MIN_PREPARE_TIME           AOUT_MAX_PTS_ADVANCE

/* Tolerance values from EBU Recommendation 37 */
/** Maximum advance of actual audio playback time to coded PTS,
 * above which downsampling will be performed */
#define AOUT_MAX_PTS_ADVANCE            VLC_TICK_FROM_MS(40)

/** Maximum delay of actual audio playback time from coded PTS,
 * above which upsampling will be performed */
#define AOUT_MAX_PTS_DELAY              VLC_TICK_FROM_MS(60)

/* Max acceptable resampling (in %) */
#define AOUT_MAX_RESAMPLING             10

#include "vlc_es.h"

#define AOUT_FMTS_IDENTICAL( p_first, p_second ) (                          \
    ((p_first)->i_format == (p_second)->i_format)                           \
      && AOUT_FMTS_SIMILAR(p_first, p_second) )

/* Check if i_rate == i_rate and i_channels == i_channels */
#define AOUT_FMTS_SIMILAR( p_first, p_second ) (                            \
    ((p_first)->i_rate == (p_second)->i_rate)                               \
      && ((p_first)->channel_type == (p_second)->channel_type)            \
      && ((p_first)->i_physical_channels == (p_second)->i_physical_channels)\
      && ((p_first)->i_chan_mode == (p_second)->i_chan_mode) )

#define AOUT_FMT_LINEAR( p_format ) \
    (aout_BitsPerSample((p_format)->i_format) != 0)

#define VLC_CODEC_SPDIFL VLC_FOURCC('s','p','d','i')
#define VLC_CODEC_SPDIFB VLC_FOURCC('s','p','d','b')

#define AOUT_FMT_SPDIF( p_format ) \
    ( ((p_format)->i_format == VLC_CODEC_SPDIFL)       \
       || ((p_format)->i_format == VLC_CODEC_SPDIFB)   \
       || ((p_format)->i_format == VLC_CODEC_A52)      \
       || ((p_format)->i_format == VLC_CODEC_DTS) )

#define AOUT_FMT_HDMI( p_format )                   \
    ( (p_format)->i_format == VLC_CODEC_EAC3        \
    ||(p_format)->i_format == VLC_CODEC_DTSHD       \
    ||(p_format)->i_format == VLC_CODEC_TRUEHD      \
    ||(p_format)->i_format == VLC_CODEC_MLP         \
    )

/* Values used for the audio-channels object variable */
#define AOUT_VAR_CHAN_UNSET         0 /* must be zero */
#define AOUT_VAR_CHAN_STEREO        1
#define AOUT_VAR_CHAN_RSTEREO       2
#define AOUT_VAR_CHAN_LEFT          3
#define AOUT_VAR_CHAN_RIGHT         4
#define AOUT_VAR_CHAN_DOLBYS        5
#define AOUT_VAR_CHAN_HEADPHONES    6
#define AOUT_VAR_CHAN_MONO          7

/*****************************************************************************
 * Main audio output structures
 *****************************************************************************/

/* Size of a frame for S/PDIF output. */
#define AOUT_SPDIF_SIZE 6144

/* Number of samples in an A/52 frame. */
#define A52_FRAME_NB 1536

/* FIXME to remove once aout.h is cleaned a bit more */
#include <vlc_block.h>

struct vlc_audio_output_events {
    void (*timing_report)(audio_output_t *, vlc_tick_t system_now, vlc_tick_t pts);
    void (*volume_report)(audio_output_t *, float);
    void (*mute_report)(audio_output_t *, bool);
    void (*policy_report)(audio_output_t *, bool);
    void (*device_report)(audio_output_t *, const char *);
    void (*hotplug_report)(audio_output_t *, const char *, const char *);
    void (*restart_request)(audio_output_t *, unsigned);
    int (*gain_request)(audio_output_t *, float);
};

/** Audio output object
 *
 * The audio output object is the abstraction for rendering decoded
 * (or pass-through) audio samples. In addition to playing samples,
 * the abstraction exposes controls for pause/resume, flush/drain,
 * changing the volume or mut flag, and listing and changing output device.
 *
 * An audio output can be in one of three different states:
 * stopped, playing or paused.
 * The audio output is always created in stopped state and is always destroyed
 * in that state also. It is moved from stopped to playing state by start(),
 * and from playing or paused states back to stopped state by stop().
 **/
struct audio_output
{
    struct vlc_object_t obj;

    void *sys; /**< Private data for callbacks */

    int (*start)(audio_output_t *, audio_sample_format_t *fmt);
    /**< Starts a new stream (mandatory, cannot be NULL).
      *
      * This callback changes the audio output from stopped to playing state
      * (if succesful). After the callback returns, time_get(), play(),
      * pause(), flush() and eventually stop() callbacks may be called.
      *
      * \param fmt input stream sample format upon entry,
      *            output stream sample format upon return [IN/OUT]
      * \return VLC_SUCCESS on success, non-zero on failure
      *
      * \note This callback can only be called while the audio output is in
      * stopped state. There can be only one stream per audio output at a time.
      *
      * \note This callbacks needs not be reentrant.
      */

    void (*stop)(audio_output_t *);
    /**< Stops the existing stream (mandatory, cannot be NULL).
      *
      * This callback terminates the current audio stream,
      * and returns the audio output to stopped state.
      *
      * \note This callback needs not be reentrant.
      */

    int (*time_get)(audio_output_t *, vlc_tick_t *delay);
    /**< Estimates playback buffer latency (mandatory, cannot be NULL).
      *
      * This callback computes an estimation of the delay until the current
      * tail of the audio output buffer would be rendered. This is essential
      * for (lip) synchronization and long term drift between the audio output
      * clock and the media upstream clock (if any).
      *
      * If the audio output clock is exactly synchronized with the system
      * monotonic clock (i.e. vlc_tick_now()), then aout_TimeGetDefault() can
      * implement this callback. In that case, drain must be implemented (since
      * the default implementation uses the delay to wait for the end of the
      * stream).
      *
      * This callback is called before the first play() in order to get the
      * initial delay (the hw latency). Most modules won't be able to know this
      * latency before the first play. In that case, they should return -1 and
      * handle the first play() date, cf. play() documentation.
      *
      * \param delay pointer to the delay until the next sample to be written
      *              to the playback buffer is rendered [OUT]
      * \return 0 on success, non-zero on failure or lack of data
      *
      * \note This callback cannot be called in stopped state.
      */

    void (*play)(audio_output_t *, block_t *block, vlc_tick_t date);
    /**< Queues a block of samples for playback (mandatory, cannot be NULL).
      *
      * The first play() date (after a flush()/start()) will be most likely in
      * the future. Modules that don't know the hw latency before a first play
      * (when they return -1 from the first time_get()) will need to handle
      * this. They can play a silence buffer with 'length = date - now()', or
      * configure their render callback to start at the given date.
      *
      * \param block block of audio samples
      * \param date intended system time to render the first sample
      *
      * \note This callback cannot be called in stopped state.
      */

    void (*pause)( audio_output_t *, bool pause, vlc_tick_t date);
    /**< Pauses or resumes playback (mandatory, cannot be NULL).
      *
      * This callback pauses or resumes audio playback as quickly as possible.
      * When pausing, it is desirable to stop producing sound immediately, but
      * retain already queued audio samples in the buffer to play when later
      * when resuming.
      *
      * If pausing is impossible, then aout_PauseDefault() can provide a
      * fallback implementation of this callback.
      *
      * \param pause pause if true, resume from pause if false
      * \param date timestamp when the pause or resume was requested
      *
      * \note This callback cannot be called in stopped state.
      */

    void (*flush)( audio_output_t *);
    /**< Flushes the playback buffers (mandatory, cannot be NULL).
      *
      * \param wait true to wait for playback of pending buffers (drain),
      *             false to discard pending buffers (flush)
      *
      * \note This callback cannot be called in stopped state.
      */
    void (*drain)(audio_output_t *);
    /**< Drain the playback buffers (can be NULL).
      *
      * If NULL, the caller will wait for the delay returned by time_get before
      * calling stop().
      */

    int (*volume_set)(audio_output_t *, float volume);
    /**< Changes playback volume (optional, may be NULL).
      *
      * \param volume requested volume (0. = mute, 1. = nominal)
      *
      * \note The volume is always a positive number.
      *
      * \warning A stream may or may not have been started when called.
      * \warning This callback may be called concurrently with
      * time_get(), play(), pause() or flush().
      * It will however be protected against concurrent calls to
      * start(), stop(), volume_set(), mute_set() or device_select().
      */

    int (*mute_set)(audio_output_t *, bool mute);
    /**< Changes muting (optinal, may be NULL).
      *
      * \param mute true to mute, false to unmute
      * \warning The same constraints apply as with volume_set().
      */

    int (*device_select)(audio_output_t *, const char *id);
    /**< Selects an audio output device (optional, may be NULL).
      *
      * \param id nul-terminated device unique identifier.
      * \return 0 on success, non-zero on failure.
      *
      * \warning The same constraints apply as with volume_set().
      */

    struct {
        bool headphones; /**< Default to false, set it to true if the current
                              sink is using headphones */
    } current_sink_info;
    /**< Current sink informations set by the module from the start() function */

    const struct vlc_audio_output_events *events;
};

typedef enum
{
    AOUT_CHANIDX_DISABLE = -1,
    AOUT_CHANIDX_LEFT,
    AOUT_CHANIDX_RIGHT,
    AOUT_CHANIDX_MIDDLELEFT,
    AOUT_CHANIDX_MIDDLERIGHT,
    AOUT_CHANIDX_REARLEFT,
    AOUT_CHANIDX_REARRIGHT,
    AOUT_CHANIDX_REARCENTER,
    AOUT_CHANIDX_CENTER,
    AOUT_CHANIDX_LFE,
    AOUT_CHANIDX_MAX
} vlc_chan_order_idx_t;

static_assert(AOUT_CHANIDX_MAX == AOUT_CHAN_MAX, "channel count mismatch");

#define AOUT_CHAN_REMAP_INIT { \
    AOUT_CHANIDX_LEFT,  \
    AOUT_CHANIDX_RIGHT, \
    AOUT_CHANIDX_MIDDLELEFT, \
    AOUT_CHANIDX_MIDDLERIGHT, \
    AOUT_CHANIDX_REARLEFT, \
    AOUT_CHANIDX_REARRIGHT, \
    AOUT_CHANIDX_REARCENTER, \
    AOUT_CHANIDX_CENTER, \
    AOUT_CHANIDX_LFE, \
}

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

#define AOUT_RESTART_FILTERS        0x1
#define AOUT_RESTART_OUTPUT         (AOUT_RESTART_FILTERS|0x2)
#define AOUT_RESTART_STEREOMODE     (AOUT_RESTART_OUTPUT|0x4)

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
VLC_API void aout_ChannelReorder(void *, size_t, uint8_t, const uint8_t *, vlc_fourcc_t);

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
 * XXX Only 8, 16, 32, 64 bits per sample are supported.
 */
VLC_API void aout_ChannelExtract( void *p_dst, int i_dst_channels, const void *p_src, int i_src_channels, int i_sample_count, const int *pi_selection, int i_bits_per_sample );

/* */
static inline unsigned aout_FormatNbChannels(const audio_sample_format_t *fmt)
{
    return vlc_popcount(fmt->i_physical_channels);
}

VLC_API unsigned int aout_BitsPerSample( vlc_fourcc_t i_format ) VLC_USED;
VLC_API void aout_FormatPrepare( audio_sample_format_t * p_format );
VLC_API void aout_FormatPrint(vlc_object_t *, const char *,
                              const audio_sample_format_t *);
#define aout_FormatPrint(o, t, f) aout_FormatPrint(VLC_OBJECT(o), t, f)
VLC_API const char * aout_FormatPrintChannels( const audio_sample_format_t * ) VLC_USED;

#define AOUT_VOLUME_DEFAULT             256
#define AOUT_VOLUME_MAX                 512

VLC_API float aout_VolumeGet (audio_output_t *);
VLC_API int aout_VolumeSet (audio_output_t *, float);
VLC_API int aout_VolumeUpdate (audio_output_t *, int, float *);
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
    aout->events->volume_report(aout, volume);
}

/**
 * Report change of muted flag to the core and UI.
 */
static inline void aout_MuteReport(audio_output_t *aout, bool mute)
{
    aout->events->mute_report(aout, mute);
}

/**
 * Report audio policy status.
 * \param cork true to request a cork, false to undo any pending cork.
 */
static inline void aout_PolicyReport(audio_output_t *aout, bool cork)
{
    aout->events->policy_report(aout, cork);
}

/**
 * Report change of output device.
 */
static inline void aout_DeviceReport(audio_output_t *aout, const char *id)
{
    aout->events->device_report(aout, id);
}

/**
 * Report a device hot-plug event.
 * @param id device ID
 * @param name human-readable device name (NULL for hot unplug)
 */
static inline void aout_HotplugReport(audio_output_t *aout,
                                      const char *id, const char *name)
{
    aout->events->hotplug_report(aout, id, name);
}

/**
 * Request a change of software audio amplification.
 * \param gain linear amplitude gain (must be positive)
 * \warning Values in excess 1.0 may cause overflow and distorsion.
 */
static inline int aout_GainRequest(audio_output_t *aout, float gain)
{
    return aout->events->gain_request(aout, gain);
}

static inline void aout_RestartRequest(audio_output_t *aout, unsigned mode)
{
    aout->events->restart_request(aout, mode);
}

/**
 * Default implementation for audio_output_t.time_get
 */
static inline int aout_TimeGetDefault(audio_output_t *aout,
                                      vlc_tick_t *restrict delay)
{
    (void) aout; (void) delay;
    return -1;
}

/**
 * Default implementation for audio_output_t.pause
 *
 * \warning This default callback implementation is suboptimal as it will
 * discard some audio samples.
 * Do not use this unless there are really no possible better alternatives.
 */
static inline void aout_PauseDefault(audio_output_t *aout, bool paused,
                                     vlc_tick_t date)
{
    if (paused)
        aout->flush(aout);
    (void) date;
}

/* Audio output filters */

/**
 * Enable or disable an audio filter ("audio-filter")
 *
 * \param aout a valid audio output
 * \param name a valid filter name
 * \param add true to add the filter, false to remove it
 * \return 0 on success, non-zero on failure.
 */
VLC_API int aout_EnableFilter(audio_output_t *aout, const char *name, bool add);

typedef struct
{
    /**
     * If the remap order differs from the WG4 order, a remap audio filter will
     * be inserted to remap channels according to this array.
     */
    int remap[AOUT_CHANIDX_MAX];
    /**
     * If true, a filter will be inserted to add a headphones effect (like a
     * binauralizer audio filter).
     */
    bool headphones;
} aout_filters_cfg_t;

#define AOUT_FILTERS_CFG_INIT (aout_filters_cfg_t) \
    { .remap = AOUT_CHAN_REMAP_INIT, \
      .headphones = false, \
    };

typedef struct aout_filters aout_filters_t;

VLC_API aout_filters_t *aout_FiltersNew(vlc_object_t *,
                                        const audio_sample_format_t *,
                                        const audio_sample_format_t *,
                                        const aout_filters_cfg_t *cfg) VLC_USED;
#define aout_FiltersNew(o,inf,outf,remap) \
        aout_FiltersNew(VLC_OBJECT(o),inf,outf,remap)
VLC_API void aout_FiltersDelete(vlc_object_t *, aout_filters_t *);
#define aout_FiltersDelete(o,f) \
        aout_FiltersDelete(VLC_OBJECT(o),f)
VLC_API bool aout_FiltersAdjustResampling(aout_filters_t *, int);
VLC_API block_t *aout_FiltersPlay(aout_filters_t *, block_t *, float rate);
VLC_API block_t *aout_FiltersDrain(aout_filters_t *);
VLC_API void     aout_FiltersFlush(aout_filters_t *);
VLC_API void     aout_FiltersChangeViewpoint(aout_filters_t *, const vlc_viewpoint_t *vp);

VLC_API vout_thread_t *aout_filter_GetVout(filter_t *, const video_format_t *);

/** @} */

#endif /* VLC_AOUT_H */
