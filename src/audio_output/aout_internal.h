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

#if defined(__PLUGIN__) || defined(__BUILTIN__) || !defined(__LIBVLC__)
# error This header file can only be included from LibVLC.
#endif

#ifndef __LIBVLC_AOUT_INTERNAL_H
# define __LIBVLC_AOUT_INTERNAL_H 1

#include <assert.h>

#if defined( __APPLE__ ) || defined( SYS_BSD )
#undef HAVE_ALLOCA
#endif

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
            (p_new_buffer)->p_buffer = (uint8_t *)(p_new_buffer)            \
                                         + sizeof(aout_buffer_t);           \
            (p_new_buffer)->b_discontinuity = false;                        \
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

struct aout_filter_owner_sys_t
{
    aout_instance_t *p_aout;
    aout_input_t    *p_input;
};

/****************************************************************************
 * Prototypes
 *****************************************************************************/

/* From input.c : */
int aout_InputNew( aout_instance_t * p_aout, aout_input_t * p_input, const aout_request_vout_t * );
int aout_InputDelete( aout_instance_t * p_aout, aout_input_t * p_input );
int aout_InputPlay( aout_instance_t * p_aout, aout_input_t * p_input,
                    aout_buffer_t * p_buffer, int i_input_rate );

/* From filters.c : */
int aout_FiltersCreatePipeline ( aout_instance_t * p_aout, aout_filter_t ** pp_filters, int * pi_nb_filters, const audio_sample_format_t * p_input_format, const audio_sample_format_t * p_output_format );
void aout_FiltersDestroyPipeline ( aout_instance_t * p_aout, aout_filter_t ** pp_filters, int i_nb_filters );
void  aout_FiltersPlay ( aout_instance_t * p_aout, aout_filter_t ** pp_filters, int i_nb_filters, aout_buffer_t ** pp_input_buffer );
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


/* From common.c : */
#define aout_New(a) __aout_New(VLC_OBJECT(a))
/* Release with vlc_object_release() */
aout_instance_t * __aout_New ( vlc_object_t * );

void aout_FifoInit( aout_instance_t *, aout_fifo_t *, uint32_t );
mtime_t aout_FifoNextStart( aout_instance_t *, aout_fifo_t * );
void aout_FifoPush( aout_instance_t *, aout_fifo_t *, aout_buffer_t * );
void aout_FifoSet( aout_instance_t *, aout_fifo_t *, mtime_t );
void aout_FifoMoveDates( aout_instance_t *, aout_fifo_t *, mtime_t );
void aout_FifoDestroy( aout_instance_t * p_aout, aout_fifo_t * p_fifo );
void aout_FormatsPrint( aout_instance_t * p_aout, const char * psz_text, const audio_sample_format_t * p_format1, const audio_sample_format_t * p_format2 );


/* From intf.c :*/
int aout_VolumeSoftGet( aout_instance_t *, audio_volume_t * );
int aout_VolumeSoftSet( aout_instance_t *, audio_volume_t );
int aout_VolumeSoftInfos( aout_instance_t *, audio_volume_t * );
int aout_VolumeNoneGet( aout_instance_t *, audio_volume_t * );
int aout_VolumeNoneSet( aout_instance_t *, audio_volume_t );
int aout_VolumeNoneInfos( aout_instance_t *, audio_volume_t * );

/* From dec.c */
#define aout_DecNew(a, b, c, d, e) __aout_DecNew(VLC_OBJECT(a), b, c, d, e)
aout_input_t * __aout_DecNew( vlc_object_t *, aout_instance_t **,
                              audio_sample_format_t *, const audio_replay_gain_t *,
                              const aout_request_vout_t * );
int aout_DecDelete ( aout_instance_t *, aout_input_t * );
aout_buffer_t * aout_DecNewBuffer( aout_input_t *, size_t );
void aout_DecDeleteBuffer( aout_instance_t *, aout_input_t *, aout_buffer_t * );
int aout_DecPlay( aout_instance_t *, aout_input_t *, aout_buffer_t *, int i_input_rate );
int aout_DecGetResetLost( aout_instance_t *, aout_input_t * );
void aout_DecChangePause( aout_instance_t *, aout_input_t *, bool b_paused, mtime_t i_date );
void aout_DecFlush( aout_instance_t *, aout_input_t * );

/* Helpers */

static inline void aout_lock_mixer( aout_instance_t *p_aout )
{
    vlc_mutex_lock( &p_aout->mixer_lock );
}

static inline void aout_unlock_mixer( aout_instance_t *p_aout )
{
    vlc_mutex_unlock( &p_aout->mixer_lock );
}

static inline void aout_lock_input_fifos( aout_instance_t *p_aout )
{
    vlc_mutex_lock( &p_aout->input_fifos_lock );
}

static inline void aout_unlock_input_fifos( aout_instance_t *p_aout )
{
    vlc_mutex_unlock( &p_aout->input_fifos_lock );
}

static inline void aout_lock_output_fifo( aout_instance_t *p_aout )
{
    vlc_mutex_lock( &p_aout->output_fifo_lock );
}

static inline void aout_unlock_output_fifo( aout_instance_t *p_aout )
{
    vlc_mutex_unlock( &p_aout->output_fifo_lock );
}

static inline void aout_lock_input( aout_instance_t *p_aout, aout_input_t * p_input )
{
    (void)p_aout;
    vlc_mutex_lock( &p_input->lock );
}

static inline void aout_unlock_input( aout_instance_t *p_aout, aout_input_t * p_input )
{
    (void)p_aout;
    vlc_mutex_unlock( &p_input->lock );
}


/**
 * This function will safely mark aout input to be restarted as soon as
 * possible to take configuration changes into account */
static inline void AoutInputsMarkToRestart( aout_instance_t *p_aout )
{
    int i;
    aout_lock_mixer( p_aout );
    for( i = 0; i < p_aout->i_nb_inputs; i++ )
        p_aout->pp_inputs[i]->b_restart = true;
    aout_unlock_mixer( p_aout );
}

/* This function will add or remove a a module from a string list (comma
 * separated). It will return true if there is a modification
 * In case p_aout is NULL, we will use configuration instead of variable */
static inline bool AoutChangeFilterString( vlc_object_t *p_obj, aout_instance_t * p_aout,
                                           const char* psz_variable,
                                           const char *psz_name, bool b_add )
{
    vlc_value_t val;
    char *psz_parser;

    if( *psz_name == '\0' )
        return false;

    if( p_aout )
        var_Get( p_aout, psz_variable, &val );
    else
        val.psz_string = config_GetPsz( p_obj, "audio-filter" );

    if( !val.psz_string )
        val.psz_string = strdup("");

    psz_parser = strstr( val.psz_string, psz_name );

    if( ( b_add && psz_parser ) || ( !b_add && !psz_parser ) )
    {
        /* Nothing to do */
        free( val.psz_string );
        return false;
    }

    if( b_add )
    {
        char *psz_old = val.psz_string;
        if( *psz_old )
        {
            if( asprintf( &val.psz_string, "%s:%s", psz_old, psz_name ) == -1 )
                val.psz_string = NULL;
        }
        else
            val.psz_string = strdup( psz_name );
        free( psz_old );
    }
    else
    {
        const int i_name = strlen( psz_name );
        const char *psz_next;

        psz_next = &psz_parser[i_name];
        if( *psz_next == ':' )
            psz_next++;

        memmove( psz_parser, psz_next, strlen(psz_next)+1 );
    }

    if( p_aout )
        var_Set( p_aout, psz_variable, val );
    else
        config_PutPsz( p_obj, psz_variable, val.psz_string );
    free( val.psz_string );
    return true;
}

#endif /* !__LIBVLC_AOUT_INTERNAL_H */
