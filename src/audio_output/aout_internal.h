/*****************************************************************************
 * aout_internal.h : internal defines for audio output
 *****************************************************************************
 * Copyright (C) 2002 VLC authors and VideoLAN
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

#ifndef LIBVLC_AOUT_INTERNAL_H
# define LIBVLC_AOUT_INTERNAL_H 1

# include <stdatomic.h>

# include <vlc_atomic.h>
# include <vlc_filter.h>
# include <vlc_list.h>
# include <vlc_viewpoint.h>
# include "../clock/clock.h"

/* Max input rate factor (1/4 -> 4) */
# define AOUT_MAX_INPUT_RATE (4)

enum {
    AOUT_RESAMPLING_NONE=0,
    AOUT_RESAMPLING_UP,
    AOUT_RESAMPLING_DOWN
};

typedef struct aout_volume aout_volume_t;
typedef struct vlc_aout_stream vlc_aout_stream;

typedef struct
{
    vlc_mutex_t lock;
    module_t *module; /**< Output plugin (or NULL if inactive) */
    bool bitexact;

    vlc_aout_stream *main_stream;

    struct
    {
        vlc_mutex_t lock;
        struct vlc_list list;
        unsigned count;
    } dev;

    struct
    {
        atomic_bool update;
        vlc_mutex_t lock;
        vlc_viewpoint_t value;
    } vp;

    int requested_stereo_mode; /**< Requested stereo mode set by the user */
    int requested_mix_mode; /**< Requested mix mode set by the user */

    struct vlc_audio_meter meter;

    vlc_atomic_rc_t rc;
} aout_owner_t;

typedef struct
{
    audio_output_t output;
    aout_owner_t   owner;
} aout_instance_t;

static inline aout_instance_t *aout_instance (audio_output_t *aout)
{
    return container_of(aout, aout_instance_t, output);
}

static inline aout_owner_t *aout_owner (audio_output_t *aout)
{
    aout_instance_t *instance = aout_instance(aout);
    return &instance->owner;
}

/****************************************************************************
 * Prototypes
 *****************************************************************************/

/* From mixer.c : */
aout_volume_t *aout_volume_New(vlc_object_t *, const audio_replay_gain_t *);
#define aout_volume_New(o, g) aout_volume_New(VLC_OBJECT(o), g)
int aout_volume_SetFormat(aout_volume_t *, vlc_fourcc_t);
void aout_volume_SetVolume(aout_volume_t *, float);
int aout_volume_Amplify(aout_volume_t *, block_t *);
void aout_volume_Delete(aout_volume_t *);


/* From output.c : */
audio_output_t *aout_New (vlc_object_t *);
#define aout_New(a) aout_New(VLC_OBJECT(a))
void aout_Destroy (audio_output_t *);

int aout_OutputNew(audio_output_t *aout, vlc_aout_stream *stream,
                   audio_sample_format_t *fmt, int input_profile,
                   audio_sample_format_t *filter_fmt,
                   aout_filters_cfg_t *filters_cfg);
void aout_OutputDelete( audio_output_t * p_aout );

vlc_audio_meter_plugin *
aout_AddMeterPlugin(audio_output_t *aout, const char *chain,
                    const struct vlc_audio_meter_plugin_owner *owner);

void
aout_RemoveMeterPlugin(audio_output_t *aout, vlc_audio_meter_plugin *plugin);

/* From common.c : */
void aout_FormatsPrint(vlc_object_t *, const char *,
                       const audio_sample_format_t *,
                       const audio_sample_format_t *);
#define aout_FormatsPrint(o, t, a, b) \
        aout_FormatsPrint(VLC_OBJECT(o), t, a, b)

/* From dec.c */
#define AOUT_DEC_SUCCESS 0
#define AOUT_DEC_CHANGED 1
#define AOUT_DEC_FAILED VLC_EGENERIC

struct vlc_aout_stream_cfg
{
    const audio_sample_format_t *fmt;
    int profile;
    struct vlc_clock_t *clock;
    const char *str_id;
    const audio_replay_gain_t *replay_gain;
};

vlc_aout_stream *vlc_aout_stream_New(audio_output_t *p_aout,
                                     const struct vlc_aout_stream_cfg *cfg);
void vlc_aout_stream_Delete(vlc_aout_stream *);
int vlc_aout_stream_Play(vlc_aout_stream *stream, block_t *block);
void vlc_aout_stream_GetResetStats(vlc_aout_stream *stream, unsigned *, unsigned *);
void vlc_aout_stream_ChangePause(vlc_aout_stream *stream, bool b_paused, vlc_tick_t i_date);
void vlc_aout_stream_ChangeRate(vlc_aout_stream *stream, float rate);
void vlc_aout_stream_ChangeDelay(vlc_aout_stream *stream, vlc_tick_t delay);
void vlc_aout_stream_Flush(vlc_aout_stream *stream);
void vlc_aout_stream_Drain(vlc_aout_stream *stream);
/* Contrary to other vlc_aout_stream_*() functions, this function can be called from
 * any threads */
bool vlc_aout_stream_IsDrained(vlc_aout_stream *stream);
/* Called from output.c */
void vlc_aout_stream_NotifyDrained(vlc_aout_stream *stream);
void vlc_aout_stream_NotifyGain(vlc_aout_stream *stream, float gain);

void vlc_aout_stream_RequestRestart(vlc_aout_stream *stream, unsigned);

void aout_InputRequestRestart(audio_output_t *aout);

static inline void aout_SetWavePhysicalChannels(audio_sample_format_t *fmt)
{
    static const uint32_t wave_channels[] = {
        AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER,
        AOUT_CHAN_LFE, AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,
        AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT, AOUT_CHAN_REARCENTER };

    fmt->i_physical_channels = 0;
    for (int i = 0; i < fmt->i_channels && i < AOUT_CHAN_MAX; ++i)
        fmt->i_physical_channels |= wave_channels[i];
    aout_FormatPrepare(fmt);
}

/* From filters.c */

/* Extended version of aout_FiltersNew
 *
 * The clock, that is not mandatory, will be used to create a new slave clock
 * for the filter vizualisation plugins.
 */
aout_filters_t *aout_FiltersNewWithClock(vlc_object_t *, const vlc_clock_t *,
                                         const audio_sample_format_t *,
                                         const audio_sample_format_t *,
                                         const aout_filters_cfg_t *cfg) VLC_USED;
void aout_FiltersResetClock(aout_filters_t *filters);
void aout_FiltersSetClockDelay(aout_filters_t *filters, vlc_tick_t delay);
bool aout_FiltersCanResample (aout_filters_t *filters);
filter_t *aout_filter_Create(vlc_object_t *obj, const filter_owner_t *restrict owner,
                             const char *type, const char *name,
                             const audio_sample_format_t *infmt,
                             const audio_sample_format_t *outfmt,
                             config_chain_t *cfg, bool const_fmt);

#endif /* !LIBVLC_AOUT_INTERNAL_H */
