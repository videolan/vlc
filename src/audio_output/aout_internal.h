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

struct filter_owner_sys_t
{
    audio_output_t *p_aout;
    aout_input_t    *p_input;
};

/** an input stream for the audio output */
struct aout_input_t
{
    unsigned            samplerate; /**< Input sample rate */

    /* pre-filters */
    filter_t *              pp_filters[AOUT_MAX_FILTERS];
    int                     i_nb_filters;

    filter_t *              p_playback_rate_filter;

    /* resamplers */
    filter_t *              pp_resamplers[AOUT_MAX_FILTERS];
    int                     i_nb_resamplers;
    int                     i_resampling_type;
    mtime_t                 i_resamp_start_date;
    int                     i_resamp_start_drift;

    /* last rate from input */
    int               i_last_input_rate;

    /* */
    int               i_buffer_lost;

    /* */
    bool                b_recycle_vout;
    aout_request_vout_t request_vout;
};

typedef struct
{
    vlc_mutex_t lock;
    module_t *module; /**< Output plugin (or NULL if inactive) */
    aout_input_t *input;

    struct
    {
        date_t date;
    } sync;

    struct
    {
        vlc_mutex_t lock;
        float multiplier; /**< Software volume amplification multiplier */
        struct audio_mixer *mixer; /**< Software volume plugin */
    } volume;

    struct
    {
        vlc_atomic_t multiplier;
        audio_replay_gain_t data;
    } gain;

    audio_sample_format_t mixer_format;
    audio_sample_format_t input_format;

    /* Filters between mixer and output */
    filter_t *filters[AOUT_MAX_FILTERS];
    int       nb_filters;

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
aout_input_t *aout_InputNew(audio_output_t *, const audio_sample_format_t *,
                            const audio_sample_format_t *,
                            const aout_request_vout_t *);
int aout_InputDelete( audio_output_t * p_aout, aout_input_t * p_input );
block_t *aout_InputPlay( audio_output_t *p_aout, aout_input_t *p_input,
                         block_t *p_buffer, int i_input_rate, date_t * );

/* From filters.c : */
int aout_FiltersCreatePipeline( vlc_object_t *, filter_t **, int *,
    const audio_sample_format_t *, const audio_sample_format_t * );
#define aout_FiltersCreatePipeline(o, pv, pc, inf, outf) \
        aout_FiltersCreatePipeline(VLC_OBJECT(o), pv, pc, inf, outf)
void aout_FiltersDestroyPipeline( filter_t *const *, unsigned );
void aout_FiltersPlay( filter_t *const *, unsigned, block_t ** );

/* From mixer.c : */
#define aout_MixerNew(o, f) aout_MixerNew(VLC_OBJECT(o), f)

float aout_ReplayGainSelect(vlc_object_t *, const char *,
                            const audio_replay_gain_t *);
#define aout_ReplayGainSelect(o, s, g) \
        aout_ReplayGainSelect(VLC_OBJECT(o), s, g)

static inline void aout_ReplayGainInit(audio_replay_gain_t *restrict d,
                                       const audio_replay_gain_t *restrict s)
{
    if (s != NULL)
        *d = *s;
    else
        memset (d, 0, sizeof (*d));
}


/* From output.c : */
int aout_OutputNew( audio_output_t * p_aout,
                    const audio_sample_format_t * p_format );
void aout_OutputPlay( audio_output_t * p_aout, block_t * p_buffer );
void aout_OutputPause( audio_output_t * p_aout, bool, mtime_t );
void aout_OutputFlush( audio_output_t * p_aout, bool );
void aout_OutputDelete( audio_output_t * p_aout );


/* From common.c : */
audio_output_t *aout_New (vlc_object_t *);
#define aout_New(a) aout_New(VLC_OBJECT(a))
void aout_Destroy (audio_output_t *);

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
void aout_RequestRestart(audio_output_t *);
void aout_Shutdown (audio_output_t *);

/* Audio output locking */

#if !defined (NDEBUG) \
 && defined __linux__ && (defined (__i386__) || defined (__x86_64__))
# define AOUT_DEBUG 1
#endif

#ifdef AOUT_DEBUG
enum
{
    OUTPUT_LOCK=1,
    VOLUME_LOCK=2,
};

void aout_lock_check (unsigned);
void aout_unlock_check (unsigned);

#else
# define aout_lock_check( i )   (void)0
# define aout_unlock_check( i ) (void)0
#endif

static inline void aout_lock( audio_output_t *p_aout )
{
    aout_lock_check( OUTPUT_LOCK );
    vlc_mutex_lock( &aout_owner(p_aout)->lock );
}

static inline void aout_unlock( audio_output_t *p_aout )
{
    aout_unlock_check( OUTPUT_LOCK );
    vlc_mutex_unlock( &aout_owner(p_aout)->lock );
}

static inline void aout_lock_volume( audio_output_t *p_aout )
{
    aout_lock_check( VOLUME_LOCK );
    vlc_mutex_lock( &aout_owner(p_aout)->volume.lock );
}

static inline void aout_unlock_volume( audio_output_t *p_aout )
{
    aout_unlock_check( VOLUME_LOCK );
    vlc_mutex_unlock( &aout_owner(p_aout)->volume.lock );
}

#define aout_assert_locked( aout ) \
        vlc_assert_locked( &aout_owner(aout)->lock )

#endif /* !LIBVLC_AOUT_INTERNAL_H */
