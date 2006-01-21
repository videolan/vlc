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

/*****************************************************************************
 * aout_alloc_t : allocation of memory in the audio output
 *****************************************************************************/
typedef struct aout_alloc_t
{
    int                     i_alloc_type;
    int                     i_bytes_per_sec;
} aout_alloc_t;

#define AOUT_ALLOC_NONE     0
#define AOUT_ALLOC_STACK    1
#define AOUT_ALLOC_HEAP     2

#ifdef HAVE_ALLOCA
#   define ALLOCA_TEST( p_alloc, p_new_buffer )                             \
        if ( (p_alloc)->i_alloc_type == AOUT_ALLOC_STACK )                  \
        {                                                                   \
            (p_new_buffer) = alloca( i_alloc_size + sizeof(aout_buffer_t) );\
            i_alloc_type = AOUT_ALLOC_STACK;                                \
        }                                                                   \
        else
#else
#   define ALLOCA_TEST( p_alloc, p_new_buffer )
#endif

#define aout_BufferAlloc( p_alloc, i_nb_usec, p_previous_buffer,            \
                          p_new_buffer )                                    \
    if ( (p_alloc)->i_alloc_type == AOUT_ALLOC_NONE )                       \
    {                                                                       \
        (p_new_buffer) = p_previous_buffer;                                 \
    }                                                                       \
    else                                                                    \
    {                                                                       \
        int i_alloc_size, i_alloc_type;                                     \
        i_alloc_size = (int)( (uint64_t)(p_alloc)->i_bytes_per_sec          \
                                            * (i_nb_usec) / 1000000 + 1 );  \
        ALLOCA_TEST( p_alloc, p_new_buffer )                                \
        {                                                                   \
            (p_new_buffer) = malloc( i_alloc_size + sizeof(aout_buffer_t) );\
            i_alloc_type = AOUT_ALLOC_HEAP;                                 \
        }                                                                   \
        if ( p_new_buffer != NULL )                                         \
        {                                                                   \
            (p_new_buffer)->i_alloc_type = i_alloc_type;                    \
            (p_new_buffer)->i_size = i_alloc_size;                          \
            (p_new_buffer)->p_buffer = (byte_t *)(p_new_buffer)             \
                                         + sizeof(aout_buffer_t);           \
            if ( (p_previous_buffer) != NULL )                              \
            {                                                               \
                (p_new_buffer)->start_date =                                \
                           ((aout_buffer_t *)p_previous_buffer)->start_date;\
                (p_new_buffer)->end_date =                                  \
                           ((aout_buffer_t *)p_previous_buffer)->end_date;  \
            }                                                               \
        }                                                                   \
        /* we'll keep that for a while --Meuuh */                           \
        /* else printf("%s:%d\n", __FILE__, __LINE__); */                   \
    }

#define aout_BufferFree( p_buffer )                                         \
    if ( (p_buffer)->i_alloc_type == AOUT_ALLOC_HEAP )                      \
    {                                                                       \
        free( p_buffer );                                                   \
    }

/*****************************************************************************
 * aout_fifo_t : audio output buffer FIFO
 *****************************************************************************/
struct aout_fifo_t
{
    aout_buffer_t *         p_first;
    aout_buffer_t **        pp_last;
    audio_date_t            end_date;
};

/*****************************************************************************
 * aout_filter_t : audio output filter
 *****************************************************************************/
struct aout_filter_t
{
    VLC_COMMON_MEMBERS

    audio_sample_format_t   input;
    audio_sample_format_t   output;
    aout_alloc_t            output_alloc;

    module_t *              p_module;
    struct aout_filter_sys_t * p_sys;
    void                 (* pf_do_work)( struct aout_instance_t *,
                                         struct aout_filter_t *,
                                         struct aout_buffer_t *,
                                         struct aout_buffer_t * );
    vlc_bool_t              b_in_place;
    vlc_bool_t              b_continuity;
};

/*****************************************************************************
 * aout_mixer_t : audio output mixer
 *****************************************************************************/
typedef struct aout_mixer_t
{
    audio_sample_format_t   mixer;
    aout_alloc_t            output_alloc;

    module_t *              p_module;
    struct aout_mixer_sys_t * p_sys;
    void                 (* pf_do_work)( struct aout_instance_t *,
                                         struct aout_buffer_t * );

    /* If b_error == 1, there is no mixer. */
    vlc_bool_t              b_error;
    /* Multiplier used to raise or lower the volume of the sound in
     * software. Beware, this creates sound distortion and should be avoided
     * as much as possible. This isn't available for non-float32 mixer. */
    float                   f_multiplier;
} aout_mixer_t;

/*****************************************************************************
 * aout_input_t : input stream for the audio output
 *****************************************************************************/
#define AOUT_RESAMPLING_NONE     0
#define AOUT_RESAMPLING_UP       1
#define AOUT_RESAMPLING_DOWN     2
struct aout_input_t
{
    /* When this lock is taken, the pipeline cannot be changed by a
     * third-party. */
    vlc_mutex_t             lock;

    /* The input thread that spawned this input */
    input_thread_t         *p_input_thread;

    audio_sample_format_t   input;
    aout_alloc_t            input_alloc;

    /* pre-filters */
    aout_filter_t *         pp_filters[AOUT_MAX_FILTERS];
    int                     i_nb_filters;

    /* resamplers */
    aout_filter_t *         pp_resamplers[AOUT_MAX_FILTERS];
    int                     i_nb_resamplers;
    int                     i_resampling_type;
    mtime_t                 i_resamp_start_date;
    int                     i_resamp_start_drift;

    aout_fifo_t             fifo;

    /* Mixer information */
    byte_t *                p_first_byte_to_mix;

    /* If b_restart == 1, the input pipeline will be re-created. */
    vlc_bool_t              b_restart;

    /* If b_error == 1, there is no input pipeline. */
    vlc_bool_t              b_error;

    /* Did we just change the output format? (expect buffer inconsistencies) */
    vlc_bool_t              b_changed;

    /* internal caching delay from input */
    int                     i_pts_delay;
    /* desynchronisation delay request by the user */
    int                     i_desync;

};

/*****************************************************************************
 * aout_output_t : output stream for the audio output
 *****************************************************************************/
typedef struct aout_output_t
{
    audio_sample_format_t   output;
    /* Indicates whether the audio output is currently starving, to avoid
     * printing a 1,000 "output is starving" messages. */
    vlc_bool_t              b_starving;

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
    vlc_bool_t              b_error;
} aout_output_t;

/*****************************************************************************
 * aout_instance_t : audio output thread descriptor
 *****************************************************************************/
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

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
/* From input.c : */
int aout_InputNew( aout_instance_t * p_aout, aout_input_t * p_input );
int aout_InputDelete( aout_instance_t * p_aout, aout_input_t * p_input );
int aout_InputPlay( aout_instance_t * p_aout, aout_input_t * p_input,
                    aout_buffer_t * p_buffer );

/* From filters.c : */
VLC_EXPORT( int, aout_FiltersCreatePipeline, ( aout_instance_t * p_aout, aout_filter_t ** pp_filters, int * pi_nb_filters, const audio_sample_format_t * p_input_format, const audio_sample_format_t * p_output_format ) );
VLC_EXPORT( void, aout_FiltersDestroyPipeline, ( aout_instance_t * p_aout, aout_filter_t ** pp_filters, int i_nb_filters ) );
VLC_EXPORT( void, aout_FiltersPlay, ( aout_instance_t * p_aout, aout_filter_t ** pp_filters, int i_nb_filters, aout_buffer_t ** pp_input_buffer ) );
void aout_FiltersHintBuffers( aout_instance_t * p_aout, aout_filter_t ** pp_filters, int i_nb_filters, aout_alloc_t * p_first_alloc );

/* From mixer.c : */
int aout_MixerNew( aout_instance_t * p_aout );
void aout_MixerDelete( aout_instance_t * p_aout );
void aout_MixerRun( aout_instance_t * p_aout );
int aout_MixerMultiplierSet( aout_instance_t * p_aout, float f_multiplier );
int aout_MixerMultiplierGet( aout_instance_t * p_aout, float * pf_multiplier );

/* From output.c : */
int aout_OutputNew( aout_instance_t * p_aout,
                    audio_sample_format_t * p_format );
void aout_OutputPlay( aout_instance_t * p_aout, aout_buffer_t * p_buffer );
void aout_OutputDelete( aout_instance_t * p_aout );
VLC_EXPORT( aout_buffer_t *, aout_OutputNextBuffer, ( aout_instance_t *, mtime_t, vlc_bool_t ) );

/* From common.c : */
VLC_EXPORT( unsigned int, aout_FormatNbChannels, ( const audio_sample_format_t * p_format ) );
VLC_EXPORT( void, aout_FormatPrepare, ( audio_sample_format_t * p_format ) );
VLC_EXPORT( void, aout_FormatPrint, ( aout_instance_t * p_aout, const char * psz_text, const audio_sample_format_t * p_format ) );
VLC_EXPORT( void, aout_FormatsPrint, ( aout_instance_t * p_aout, const char * psz_text, const audio_sample_format_t * p_format1, const audio_sample_format_t * p_format2 ) );
VLC_EXPORT( const char *, aout_FormatPrintChannels, ( const audio_sample_format_t * ) );
void aout_FifoInit( aout_instance_t *, aout_fifo_t *, uint32_t );
mtime_t aout_FifoNextStart( aout_instance_t *, aout_fifo_t * );
void aout_FifoPush( aout_instance_t *, aout_fifo_t *, aout_buffer_t * );
void aout_FifoSet( aout_instance_t *, aout_fifo_t *, mtime_t );
void aout_FifoMoveDates( aout_instance_t *, aout_fifo_t *, mtime_t );
VLC_EXPORT( aout_buffer_t *, aout_FifoPop, ( aout_instance_t * p_aout, aout_fifo_t * p_fifo ) );
void aout_FifoDestroy( aout_instance_t * p_aout, aout_fifo_t * p_fifo );
VLC_EXPORT( mtime_t, aout_FifoFirstDate, ( aout_instance_t *, aout_fifo_t * ) );

/* From intf.c :*/
VLC_EXPORT( void, aout_VolumeSoftInit, ( aout_instance_t * ) );
int aout_VolumeSoftGet( aout_instance_t *, audio_volume_t * );
int aout_VolumeSoftSet( aout_instance_t *, audio_volume_t );
int aout_VolumeSoftInfos( aout_instance_t *, audio_volume_t * );
VLC_EXPORT( void, aout_VolumeNoneInit, ( aout_instance_t * ) );
int aout_VolumeNoneGet( aout_instance_t *, audio_volume_t * );
int aout_VolumeNoneSet( aout_instance_t *, audio_volume_t );
int aout_VolumeNoneInfos( aout_instance_t *, audio_volume_t * );

