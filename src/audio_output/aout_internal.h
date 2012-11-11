/*****************************************************************************
 * aout_internal.h : internal defines for audio output
 *****************************************************************************
 * Copyright (C) 2002 VLC authors and VideoLAN
 * $Id$
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

# include <vlc_atomic.h>

/* Max input rate factor (1/4 -> 4) */
# define AOUT_MAX_INPUT_RATE (4)

# define AOUT_MAX_FILTERS 10

enum {
    AOUT_RESAMPLING_NONE=0,
    AOUT_RESAMPLING_UP,
    AOUT_RESAMPLING_DOWN
};

typedef struct
{
    struct vout_thread_t  *(*pf_request_vout)( void *, struct vout_thread_t *,
                                               video_format_t *, bool );
    void *p_private;
} aout_request_vout_t;

typedef struct aout_volume aout_volume_t;

/** an input stream for the audio output */
struct aout_input_t
{
    int                     i_resampling_type;
    int                     i_resamp_start_drift;

    /* last rate from input */
    int               i_last_input_rate;

    /* */
    int               i_buffer_lost;
};

typedef struct
{
    vlc_mutex_t lock;
    module_t *module; /**< Output plugin (or NULL if inactive) */
    aout_input_t *input;
    aout_volume_t *volume;

    struct
    {
        date_t date;
    } sync;

    audio_sample_format_t input_format;
    audio_sample_format_t mixer_format;

    filter_t *rate_filter; /**< The filter adjusting samples count
        (either the scaletempo filter or a resampler) */
    filter_t *resampler; /**< The resampler */
    int resampling; /**< Current resampling (Hz) */
    unsigned nb_filters;
    filter_t *filters[AOUT_MAX_FILTERS]; /**< Configured user filters
        (e.g. equalization) and their conversions */
    unsigned nb_converters;
    filter_t *converters[5]; /**< Converters to the output */

    aout_request_vout_t request_vout;
    bool recycle_vout;

    vlc_atomic_t restart;
} aout_owner_t;

typedef struct
{
    audio_output_t output;
    aout_owner_t   owner;
} aout_instance_t;

static inline aout_owner_t *aout_owner (audio_output_t *aout)
{
    return &((aout_instance_t *)aout)->owner;
}

/****************************************************************************
 * Prototypes
 *****************************************************************************/

/* From input.c : */
aout_input_t *aout_InputNew(void);
void aout_InputDelete(aout_input_t *);
block_t *aout_InputPlay( audio_output_t *p_aout, aout_input_t *p_input,
                         block_t *p_buffer, int i_input_rate, date_t * );

/* From filters.c : */
int aout_FiltersPipelineCreate(vlc_object_t *, filter_t **, unsigned *,
    unsigned, const audio_sample_format_t *, const audio_sample_format_t *);
#define aout_FiltersPipelineCreate(o, pv, pc, max, inf, outf) \
        aout_FiltersPipelineCreate(VLC_OBJECT(o), pv, pc, max, inf, outf)
void aout_FiltersPipelineDestroy(filter_t *const *, unsigned);
block_t *aout_FiltersPipelinePlay(filter_t *const *, unsigned, block_t *);

int aout_FiltersNew(audio_output_t *, const audio_sample_format_t *,
                   const audio_sample_format_t *, const aout_request_vout_t *);
void aout_FiltersDelete(audio_output_t *);
bool aout_FiltersAdjustResampling(audio_output_t *, int);
block_t *aout_FiltersPlay(audio_output_t *, block_t *, int rate);

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

int aout_OutputNew( audio_output_t * p_aout,
                    const audio_sample_format_t * p_format );
void aout_OutputPlay( audio_output_t * p_aout, block_t * p_buffer );
void aout_OutputPause( audio_output_t * p_aout, bool, mtime_t );
void aout_OutputFlush( audio_output_t * p_aout, bool );
void aout_OutputDelete( audio_output_t * p_aout );


/* From common.c : */
void aout_FormatsPrint(vlc_object_t *, const char *,
                       const audio_sample_format_t *,
                       const audio_sample_format_t *);
#define aout_FormatsPrint(o, t, a, b) \
        aout_FormatsPrint(VLC_OBJECT(o), t, a, b)
bool aout_ChangeFilterString( vlc_object_t *manager, vlc_object_t *aout,
                              const char *var, const char *name, bool b_add );

/* From dec.c */
int aout_DecNew(audio_output_t *, const audio_sample_format_t *,
                const audio_replay_gain_t *, const aout_request_vout_t *);
void aout_DecDelete(audio_output_t *);
block_t *aout_DecNewBuffer(audio_output_t *, size_t);
void aout_DecDeleteBuffer(audio_output_t *, block_t *);
int aout_DecPlay(audio_output_t *, block_t *, int i_input_rate);
int aout_DecGetResetLost(audio_output_t *);
void aout_DecChangePause(audio_output_t *, bool b_paused, mtime_t i_date);
void aout_DecFlush(audio_output_t *);
bool aout_DecIsEmpty(audio_output_t *);

void aout_InputRequestRestart(audio_output_t *);

/* Audio output locking */
static inline void aout_lock( audio_output_t *p_aout )
{
    vlc_mutex_lock( &aout_owner(p_aout)->lock );
}

static inline void aout_unlock( audio_output_t *p_aout )
{
    vlc_mutex_unlock( &aout_owner(p_aout)->lock );
}

#define aout_assert_locked( aout ) \
        vlc_assert_locked( &aout_owner(aout)->lock )

#endif /* !LIBVLC_AOUT_INTERNAL_H */
