/*****************************************************************************
 * audio_output.c : audio output thread
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: audio_output.c,v 1.62 2001/05/30 05:19:03 stef Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
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
#ifdef WIN32                   /* getpid() for win32 is located in process.h */
#include <process.h>
#endif

#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"                             /* mtime_t, mdate(), msleep() */
#include "modules.h"

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */

#include "audio_output.h"
#include "aout_common.h"

#include "main.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int aout_SpawnThread( aout_thread_t * p_aout );

/*****************************************************************************
 * aout_InitBank: initialize the audio output bank.
 *****************************************************************************/
void aout_InitBank ( void )
{
    p_aout_bank->i_count = 0;

    vlc_mutex_init( &p_aout_bank->lock );
}

/*****************************************************************************
 * aout_EndBank: empty the audio output bank.
 *****************************************************************************
 * This function ends all unused audio outputs and empties the bank in
 * case of success.
 *****************************************************************************/
void aout_EndBank ( void )
{
    /* Ask all remaining audio outputs to die */
    while( p_aout_bank->i_count )
    {
        aout_DestroyThread(
                p_aout_bank->pp_aout[ --p_aout_bank->i_count ], NULL );
    }

    vlc_mutex_destroy( &p_aout_bank->lock );
}

/*****************************************************************************
 * aout_CreateThread: initialize audio thread
 *****************************************************************************/
aout_thread_t *aout_CreateThread( int *pi_status )
{
    aout_thread_t * p_aout;                             /* thread descriptor */
#if 0
    int             i_status;                               /* thread status */
#endif

    /* Allocate descriptor */
    p_aout = (aout_thread_t *) malloc( sizeof(aout_thread_t) );
    if( p_aout == NULL )
    {
        return( NULL );
    }

    /* Choose the best module */
    p_aout->p_module = module_Need( MODULE_CAPABILITY_AOUT, NULL );

    if( p_aout->p_module == NULL )
    {
        intf_ErrMsg( "aout error: no suitable aout module" );
        free( p_aout );
        return( NULL );
    }

#define aout_functions p_aout->p_module->p_functions->aout.functions.aout
    p_aout->pf_open       = aout_functions.pf_open;
    p_aout->pf_setformat  = aout_functions.pf_setformat;
    p_aout->pf_getbufinfo = aout_functions.pf_getbufinfo;
    p_aout->pf_play       = aout_functions.pf_play;
    p_aout->pf_close      = aout_functions.pf_close;
#undef aout_functions

    /*
     * Initialize audio device
     */
    if ( p_aout->pf_open( p_aout ) )
    {
        module_Unneed( p_aout->p_module );
        free( p_aout );
        return( NULL );
    }

    if( p_aout->l_rate == 0 )
    {
        intf_ErrMsg( "aout error: null sample rate" );
        p_aout->pf_close( p_aout );
        module_Unneed( p_aout->p_module );
        free( p_aout );
        return( NULL );
    }

    /* special setting for ac3 pass-through mode */
    if( main_GetIntVariable( AOUT_SPDIF_VAR, 0 ) )
    {
        p_aout->i_format   = AOUT_FMT_AC3;
        p_aout->l_rate = 48000;
    }

    /* FIXME: only works for i_channels == 1 or 2 ?? */
    p_aout->b_stereo = ( p_aout->i_channels == 2 ) ? 1 : 0;

    if ( p_aout->pf_setformat( p_aout ) )
    {
        p_aout->pf_close( p_aout );
        module_Unneed( p_aout->p_module );
        free( p_aout );
        return( NULL );
    }

    /* Initialize the volume level */
    p_aout->i_volume = VOLUME_DEFAULT;
    p_aout->i_savedvolume = 0;
    
    /* FIXME: maybe it would be cleaner to change SpawnThread prototype
     * see vout to handle status correctly ?? however, it is not critical since
     * this thread is only called in main and all calls are blocking */
    if( aout_SpawnThread( p_aout ) )
    {
        p_aout->pf_close( p_aout );
        module_Unneed( p_aout->p_module );
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
    int     i_fifo;
    long    l_bytes;
    void (* pf_aout_thread)( aout_thread_t * ) = NULL;

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

    /* Make pf_aout_thread point to the right thread function, and compute the
     * byte size of the audio output buffer */
    switch ( p_aout->i_channels )
    {
    /* Audio output is mono */
    case 1:
        switch ( p_aout->i_format )
        {
        case AOUT_FMT_U8:
            intf_WarnMsg( 2, "aout info: unsigned 8 bits mono thread" );
            l_bytes = 1 * sizeof(u8) * p_aout->l_units;
            pf_aout_thread = aout_U8MonoThread;
            break;

        case AOUT_FMT_S8:
            intf_WarnMsg( 2, "aout info: signed 8 bits mono thread" );
            l_bytes = 1 * sizeof(s8) * p_aout->l_units;
            pf_aout_thread = aout_S8MonoThread;
            break;

        case AOUT_FMT_U16_LE:
        case AOUT_FMT_U16_BE:
            intf_WarnMsg( 2, "aout info: unsigned 16 bits mono thread" );
            l_bytes = 1 * sizeof(u16) * p_aout->l_units;
            pf_aout_thread = aout_U16MonoThread;
            break;

        case AOUT_FMT_S16_LE:
        case AOUT_FMT_S16_BE:
            intf_WarnMsg( 2, "aout info: signed 16 bits mono thread" );
            l_bytes = 1 * sizeof(s16) * p_aout->l_units;
            pf_aout_thread = aout_S16MonoThread;
            break;

        default:
            intf_ErrMsg( "aout error: unknown audio output format (%i)",
                         p_aout->i_format );
            return( -1 );
        }
        break;

    /* Audio output is stereo */
    case 2:
        switch ( p_aout->i_format )
        {
        case AOUT_FMT_U8:
            intf_WarnMsg( 2, "aout info: unsigned 8 bits stereo thread" );
            l_bytes = 2 * sizeof(u8) * p_aout->l_units;
            pf_aout_thread = aout_U8StereoThread;
            break;

        case AOUT_FMT_S8:
            intf_WarnMsg( 2, "aout info: signed 8 bits stereo thread" );
            l_bytes = 2 * sizeof(s8) * p_aout->l_units;
            pf_aout_thread = aout_S8StereoThread;
            break;

        case AOUT_FMT_U16_LE:
        case AOUT_FMT_U16_BE:
            intf_WarnMsg( 2, "aout info: unsigned 16 bits stereo thread" );
            l_bytes = 2 * sizeof(u16) * p_aout->l_units;
            pf_aout_thread = aout_U16StereoThread;
            break;

        case AOUT_FMT_S16_LE:
        case AOUT_FMT_S16_BE:
            intf_WarnMsg( 2, "aout info: signed 16 bits stereo thread" );
            l_bytes = 2 * sizeof(s16) * p_aout->l_units;
            pf_aout_thread = aout_S16StereoThread;
            break;

        case AOUT_FMT_AC3:
            intf_WarnMsg( 2, "aout info: ac3 pass-through thread" );
            l_bytes = SPDIF_FRAME_SIZE;
            pf_aout_thread = aout_SpdifThread;
            break;

        default:
            intf_ErrMsg( "aout error: unknown audio output format %i",
                         p_aout->i_format );
            return( -1 );
        }
        break;

    default:
        intf_ErrMsg( "aout error: unknown number of audio channels (%i)",
                     p_aout->i_channels );
        return( -1 );
    }

    /* Allocate the memory needed by the audio output buffers, and set to zero
     * the s32 buffer's memory */
    p_aout->buffer = malloc( l_bytes );
    if ( p_aout->buffer == NULL )
    {
        intf_ErrMsg( "aout error: cannot create output buffer" );
        return( -1 );
    }

    p_aout->s32_buffer = (s32 *)calloc( p_aout->l_units,
                                        sizeof(s32) << ( p_aout->b_stereo ) );
    if ( p_aout->s32_buffer == NULL )
    {
        intf_ErrMsg( "aout error: cannot create the s32 output buffer" );
        free( p_aout->buffer );
        return( -1 );
    }

    /* Rough estimate of the playing date */
    p_aout->date = mdate();

    /* Launch the thread */
    if ( vlc_thread_create( &p_aout->thread_id, "audio output",
                            (vlc_thread_func_t)pf_aout_thread, p_aout ) )
    {
        intf_ErrMsg( "aout error: cannot spawn audio output thread" );
        free( p_aout->buffer );
        free( p_aout->s32_buffer );
        return( -1 );
    }

    intf_WarnMsg( 2, "aout info: audio output thread %i spawned", getpid() );
    return( 0 );
}

/*****************************************************************************
 * aout_DestroyThread
 *****************************************************************************/
void aout_DestroyThread( aout_thread_t * p_aout, int *pi_status )
{
    int i_fifo;
    
    /* FIXME: pi_status is not handled correctly: check vout how to do!?? */

    /* Ask thread to kill itself and wait until it's done */
    p_aout->b_die = 1;
    vlc_thread_join( p_aout->thread_id ); /* only if pi_status is NULL */

    /* Free the allocated memory */
    free( p_aout->buffer );
    free( p_aout->s32_buffer );

    /* Destroy the condition and mutex locks */
    for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
    {
        vlc_mutex_destroy( &p_aout->fifo[i_fifo].data_lock );
        vlc_cond_destroy( &p_aout->fifo[i_fifo].data_wait );
    }
    vlc_mutex_destroy( &p_aout->fifos_lock );
    
    /* Free the plugin */
    p_aout->pf_close( p_aout );

    /* Release the aout module */
    module_Unneed( p_aout->p_module );

    /* Free structure */
    free( p_aout );
}

