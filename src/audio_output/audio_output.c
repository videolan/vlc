/*****************************************************************************
 * audio_output.c : audio output thread
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

/* TODO:
 *
 * - Passer un certain nombre de "fonctions" (genre add_samples) en macro ou
 *   inline
 * - Faire les optimisations dans les fonctions threads :
 *   = Stocker les "petits calculs" dans des variables au lieu de les refaire
 *     à chaque boucle
 *   = Utiliser des tables pour les gros calculs
 * - Faire une structure différente pour intf/adec fifo
 *
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <unistd.h>                                              /* getpid() */

#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"                             /* mtime_t, mdate(), msleep() */
#include "plugins.h"

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */

#include "audio_output.h"
#include "main.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int aout_SpawnThread( aout_thread_t * p_aout );

/* Creating as much aout_Thread functions as configurations is one solution,
 * examining the different cases in the Thread loop of an unique function is
 * another. I chose the first solution. */
void aout_Thread_S8_Mono        ( aout_thread_t * p_aout );
void aout_Thread_U8_Mono        ( aout_thread_t * p_aout );
void aout_Thread_S16_Mono       ( aout_thread_t * p_aout );
void aout_Thread_U16_Mono       ( aout_thread_t * p_aout );
void aout_Thread_S8_Stereo      ( aout_thread_t * p_aout );
void aout_Thread_U8_Stereo      ( aout_thread_t * p_aout );
void aout_Thread_S16_Stereo     ( aout_thread_t * p_aout );
void aout_Thread_U16_Stereo     ( aout_thread_t * p_aout );

static __inline__ void InitializeIncrement( aout_increment_t * p_increment, long l_numerator, long l_denominator );
static __inline__ int NextFrame( aout_thread_t * p_aout, aout_fifo_t * p_fifo, mtime_t aout_date );

/*****************************************************************************
 * aout_CreateThread: initialize audio thread
 *****************************************************************************/
aout_thread_t *aout_CreateThread( int *pi_status )
{
    aout_thread_t * p_aout;                             /* thread descriptor */
    char * psz_method;
#if 0
    int             i_status;                                 /* thread status */
#endif

    /* Allocate descriptor */
    p_aout = (aout_thread_t *) malloc( sizeof(aout_thread_t) );
    if( p_aout == NULL )
    {
        return( NULL );
    }

    /* Request an interface plugin */
    psz_method = main_GetPszVariable( AOUT_METHOD_VAR, AOUT_DEFAULT_METHOD );

    if( RequestPlugin( &p_aout->aout_plugin, "aout", psz_method ) )
    {
        intf_ErrMsg( "error: could not open audio plugin aout_%s.so\n", psz_method );
        free( p_aout );
        return( NULL );
    }

    /* Get plugins */
    p_aout->p_sys_open =         GetPluginFunction( p_aout->aout_plugin, "aout_SysOpen" );
    p_aout->p_sys_reset =        GetPluginFunction( p_aout->aout_plugin, "aout_SysReset" );
    p_aout->p_sys_setformat =    GetPluginFunction( p_aout->aout_plugin, "aout_SysSetFormat" );
    p_aout->p_sys_setchannels =  GetPluginFunction( p_aout->aout_plugin, "aout_SysSetChannels" );
    p_aout->p_sys_setrate =      GetPluginFunction( p_aout->aout_plugin, "aout_SysSetRate" );
    p_aout->p_sys_getbufinfo =   GetPluginFunction( p_aout->aout_plugin, "aout_SysGetBufInfo" );
    p_aout->p_sys_playsamples =  GetPluginFunction( p_aout->aout_plugin, "aout_SysPlaySamples" );
    p_aout->p_sys_close =        GetPluginFunction( p_aout->aout_plugin, "aout_SysClose" );

    /*
     * Initialize audio device
     */
    if ( p_aout->p_sys_open( p_aout ) )
    {
        TrashPlugin( p_aout->aout_plugin );
        free( p_aout );
        return( NULL );
    }

    p_aout->b_stereo = ( p_aout->i_channels == 2 ) ? 1 : 0; /* FIXME: only works
                                                   for i_channels == 1 or 2 ??*/

    if ( p_aout->p_sys_reset( p_aout ) )
    {
        p_aout->p_sys_close( p_aout );
        TrashPlugin( p_aout->aout_plugin );
        free( p_aout );
        return( NULL );
    }
    if ( p_aout->p_sys_setformat( p_aout ) )
    {
        p_aout->p_sys_close( p_aout );
        TrashPlugin( p_aout->aout_plugin );
        free( p_aout );
        return( NULL );
    }
    if ( p_aout->p_sys_setchannels( p_aout ) )
    {
        p_aout->p_sys_close( p_aout );
        TrashPlugin( p_aout->aout_plugin );
        free( p_aout );
        return( NULL );
    }
    if ( p_aout->p_sys_setrate( p_aout ) )
    {
        p_aout->p_sys_close( p_aout );
        TrashPlugin( p_aout->aout_plugin );
        free( p_aout );
        return( NULL );
    }

    /* FIXME: maybe it would be cleaner to change SpawnThread prototype
     * see vout to handle status correctly ?? however, it is not critical since
     * this thread is only called in main and all calls are blocking */
    if( aout_SpawnThread( p_aout ) )
    {
        p_aout->p_sys_close( p_aout );
        TrashPlugin( p_aout->aout_plugin );
        free( p_aout );
        return( NULL );
    }

    return( p_aout );
}

/*****************************************************************************
 * aout_SpawnThread
 *****************************************************************************/
static int aout_SpawnThread( aout_thread_t * p_aout )
{
    int             i_fifo;
    long            l_bytes;
    void *          aout_thread = NULL;

    intf_DbgMsg("aout debug: spawning audio output thread (%p)\n", p_aout);

    /* We want the audio output thread to live */
    p_aout->b_die = 0;
    p_aout->b_active = 1;

    /* Initialize the fifos lock */
    vlc_mutex_init( &p_aout->fifos_lock );
    /* Initialize audio fifos : set all fifos as empty and initialize locks */
    for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
    {
        p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO;
        vlc_mutex_init( &p_aout->fifo[i_fifo].data_lock );
        vlc_cond_init( &p_aout->fifo[i_fifo].data_wait );
    }

    /* Compute the size (in audio units) of the audio output buffer. Although
     * AOUT_BUFFER_DURATION is given in microseconds, the output rate is given
     * in Hz, that's why we need to divide by 10^6 microseconds (1 second) */
    p_aout->l_units = (long)( ((s64)p_aout->l_rate * AOUT_BUFFER_DURATION) / 1000000 );
    p_aout->l_msleep = (long)( ((s64)p_aout->l_units * 1000000) / (s64)p_aout->l_rate );

    /* Make aout_thread point to the right thread function, and compute the
     * byte size of the audio output buffer */
    switch ( p_aout->i_channels )
    {
        /* Audio output is mono */
        case 1:
            switch ( p_aout->i_format )
            {
                case AOUT_FMT_U8:
                    l_bytes = 1 * sizeof(u8) * p_aout->l_units;
                    aout_thread = (void *)aout_Thread_U8_Mono;
                    break;

                case AOUT_FMT_S8:
                    l_bytes = 1 * sizeof(s8) * p_aout->l_units;
                    aout_thread = (void *)aout_Thread_S8_Mono;
                    break;

                case AOUT_FMT_U16_LE:
                case AOUT_FMT_U16_BE:
                    l_bytes = 1 * sizeof(u16) * p_aout->l_units;
                    aout_thread = (void *)aout_Thread_U16_Mono;
                    break;

                case AOUT_FMT_S16_LE:
                case AOUT_FMT_S16_BE:
                    l_bytes = 1 * sizeof(s16) * p_aout->l_units;
                    aout_thread = (void *)aout_Thread_S16_Mono;
                    break;

                default:
                    intf_ErrMsg( "aout error: unknown audio output format (%i)\n",
                                 p_aout->i_format );
                    return( -1 );
            }
            break;

        /* Audio output is stereo */
        case 2:
            switch ( p_aout->i_format )
            {
                case AOUT_FMT_U8:
                    l_bytes = 2 * sizeof(u8) * p_aout->l_units;
                    aout_thread = (void *)aout_Thread_U8_Stereo;
                    break;

                case AOUT_FMT_S8:
                    l_bytes = 2 * sizeof(s8) * p_aout->l_units;
                    aout_thread = (void *)aout_Thread_S8_Stereo;
                    break;

                case AOUT_FMT_U16_LE:
                case AOUT_FMT_U16_BE:
                    l_bytes = 2 * sizeof(u16) * p_aout->l_units;
                    aout_thread = (void *)aout_Thread_U16_Stereo;
                    break;

                case AOUT_FMT_S16_LE:
                case AOUT_FMT_S16_BE:
                    l_bytes = 2 * sizeof(s16) * p_aout->l_units;
                    aout_thread = (void *)aout_Thread_S16_Stereo;
                    break;

                default:
                    intf_ErrMsg("aout error: unknown audio output format (%i)\n",
                        p_aout->i_format);
                    return( -1 );
            }
            break;

        default:
            intf_ErrMsg("aout error: unknown number of audio channels (%i)\n",
                p_aout->i_channels );
            return( -1 );
    }

    /* Allocate the memory needed by the audio output buffers, and set to zero
     * the s32 buffer's memory */
    if ( (p_aout->buffer = malloc(l_bytes)) == NULL )
    {
        intf_ErrMsg("aout error: not enough memory to create the output buffer\n");
        return( -1 );
    }
    if ( (p_aout->s32_buffer = (s32 *)calloc(p_aout->l_units, sizeof(s32) << ( p_aout->b_stereo))) == NULL )
    {
        intf_ErrMsg("aout error: not enough memory to create the s32 output buffer\n");
        free( p_aout->buffer );
        return( -1 );
    }

    /* Before launching the thread, we try to predict the date of the first
     * audio unit in the first output buffer */
    p_aout->date = mdate() - 1000000;

    /* Launch the thread */
    if ( vlc_thread_create( &p_aout->thread_id, "audio output", (vlc_thread_func_t)aout_thread, p_aout ) )
    {
        intf_ErrMsg("aout error: can't spawn audio output thread (%p)\n", p_aout);
        free( p_aout->buffer );
        free( p_aout->s32_buffer );
        return( -1 );
    }

    intf_DbgMsg("aout debug: audio output thread (%p) spawned\n", p_aout);
    return( 0 );
}

/*****************************************************************************
 * aout_DestroyThread
 *****************************************************************************/
void aout_DestroyThread( aout_thread_t * p_aout, int *pi_status )
{
    /* FIXME: pi_status is not handled correctly: check vout how to do!?? */

    intf_DbgMsg("aout debug: requesting termination of audio output thread (%p)\n", p_aout);

    /* Ask thread to kill itself and wait until it's done */
    p_aout->b_die = 1;
    vlc_thread_join( p_aout->thread_id ); /* only if pi_status is NULL */

    /* Free the allocated memory */
    free( p_aout->buffer );
    free( p_aout->s32_buffer );

    /* Free the structure */
    p_aout->p_sys_close( p_aout );
    intf_DbgMsg("aout debug: audio device (%s) closed\n", p_aout->psz_device);

    /* Close plugin */
    TrashPlugin( p_aout->aout_plugin );

    /* Free structure */
    free( p_aout );
}

/*****************************************************************************
 * aout_CreateFifo
 *****************************************************************************/
aout_fifo_t * aout_CreateFifo( aout_thread_t * p_aout, aout_fifo_t * p_fifo )
{
    int i_fifo;

    /* Take the fifos lock */
    vlc_mutex_lock( &p_aout->fifos_lock );

    /* Looking for a free fifo structure */
    for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
    {
        if ( p_aout->fifo[i_fifo].i_type == AOUT_EMPTY_FIFO)
        {
            break;
        }
    }
    if ( i_fifo == AOUT_MAX_FIFOS )
    {
        intf_ErrMsg("aout error: no empty fifo available\n");
        vlc_mutex_unlock( &p_aout->fifos_lock );
        return( NULL );
    }

    /* Initialize the new fifo structure */
    switch ( p_aout->fifo[i_fifo].i_type = p_fifo->i_type )
    {
        case AOUT_INTF_MONO_FIFO:
        case AOUT_INTF_STEREO_FIFO:
            p_aout->fifo[i_fifo].b_die = 0;

            p_aout->fifo[i_fifo].i_channels = p_fifo->i_channels;
            p_aout->fifo[i_fifo].b_stereo = p_fifo->b_stereo;
            p_aout->fifo[i_fifo].l_rate = p_fifo->l_rate;

            p_aout->fifo[i_fifo].buffer = p_fifo->buffer;

            p_aout->fifo[i_fifo].l_unit = 0;
            InitializeIncrement( &p_aout->fifo[i_fifo].unit_increment, p_fifo->l_rate, p_aout->l_rate );
            p_aout->fifo[i_fifo].l_units = p_fifo->l_units;
            break;

        case AOUT_ADEC_MONO_FIFO:
        case AOUT_ADEC_STEREO_FIFO:
            p_aout->fifo[i_fifo].b_die = 0;

            p_aout->fifo[i_fifo].i_channels = p_fifo->i_channels;
            p_aout->fifo[i_fifo].b_stereo = p_fifo->b_stereo;
            p_aout->fifo[i_fifo].l_rate = p_fifo->l_rate;

            p_aout->fifo[i_fifo].l_frame_size = p_fifo->l_frame_size;
            /* Allocate the memory needed to store the audio frames. As the
             * fifo is a rotative fifo, we must be able to find out whether the
             * fifo is full or empty, that's why we must in fact allocate memory
             * for (AOUT_FIFO_SIZE+1) audio frames. */
            if ( (p_aout->fifo[i_fifo].buffer = malloc( sizeof(s16)*(AOUT_FIFO_SIZE+1)*p_fifo->l_frame_size )) == NULL )
            {
                intf_ErrMsg("aout error: not enough memory to create the frames buffer\n");
                p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO;
                vlc_mutex_unlock( &p_aout->fifos_lock );
                return( NULL );
            }

            /* Allocate the memory needed to store the dates of the frames */
            if ( (p_aout->fifo[i_fifo].date = (mtime_t *)malloc( sizeof(mtime_t)*(AOUT_FIFO_SIZE+1) )) == NULL )
            {
                intf_ErrMsg("aout error: not enough memory to create the dates buffer\n");
                free( p_aout->fifo[i_fifo].buffer );
                p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO;
                vlc_mutex_unlock( &p_aout->fifos_lock );
                return( NULL );
            }

            /* Set the fifo's buffer as empty (the first frame that is to be
             * played is also the first frame that is not to be played) */
            p_aout->fifo[i_fifo].l_start_frame = 0;
            /* p_aout->fifo[i_fifo].l_next_frame = 0; */
            p_aout->fifo[i_fifo].l_end_frame = 0;

            /* Waiting for the audio decoder to compute enough frames to work
             * out the fifo's current rate (as soon as the decoder has decoded
             * enough frames, the members of the fifo structure that are not
             * initialized now will be calculated) */
            p_aout->fifo[i_fifo].b_start_frame = 0;
            p_aout->fifo[i_fifo].b_next_frame = 0;
            break;

        default:
            intf_ErrMsg("aout error: unknown fifo type (%i)\n", p_aout->fifo[i_fifo].i_type);
            p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO;
            vlc_mutex_unlock( &p_aout->fifos_lock );
            return( NULL );
    }

    /* Release the fifos lock */
    vlc_mutex_unlock( &p_aout->fifos_lock );

    /* Return the pointer to the fifo structure */
    intf_DbgMsg("aout debug: audio output fifo (%p) allocated\n", &p_aout->fifo[i_fifo]);
    return( &p_aout->fifo[i_fifo] );
}

/*****************************************************************************
 * aout_DestroyFifo
 *****************************************************************************/
void aout_DestroyFifo( aout_fifo_t * p_fifo )
{
    intf_DbgMsg("aout debug: requesting destruction of audio output fifo (%p)\n", p_fifo);
    p_fifo->b_die = 1;
}

/* Here are the local macros */

#define UPDATE_INCREMENT( increment, integer ) \
    if ( ((increment).l_remainder += (increment).l_euclidean_remainder) >= 0 )\
    { \
        (integer) += (increment).l_euclidean_integer + 1; \
        (increment).l_remainder -= (increment).l_euclidean_denominator; \
    } \
    else \
    { \
        (integer) += (increment).l_euclidean_integer; \
    }

/* Following functions are local */

/*****************************************************************************
 * InitializeIncrement
 *****************************************************************************/
static __inline__ void InitializeIncrement( aout_increment_t * p_increment, long l_numerator, long l_denominator )
{
    p_increment->l_remainder = -l_denominator;

    p_increment->l_euclidean_integer = 0;
    while ( l_numerator >= l_denominator )
    {
        p_increment->l_euclidean_integer++;
        l_numerator -= l_denominator;
    }

    p_increment->l_euclidean_remainder = l_numerator;

    p_increment->l_euclidean_denominator = l_denominator;
}

/*****************************************************************************
 * NextFrame
 *****************************************************************************/
static __inline__ int NextFrame( aout_thread_t * p_aout, aout_fifo_t * p_fifo, mtime_t aout_date )
{
    long l_units, l_rate;

    /* We take the lock */
    vlc_mutex_lock( &p_fifo->data_lock );

    /* Are we looking for a dated start frame ? */
    if ( !p_fifo->b_start_frame )
    {
        while ( p_fifo->l_start_frame != p_fifo->l_end_frame )
        {
            if ( p_fifo->date[p_fifo->l_start_frame] != LAST_MDATE )
            {
                p_fifo->b_start_frame = 1;
                p_fifo->l_next_frame = (p_fifo->l_start_frame + 1) & AOUT_FIFO_SIZE;
                p_fifo->l_unit = p_fifo->l_start_frame * (p_fifo->l_frame_size >> (p_fifo->b_stereo));
                break;
            }
            p_fifo->l_start_frame = (p_fifo->l_start_frame + 1) & AOUT_FIFO_SIZE;
        }

        if ( p_fifo->l_start_frame == p_fifo->l_end_frame )
        {
            vlc_mutex_unlock( &p_fifo->data_lock );
            return( -1 );
        }
    }

    /* We are looking for the next dated frame */
    /* FIXME : is the output fifo full ?? */
    while ( !p_fifo->b_next_frame )
    {
        while ( p_fifo->l_next_frame != p_fifo->l_end_frame )
        {
            if ( p_fifo->date[p_fifo->l_next_frame] != LAST_MDATE )
            {
                p_fifo->b_next_frame = 1;
                break;
            }
            p_fifo->l_next_frame = (p_fifo->l_next_frame + 1) & AOUT_FIFO_SIZE;
        }

        while ( p_fifo->l_next_frame == p_fifo->l_end_frame )
        {
            vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
            if ( p_fifo->b_die )
            {
                vlc_mutex_unlock( &p_fifo->data_lock );
                return( -1 );
            }
        }
    }

    l_units = ((p_fifo->l_next_frame - p_fifo->l_start_frame) & AOUT_FIFO_SIZE) * (p_fifo->l_frame_size >> (p_fifo->b_stereo));

    l_rate = p_fifo->l_rate + ((aout_date - p_fifo->date[p_fifo->l_start_frame]) / 256);
//    fprintf( stderr, "aout debug: %lli (%li);\n", aout_date - p_fifo->date[p_fifo->l_start_frame], l_rate );

    InitializeIncrement( &p_fifo->unit_increment, l_rate, p_aout->l_rate );

    p_fifo->l_units = (((l_units - (p_fifo->l_unit -
        (p_fifo->l_start_frame * (p_fifo->l_frame_size >> (p_fifo->b_stereo)))))
        * p_aout->l_rate) / l_rate) + 1;

    /* We release the lock before leaving */
    vlc_mutex_unlock( &p_fifo->data_lock );
    return( 0 );
}

void aout_Thread_S8_Mono( aout_thread_t * p_aout )
{
}

void aout_Thread_S8_Stereo( aout_thread_t * p_aout )
{
}

void aout_Thread_U8_Mono( aout_thread_t * p_aout )
{
}

void aout_Thread_U8_Stereo( aout_thread_t * p_aout )
{
    int i_fifo;
    long l_buffer, l_buffer_limit;
    long l_units, l_bytes;

    intf_DbgMsg("adec debug: ********aout_Thread_U8_Stereo********\n");
    intf_DbgMsg("adec debug: running audio output thread (%p) (pid == %i)\n", p_aout, getpid());

    /* As the s32_buffer was created with calloc(), we don't have to set this
     * memory to zero and we can immediately jump into the thread's loop */
    while ( !p_aout->b_die )
    {
        vlc_mutex_lock( &p_aout->fifos_lock );
        for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
        {
            switch ( p_aout->fifo[i_fifo].i_type )
            {
                case AOUT_EMPTY_FIFO:
                    break;

                case AOUT_INTF_MONO_FIFO:
                    if ( p_aout->fifo[i_fifo].l_units > p_aout->l_units )
                    {
                        l_buffer = 0;
                        while ( l_buffer < (p_aout->l_units << 1) ) /* p_aout->b_stereo == 1 */
                        {
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );
                            UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                        }
                        p_aout->fifo[i_fifo].l_units -= p_aout->l_units;
                    }
                    else
                    {
                        l_buffer = 0;
                        while ( l_buffer < (p_aout->fifo[i_fifo].l_units << 1) ) /* p_aout->b_stereo == 1 */
                        {
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );
                            UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                        }
                        free( p_aout->fifo[i_fifo].buffer ); /* !! */
                        p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO; /* !! */
                        intf_DbgMsg("aout debug: audio output fifo (%p) destroyed\n", &p_aout->fifo[i_fifo]); /* !! */
                    }
                    break;

                case AOUT_INTF_STEREO_FIFO:
                    if ( p_aout->fifo[i_fifo].l_units > p_aout->l_units )
                    {
                        l_buffer = 0;
                        while ( l_buffer < (p_aout->l_units << 1) ) /* p_aout->b_stereo == 1 */
                        {
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit] );
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit+1] );
                            UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                        }
                        p_aout->fifo[i_fifo].l_units -= p_aout->l_units;
                    }
                    else
                    {
                        l_buffer = 0;
                        while ( l_buffer < (p_aout->fifo[i_fifo].l_units << 1) ) /* p_aout->b_stereo == 1 */
                        {
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit] );
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit+1] );
                            UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                        }
                        free( p_aout->fifo[i_fifo].buffer ); /* !! */
                        p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO; /* !! */
                        intf_DbgMsg("aout debug: audio output fifo (%p) destroyed\n", &p_aout->fifo[i_fifo]); /* !! */
                    }
                    break;

                case AOUT_ADEC_MONO_FIFO:
                    if ( p_aout->fifo[i_fifo].b_die )
                    {
                        free( p_aout->fifo[i_fifo].buffer );
                        free( p_aout->fifo[i_fifo].date );
                        p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO; /* !! */
                        intf_DbgMsg("aout debug: audio output fifo (%p) destroyed\n", &p_aout->fifo[i_fifo]);
                        continue;
                    }

                    l_units = p_aout->l_units;
                    l_buffer = 0;
                    while ( l_units > 0 )
                    {
                        if ( !p_aout->fifo[i_fifo].b_next_frame )
                        {
                            if ( NextFrame(p_aout, &p_aout->fifo[i_fifo], p_aout->date + ((((mtime_t)(l_buffer >> 1)) * 1000000) / ((mtime_t)p_aout->l_rate))) )
                            {
                                break;
                            }
                        }

                        if ( p_aout->fifo[i_fifo].l_units > l_units )
                        {
                            l_buffer_limit = p_aout->l_units << 1; /* p_aout->b_stereo == 1 */
                            while ( l_buffer < l_buffer_limit )
                            {
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );

                                UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                                if ( p_aout->fifo[i_fifo].l_unit >= /* p_aout->fifo[i_fifo].b_stereo == 0 */
                                     ((AOUT_FIFO_SIZE + 1) * (p_aout->fifo[i_fifo].l_frame_size >> 0)) )
                                {
                                    p_aout->fifo[i_fifo].l_unit -= /* p_aout->fifo[i_fifo].b_stereo == 0 */
                                        ((AOUT_FIFO_SIZE + 1) * (p_aout->fifo[i_fifo].l_frame_size >> 0));
                                }
                            }
                            p_aout->fifo[i_fifo].l_units -= l_units;
                            break;
                        }
                        else
                        {
                            l_buffer_limit = l_buffer + (p_aout->fifo[i_fifo].l_units << 1);
                            /* p_aout->b_stereo == 1 */
                            while ( l_buffer < l_buffer_limit )
                            {
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );

                                UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                                if ( p_aout->fifo[i_fifo].l_unit >= /* p_aout->fifo[i_fifo].b_stereo == 0 */
                                     ((AOUT_FIFO_SIZE + 1) * (p_aout->fifo[i_fifo].l_frame_size >> 0)) )
                                {
                                    p_aout->fifo[i_fifo].l_unit -= /* p_aout->fifo[i_fifo].b_stereo == 0 */
                                        ((AOUT_FIFO_SIZE + 1) * (p_aout->fifo[i_fifo].l_frame_size >> 0));
                                }
                            }
                            l_units -= p_aout->fifo[i_fifo].l_units;

                            vlc_mutex_lock( &p_aout->fifo[i_fifo].data_lock );
                            p_aout->fifo[i_fifo].l_start_frame = p_aout->fifo[i_fifo].l_next_frame;
                            vlc_cond_signal( &p_aout->fifo[i_fifo].data_wait );
                            vlc_mutex_unlock( &p_aout->fifo[i_fifo].data_lock );

                            /* p_aout->fifo[i_fifo].b_start_frame = 1; */
                            p_aout->fifo[i_fifo].l_next_frame += 1;
                            p_aout->fifo[i_fifo].l_next_frame &= AOUT_FIFO_SIZE;
                            p_aout->fifo[i_fifo].b_next_frame = 0;
                        }
                    }
                    break;

                case AOUT_ADEC_STEREO_FIFO:
                    if ( p_aout->fifo[i_fifo].b_die )
                    {
                        free( p_aout->fifo[i_fifo].buffer );
                        free( p_aout->fifo[i_fifo].date );
                        p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO; /* !! */
                        intf_DbgMsg("aout debug: audio output fifo (%p) destroyed\n", &p_aout->fifo[i_fifo]);
                        continue;
                    }

                    l_units = p_aout->l_units;
                    l_buffer = 0;
                    while ( l_units > 0 )
                    {
                        if ( !p_aout->fifo[i_fifo].b_next_frame )
                        {
                            if ( NextFrame(p_aout, &p_aout->fifo[i_fifo], p_aout->date + ((((mtime_t)(l_buffer >> 1)) * 1000000) / ((mtime_t)p_aout->l_rate))) )
                            {
                                break;
                            }
                        }

                        if ( p_aout->fifo[i_fifo].l_units > l_units )
                        {
                            l_buffer_limit = p_aout->l_units << 1; /* p_aout->b_stereo == 1 */
                            while ( l_buffer < l_buffer_limit )
                            {
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit] );
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit+1] );

                                UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                                if ( p_aout->fifo[i_fifo].l_unit >= /* p_aout->fifo[i_fifo].b_stereo == 1 */
                                     ((AOUT_FIFO_SIZE + 1) * (p_aout->fifo[i_fifo].l_frame_size >> 1)) )
                                {
                                    p_aout->fifo[i_fifo].l_unit -= /* p_aout->fifo[i_fifo].b_stereo == 1 */
                                        ((AOUT_FIFO_SIZE + 1) * (p_aout->fifo[i_fifo].l_frame_size >> 1));
                                }
                            }
                            p_aout->fifo[i_fifo].l_units -= l_units;
                            break;
                        }
                        else
                        {
                            l_buffer_limit = l_buffer + (p_aout->fifo[i_fifo].l_units << 1);
                            /* p_aout->b_stereo == 1 */
                            while ( l_buffer < l_buffer_limit )
                            {
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit] );
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit+1] );

                                UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                                if ( p_aout->fifo[i_fifo].l_unit >= /* p_aout->fifo[i_fifo].b_stereo == 1 */
                                     ((AOUT_FIFO_SIZE + 1) * (p_aout->fifo[i_fifo].l_frame_size >> 1)) )
                                {
                                    p_aout->fifo[i_fifo].l_unit -= /* p_aout->fifo[i_fifo].b_stereo == 1 */
                                        ((AOUT_FIFO_SIZE + 1) * (p_aout->fifo[i_fifo].l_frame_size >> 1));
                                }
                            }
                            l_units -= p_aout->fifo[i_fifo].l_units;

                            vlc_mutex_lock( &p_aout->fifo[i_fifo].data_lock );
                            p_aout->fifo[i_fifo].l_start_frame = p_aout->fifo[i_fifo].l_next_frame;
                            vlc_cond_signal( &p_aout->fifo[i_fifo].data_wait );
                            vlc_mutex_unlock( &p_aout->fifo[i_fifo].data_lock );

                            /* p_aout->fifo[i_fifo].b_start_frame = 1; */
                            p_aout->fifo[i_fifo].l_next_frame += 1;
                            p_aout->fifo[i_fifo].l_next_frame &= AOUT_FIFO_SIZE;
                            p_aout->fifo[i_fifo].b_next_frame = 0;
                        }
                    }
                    break;

            default:
                    intf_DbgMsg("aout debug: unknown fifo type (%i)\n", p_aout->fifo[i_fifo].i_type);
                    break;
            }
        }
        vlc_mutex_unlock( &p_aout->fifos_lock );

        l_buffer_limit = p_aout->l_units  << 1 ; /* p_aout->b_stereo == 1 */

        for ( l_buffer = 0; l_buffer < l_buffer_limit; l_buffer++ )
        {
            ((u8 *)p_aout->buffer)[l_buffer] = (u8)( (p_aout->s32_buffer[l_buffer] / 256) + 128 );
            p_aout->s32_buffer[l_buffer] = 0;
        }
        l_bytes = p_aout->p_sys_getbufinfo( p_aout, l_buffer_limit );
        p_aout->date = mdate() + ((((mtime_t)(l_bytes / 2 )) * 1000000) / ((mtime_t)p_aout->l_rate)); /* sizeof(u8) << (p_aout->b_stereo) == 2 */
        p_aout->p_sys_playsamples( p_aout, (byte_t *)p_aout->buffer, l_buffer_limit * sizeof(u8) );
        if ( l_bytes > (l_buffer_limit * sizeof(u8)) )
        {
            msleep( p_aout->l_msleep );
        }
    }

    vlc_mutex_lock( &p_aout->fifos_lock );
    for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
    {
        switch ( p_aout->fifo[i_fifo].i_type )
        {
            case AOUT_EMPTY_FIFO:
                break;

            case AOUT_INTF_MONO_FIFO:
            case AOUT_INTF_STEREO_FIFO:
                free( p_aout->fifo[i_fifo].buffer ); /* !! */
                p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO; /* !! */
                intf_DbgMsg("aout debug: audio output fifo (%p) destroyed\n", &p_aout->fifo[i_fifo]);
                break;

            case AOUT_ADEC_MONO_FIFO:
            case AOUT_ADEC_STEREO_FIFO:
                free( p_aout->fifo[i_fifo].buffer );
                free( p_aout->fifo[i_fifo].date );
                p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO; /* !! */
                intf_DbgMsg("aout debug: audio output fifo (%p) destroyed\n", &p_aout->fifo[i_fifo]);
                break;

            default:
                break;
        }
    }
    vlc_mutex_unlock( &p_aout->fifos_lock );

}

void aout_Thread_S16_Mono( aout_thread_t * p_aout )
{
}

void aout_Thread_S16_Stereo( aout_thread_t * p_aout )
{
    int i_fifo;
    long l_buffer, l_buffer_limit;
    long l_units, l_bytes;

    intf_DbgMsg("adec debug: ********aout_Thread_S16_Stereo********\n");
    intf_DbgMsg("adec debug: running audio output thread (%p) (pid == %i)\n", p_aout, getpid());

    /* As the s32_buffer was created with calloc(), we don't have to set this
     * memory to zero and we can immediately jump into the thread's loop */
    while ( !p_aout->b_die )
    {
        vlc_mutex_lock( &p_aout->fifos_lock );
        for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
        {
            switch ( p_aout->fifo[i_fifo].i_type )
            {
                case AOUT_EMPTY_FIFO:
                    break;

                case AOUT_INTF_MONO_FIFO:
                    if ( p_aout->fifo[i_fifo].l_units > p_aout->l_units )
                    {
                        l_buffer = 0;
                        while ( l_buffer < (p_aout->l_units << 1) ) /* p_aout->b_stereo == 1 */
                        {
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );
                            UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                        }
                        p_aout->fifo[i_fifo].l_units -= p_aout->l_units;
                    }
                    else
                    {
                        l_buffer = 0;
                        while ( l_buffer < (p_aout->fifo[i_fifo].l_units << 1) ) /* p_aout->b_stereo == 1 */
                        {
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );
                            UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                        }
                        free( p_aout->fifo[i_fifo].buffer ); /* !! */
                        p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO; /* !! */
                        intf_DbgMsg("aout debug: audio output fifo (%p) destroyed\n", &p_aout->fifo[i_fifo]); /* !! */
                    }
                    break;

                case AOUT_INTF_STEREO_FIFO:
                    if ( p_aout->fifo[i_fifo].l_units > p_aout->l_units )
                    {
                        l_buffer = 0;
                        while ( l_buffer < (p_aout->l_units << 1) ) /* p_aout->b_stereo == 1 */
                        {
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit] );
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit+1] );
                            UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                        }
                        p_aout->fifo[i_fifo].l_units -= p_aout->l_units;
                    }
                    else
                    {
                        l_buffer = 0;
                        while ( l_buffer < (p_aout->fifo[i_fifo].l_units << 1) ) /* p_aout->b_stereo == 1 */
                        {
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit] );
                            p_aout->s32_buffer[l_buffer++] +=
                                (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit+1] );
                            UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                        }
                        free( p_aout->fifo[i_fifo].buffer ); /* !! */
                        p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO; /* !! */
                        intf_DbgMsg("aout debug: audio output fifo (%p) destroyed\n", &p_aout->fifo[i_fifo]); /* !! */
                    }
                    break;

                case AOUT_ADEC_MONO_FIFO:
                    if ( p_aout->fifo[i_fifo].b_die )
                    {
                        free( p_aout->fifo[i_fifo].buffer );
                        free( p_aout->fifo[i_fifo].date );
                        p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO; /* !! */
                        intf_DbgMsg("aout debug: audio output fifo (%p) destroyed\n", &p_aout->fifo[i_fifo]);
                        continue;
                    }

                    l_units = p_aout->l_units;
                    l_buffer = 0;
                    while ( l_units > 0 )
                    {
                        if ( !p_aout->fifo[i_fifo].b_next_frame )
                        {
                            if ( NextFrame(p_aout, &p_aout->fifo[i_fifo], p_aout->date + ((((mtime_t)(l_buffer >> 1)) * 1000000) / ((mtime_t)p_aout->l_rate))) )
                            {
                                break;
                            }
                        }

                        if ( p_aout->fifo[i_fifo].l_units > l_units )
                        {
                            l_buffer_limit = p_aout->l_units << 1; /* p_aout->b_stereo == 1 */
                            while ( l_buffer < l_buffer_limit )
                            {
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );

                                UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                                if ( p_aout->fifo[i_fifo].l_unit >= /* p_aout->fifo[i_fifo].b_stereo == 0 */
                                     ((AOUT_FIFO_SIZE + 1) * (p_aout->fifo[i_fifo].l_frame_size >> 0)) )
                                {
                                    p_aout->fifo[i_fifo].l_unit -= /* p_aout->fifo[i_fifo].b_stereo == 0 */
                                        ((AOUT_FIFO_SIZE + 1) * (p_aout->fifo[i_fifo].l_frame_size >> 0));
                                }
                            }
                            p_aout->fifo[i_fifo].l_units -= l_units;
                            break;
                        }
                        else
                        {
                            l_buffer_limit = l_buffer + (p_aout->fifo[i_fifo].l_units << 1);
                            /* p_aout->b_stereo == 1 */
                            while ( l_buffer < l_buffer_limit )
                            {
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[p_aout->fifo[i_fifo].l_unit] );

                                UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                                if ( p_aout->fifo[i_fifo].l_unit >= /* p_aout->fifo[i_fifo].b_stereo == 0 */
                                     ((AOUT_FIFO_SIZE + 1) * (p_aout->fifo[i_fifo].l_frame_size >> 0)) )
                                {
                                    p_aout->fifo[i_fifo].l_unit -= /* p_aout->fifo[i_fifo].b_stereo == 0 */
                                        ((AOUT_FIFO_SIZE + 1) * (p_aout->fifo[i_fifo].l_frame_size >> 0));
                                }
                            }
                            l_units -= p_aout->fifo[i_fifo].l_units;

                            vlc_mutex_lock( &p_aout->fifo[i_fifo].data_lock );
                            p_aout->fifo[i_fifo].l_start_frame = p_aout->fifo[i_fifo].l_next_frame;
                            vlc_cond_signal( &p_aout->fifo[i_fifo].data_wait );
                            vlc_mutex_unlock( &p_aout->fifo[i_fifo].data_lock );

                            /* p_aout->fifo[i_fifo].b_start_frame = 1; */
                            p_aout->fifo[i_fifo].l_next_frame += 1;
                            p_aout->fifo[i_fifo].l_next_frame &= AOUT_FIFO_SIZE;
                            p_aout->fifo[i_fifo].b_next_frame = 0;
                        }
                    }
                    break;

                case AOUT_ADEC_STEREO_FIFO:
                    if ( p_aout->fifo[i_fifo].b_die )
                    {
                        free( p_aout->fifo[i_fifo].buffer );
                        free( p_aout->fifo[i_fifo].date );
                        p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO; /* !! */
                        intf_DbgMsg("aout debug: audio output fifo (%p) destroyed\n", &p_aout->fifo[i_fifo]);
                        continue;
                    }

                    l_units = p_aout->l_units;
                    l_buffer = 0;
                    while ( l_units > 0 )
                    {
                        if ( !p_aout->fifo[i_fifo].b_next_frame )
                        {
                            if ( NextFrame(p_aout, &p_aout->fifo[i_fifo], p_aout->date + ((((mtime_t)(l_buffer >> 1)) * 1000000) / ((mtime_t)p_aout->l_rate))) )
                            {
                                break;
                            }
                        }

                        if ( p_aout->fifo[i_fifo].l_units > l_units )
                        {
                            l_buffer_limit = p_aout->l_units << 1; /* p_aout->b_stereo == 1 */
                            while ( l_buffer < l_buffer_limit )
                            {
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit] );
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit+1] );

                                UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                                if ( p_aout->fifo[i_fifo].l_unit >= /* p_aout->fifo[i_fifo].b_stereo == 1 */
                                     ((AOUT_FIFO_SIZE + 1) * (p_aout->fifo[i_fifo].l_frame_size >> 1)) )
                                {
                                    p_aout->fifo[i_fifo].l_unit -= /* p_aout->fifo[i_fifo].b_stereo == 1 */
                                        ((AOUT_FIFO_SIZE + 1) * (p_aout->fifo[i_fifo].l_frame_size >> 1));
                                }
                            }
                            p_aout->fifo[i_fifo].l_units -= l_units;
                            break;
                        }
                        else
                        {
                            l_buffer_limit = l_buffer + (p_aout->fifo[i_fifo].l_units << 1);
                            /* p_aout->b_stereo == 1 */
                            while ( l_buffer < l_buffer_limit )
                            {
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit] );
                                p_aout->s32_buffer[l_buffer++] +=
                                    (s32)( ((s16 *)p_aout->fifo[i_fifo].buffer)[2*p_aout->fifo[i_fifo].l_unit+1] );

                                UPDATE_INCREMENT( p_aout->fifo[i_fifo].unit_increment, p_aout->fifo[i_fifo].l_unit )
                                if ( p_aout->fifo[i_fifo].l_unit >= /* p_aout->fifo[i_fifo].b_stereo == 1 */
                                     ((AOUT_FIFO_SIZE + 1) * (p_aout->fifo[i_fifo].l_frame_size >> 1)) )
                                {
                                    p_aout->fifo[i_fifo].l_unit -= /* p_aout->fifo[i_fifo].b_stereo == 1 */
                                        ((AOUT_FIFO_SIZE + 1) * (p_aout->fifo[i_fifo].l_frame_size >> 1));
                                }
                            }
                            l_units -= p_aout->fifo[i_fifo].l_units;

                            vlc_mutex_lock( &p_aout->fifo[i_fifo].data_lock );
                            p_aout->fifo[i_fifo].l_start_frame = p_aout->fifo[i_fifo].l_next_frame;
                            vlc_cond_signal( &p_aout->fifo[i_fifo].data_wait );
                            vlc_mutex_unlock( &p_aout->fifo[i_fifo].data_lock );

                            /* p_aout->fifo[i_fifo].b_start_frame = 1; */
                            p_aout->fifo[i_fifo].l_next_frame += 1;
                            p_aout->fifo[i_fifo].l_next_frame &= AOUT_FIFO_SIZE;
                            p_aout->fifo[i_fifo].b_next_frame = 0;
                        }
                    }
                    break;

            default:
                    intf_DbgMsg("aout debug: unknown fifo type (%i)\n", p_aout->fifo[i_fifo].i_type);
                    break;
            }
        }
        vlc_mutex_unlock( &p_aout->fifos_lock );

        l_buffer_limit = p_aout->l_units << 1; /* p_aout->b_stereo == 1 */

        for ( l_buffer = 0; l_buffer < l_buffer_limit; l_buffer++ )
        {
            ((s16 *)p_aout->buffer)[l_buffer] = (s16)( p_aout->s32_buffer[l_buffer] / AOUT_MAX_FIFOS );
            p_aout->s32_buffer[l_buffer] = 0;
        }

        l_bytes = p_aout->p_sys_getbufinfo( p_aout, l_buffer_limit );
        p_aout->date = mdate() + ((((mtime_t)(l_bytes / 4)) * 1000000) / ((mtime_t)p_aout->l_rate)); /* sizeof(s16) << (p_aout->b_stereo) == 4 */
        p_aout->p_sys_playsamples( p_aout, (byte_t *)p_aout->buffer, l_buffer_limit * sizeof(s16) );
        if ( l_bytes > (l_buffer_limit * sizeof(s16)) )
        {
            msleep( p_aout->l_msleep );
        }
    }

    vlc_mutex_lock( &p_aout->fifos_lock );
    for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
    {
        switch ( p_aout->fifo[i_fifo].i_type )
        {
            case AOUT_EMPTY_FIFO:
                break;

            case AOUT_INTF_MONO_FIFO:
            case AOUT_INTF_STEREO_FIFO:
                free( p_aout->fifo[i_fifo].buffer ); /* !! */
                p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO; /* !! */
                intf_DbgMsg("aout debug: audio output fifo (%p) destroyed\n", &p_aout->fifo[i_fifo]);
                break;

            case AOUT_ADEC_MONO_FIFO:
            case AOUT_ADEC_STEREO_FIFO:
                free( p_aout->fifo[i_fifo].buffer );
                free( p_aout->fifo[i_fifo].date );
                p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO; /* !! */
                intf_DbgMsg("aout debug: audio output fifo (%p) destroyed\n", &p_aout->fifo[i_fifo]);
                break;

            default:
                break;
        }
    }
    vlc_mutex_unlock( &p_aout->fifos_lock );
}

void aout_Thread_U16_Mono( aout_thread_t * p_aout )
{
}

void aout_Thread_U16_Stereo( aout_thread_t * p_aout )
{
}
