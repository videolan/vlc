/*****************************************************************************
 * aout_internal.h : internal defines for audio output
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
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

#ifndef LIBVLC_AOUT_INTERNAL_H
# define LIBVLC_AOUT_INTERNAL_H 1

/* Max input rate factor (1/4 -> 4) */
# define AOUT_MAX_INPUT_RATE (4)

enum {
    AOUT_RESAMPLING_NONE=0,
    AOUT_RESAMPLING_UP,
    AOUT_RESAMPLING_DOWN
};

# include <vlc_aout_mixer.h>

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

block_t *aout_FilterBufferNew( filter_t *, int );

/** an input stream for the audio output */
struct aout_input_t
{
    /* When this lock is taken, the pipeline cannot be changed by a
     * third-party. */
    vlc_mutex_t             lock;

    audio_sample_format_t   input;

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

    /* Mixer information */
    audio_replay_gain_t     replay_gain;

    /* If b_restart == 1, the input pipeline will be re-created. */
    bool              b_restart;

    /* If b_error == 1, there is no input pipeline. */
    bool              b_error;

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

    /* */
    aout_mixer_input_t mixer;
 };

/****************************************************************************
 * Prototypes
 *****************************************************************************/

/* From input.c : */
int aout_InputNew( audio_output_t * p_aout, aout_input_t * p_input, const aout_request_vout_t * );
int aout_InputDelete( audio_output_t * p_aout, aout_input_t * p_input );
void aout_InputPlay( audio_output_t * p_aout, aout_input_t * p_input,
                     aout_buffer_t * p_buffer, int i_input_rate );
void aout_InputCheckAndRestart( audio_output_t * p_aout, aout_input_t * p_input );

/* From filters.c : */
int aout_FiltersCreatePipeline( audio_output_t *, filter_t **, int *,
    const audio_sample_format_t *, const audio_sample_format_t * );
void aout_FiltersDestroyPipeline( filter_t *const *, unsigned );
void aout_FiltersPlay( filter_t *const *, unsigned, aout_buffer_t ** );

/* From mixer.c : */
int aout_MixerNew( audio_output_t * p_aout );
void aout_MixerDelete( audio_output_t * p_aout );
void aout_MixerRun( audio_output_t * p_aout, float );

/* From output.c : */
int aout_OutputNew( audio_output_t * p_aout,
                    const audio_sample_format_t * p_format );
void aout_OutputPlay( audio_output_t * p_aout, aout_buffer_t * p_buffer );
void aout_OutputPause( audio_output_t * p_aout, bool, mtime_t );
void aout_OutputDelete( audio_output_t * p_aout );


/* From common.c : */
/* Release with vlc_object_release() */
audio_output_t *aout_New ( vlc_object_t * );
#define aout_New(a) aout_New(VLC_OBJECT(a))

void aout_FifoInit( vlc_object_t *, aout_fifo_t *, uint32_t );
#define aout_FifoInit(o, f, r) aout_FifoInit(VLC_OBJECT(o), f, r)
mtime_t aout_FifoNextStart( const aout_fifo_t * ) VLC_USED;
void aout_FifoPush( aout_fifo_t *, aout_buffer_t * );
void aout_FifoSet( aout_fifo_t *, mtime_t );
void aout_FifoMoveDates( aout_fifo_t *, mtime_t );
void aout_FifoDestroy( aout_fifo_t * p_fifo );
void aout_FormatsPrint( audio_output_t * p_aout, const char * psz_text, const audio_sample_format_t * p_format1, const audio_sample_format_t * p_format2 );
bool aout_ChangeFilterString( vlc_object_t *, audio_output_t *, const char *psz_variable, const char *psz_name, bool b_add );

/* From dec.c */
aout_input_t *aout_DecNew( audio_output_t *, audio_sample_format_t *,
                   const audio_replay_gain_t *, const aout_request_vout_t * );
void aout_DecDelete ( audio_output_t *, aout_input_t * );
aout_buffer_t * aout_DecNewBuffer( aout_input_t *, size_t );
void aout_DecDeleteBuffer( audio_output_t *, aout_input_t *, aout_buffer_t * );
int aout_DecPlay( audio_output_t *, aout_input_t *, aout_buffer_t *, int i_input_rate );
int aout_DecGetResetLost( audio_output_t *, aout_input_t * );
void aout_DecChangePause( audio_output_t *, aout_input_t *, bool b_paused, mtime_t i_date );
void aout_DecFlush( audio_output_t *, aout_input_t * );
bool aout_DecIsEmpty( audio_output_t * p_aout, aout_input_t * p_input );

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
    vlc_mutex_lock( &p_aout->lock );
}

static inline void aout_unlock( audio_output_t *p_aout )
{
    aout_unlock_check( OUTPUT_LOCK );
    vlc_mutex_unlock( &p_aout->lock );
}

static inline void aout_lock_volume( audio_output_t *p_aout )
{
    aout_lock_check( VOLUME_LOCK );
    vlc_mutex_lock( &p_aout->volume_lock );
}

static inline void aout_unlock_volume( audio_output_t *p_aout )
{
    aout_unlock_check( VOLUME_LOCK );
    vlc_mutex_unlock( &p_aout->volume_lock );
}

/* Helpers */

/**
 * This function will safely mark aout input to be restarted as soon as
 * possible to take configuration changes into account */
static inline void AoutInputsMarkToRestart( audio_output_t *p_aout )
{
    aout_lock( p_aout );
    if( p_aout->p_input != NULL )
        p_aout->p_input->b_restart = true;
    aout_unlock( p_aout );
}

#endif /* !LIBVLC_AOUT_INTERNAL_H */
