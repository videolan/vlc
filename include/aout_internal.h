/*****************************************************************************
 * aout_internal.h : internal defines for audio output
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: aout_internal.h,v 1.12 2002/08/25 09:39:59 sam Exp $
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

#define aout_BufferAlloc( p_alloc, i_nb_usec, p_previous_buffer,            \
                          p_new_buffer )                                    \
    if ( (p_alloc)->i_alloc_type == AOUT_ALLOC_NONE )                       \
    {                                                                       \
        (p_new_buffer) = p_previous_buffer;                                 \
    }                                                                       \
    else                                                                    \
    {                                                                       \
        int i_alloc_size;                                                   \
        i_alloc_size = (u64)(p_alloc)->i_bytes_per_sec                      \
                                            * (i_nb_usec) / 1000000 + 1;    \
        if ( (p_alloc)->i_alloc_type == AOUT_ALLOC_STACK )                  \
        {                                                                   \
            (p_new_buffer) = alloca( i_alloc_size + sizeof(aout_buffer_t) );\
        }                                                                   \
        else                                                                \
        {                                                                   \
            (p_new_buffer) = malloc( i_alloc_size + sizeof(aout_buffer_t) );\
        }                                                                   \
        if ( p_new_buffer != NULL )                                         \
        {                                                                   \
            (p_new_buffer)->i_alloc_type = (p_alloc)->i_alloc_type;         \
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
typedef struct aout_filter_t
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
} aout_filter_t;

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
} aout_mixer_t;

/*****************************************************************************
 * aout_input_t : input stream for the audio output
 *****************************************************************************/
struct aout_input_t
{
    audio_sample_format_t   input;
    aout_alloc_t            input_alloc;

    /* pre-filters */
    aout_filter_t *         pp_filters[AOUT_MAX_FILTERS];
    int                     i_nb_filters;

    aout_fifo_t             fifo;

    /* Mixer information */
    byte_t *                p_first_byte_to_mix;
};

/*****************************************************************************
 * aout_output_t : output stream for the audio output
 *****************************************************************************/
typedef struct aout_output_t
{
    audio_sample_format_t   output;

    /* post-filters */
    aout_filter_t *         pp_filters[AOUT_MAX_FILTERS];
    int                     i_nb_filters;

    aout_fifo_t             fifo;

    struct module_t *       p_module;
    struct aout_sys_t *     p_sys;
    int                  (* pf_setformat)( aout_instance_t * );
    void                 (* pf_play)( aout_instance_t * );
    int                     i_nb_samples;
} aout_output_t;

/*****************************************************************************
 * aout_instance_t : audio output thread descriptor
 *****************************************************************************/
struct aout_instance_t
{
    VLC_COMMON_MEMBERS

    /* Input streams & pre-filters */
    vlc_mutex_t             input_lock;
    vlc_cond_t              input_signal;
    int                     i_inputs_active;
    vlc_bool_t              b_change_requested;
    aout_input_t *          pp_inputs[AOUT_MAX_INPUTS];
    int                     i_nb_inputs;

    /* Mixer */
    vlc_mutex_t             mixer_lock;
    aout_mixer_t            mixer;

    /* Output plug-in */
    aout_output_t           output;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void aout_InputPlay( aout_instance_t * p_aout, aout_input_t * p_input,
                     aout_buffer_t * p_buffer );

int aout_FiltersCreatePipeline( aout_instance_t * p_aout,
                                aout_filter_t ** pp_filters,
                                int * pi_nb_filters,
                                const audio_sample_format_t * p_input_format,
                                const audio_sample_format_t * p_output_format );
void aout_FiltersDestroyPipeline( aout_instance_t * p_aout,
                                  aout_filter_t ** pp_filters,
                                  int i_nb_filters );
void aout_FiltersHintBuffers( aout_instance_t * p_aout,
                              aout_filter_t ** pp_filters,
                              int i_nb_filters, aout_alloc_t * p_first_alloc );
void aout_FiltersPlay( aout_instance_t * p_aout,
                       aout_filter_t ** pp_filters,
                       int i_nb_filters, aout_buffer_t ** pp_input_buffer );

int aout_MixerNew( aout_instance_t * p_aout );
void aout_MixerDelete( aout_instance_t * p_aout );
void aout_MixerRun( aout_instance_t * p_aout );

int aout_OutputNew( aout_instance_t * p_aout,
                    audio_sample_format_t * p_format );
void aout_OutputPlay( aout_instance_t * p_aout, aout_buffer_t * p_buffer );
void aout_OutputDelete( aout_instance_t * p_aout );
VLC_EXPORT( aout_buffer_t *, aout_OutputNextBuffer, ( aout_instance_t *, mtime_t, vlc_bool_t ) );

void aout_FormatPrepare( audio_sample_format_t * p_format );
void aout_FifoInit( aout_instance_t *, aout_fifo_t *, u32 );
mtime_t aout_FifoNextStart( aout_instance_t *, aout_fifo_t * );
void aout_FifoPush( aout_instance_t *, aout_fifo_t *, aout_buffer_t * );
void aout_FifoSet( aout_instance_t *, aout_fifo_t *, mtime_t );
void aout_FifoMoveDates( aout_instance_t *, aout_fifo_t *, mtime_t );
VLC_EXPORT( aout_buffer_t *, aout_FifoPop, ( aout_instance_t * p_aout, aout_fifo_t * p_fifo ) );
void aout_FifoDestroy( aout_instance_t * p_aout, aout_fifo_t * p_fifo );

