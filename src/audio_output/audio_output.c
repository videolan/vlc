/*****************************************************************************
 * audio_output.c : audio output thread
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: audio_output.c,v 1.86 2002/06/08 14:08:46 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Cyril Deguet <asmax@via.ecp.fr>
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
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>                                           /* getpid() */
#endif

#ifdef WIN32                   /* getpid() for win32 is located in process.h */
#   include <process.h>
#endif

#include "audio_output.h"

#include "aout_pcm.h"
#include "aout_spdif.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  aout_SpawnThread ( aout_thread_t * p_aout );

/*****************************************************************************
 * aout_CreateThread: initialize audio thread
 *****************************************************************************/
aout_thread_t *aout_CreateThread( vlc_object_t *p_parent,
                                  int i_channels, int i_rate )
{
    aout_thread_t * p_aout;                             /* thread descriptor */
    char *          psz_name;
    int             i_format;

    /* Allocate descriptor */
    p_aout = vlc_object_create( p_parent, VLC_OBJECT_AOUT );
    if( p_aout == NULL )
    {
        return NULL;
    }

    p_aout->i_latency = 0;
    p_aout->i_rate = config_GetInt( p_aout, "rate" );
    p_aout->i_channels = config_GetInt( p_aout, "mono" ) ? 1 : 2;

    i_format = config_GetInt( p_aout, "audio-format" );
    if( ( !i_format ) || ( i_format > 8 ) )
    {
        p_aout->i_format = AOUT_FMT_S16_NE;
    }
    else
    {
        p_aout->i_format = 1 << ( i_format + 2 );
    }

    if( p_aout->i_rate == 0 )
    {
        msg_Err( p_aout, "null sample rate" );
        vlc_object_destroy( p_aout );
        return NULL;
    }

    /* Choose the best module */
    psz_name = config_GetPsz( p_aout, "aout" );
    p_aout->p_module = module_Need( p_aout, MODULE_CAPABILITY_AOUT,
                                    psz_name, (void *)p_aout );
    if( psz_name ) free( psz_name );
    if( p_aout->p_module == NULL )
    {
        msg_Err( p_aout, "no suitable aout module" );
        vlc_object_destroy( p_aout );
        return NULL;
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
    if ( p_aout->pf_setformat( p_aout ) )
    {
        p_aout->pf_close( p_aout );
        module_Unneed( p_aout->p_module );
        vlc_object_destroy( p_aout );
        return NULL;
    }

    /* Initialize the volume level */
    p_aout->i_volume = config_GetInt( p_aout, "volume" );
    p_aout->i_savedvolume = 0;
    
    /* FIXME: maybe it would be cleaner to change SpawnThread prototype
     * see vout to handle status correctly ?? however, it is not critical since
     * this thread is only called in main and all calls are blocking */
    if( aout_SpawnThread( p_aout ) )
    {
        p_aout->pf_close( p_aout );
        module_Unneed( p_aout->p_module );
        vlc_object_destroy( p_aout );
        return NULL;
    }

    vlc_object_attach( p_aout, p_parent->p_vlc );

    return p_aout;
}

/*****************************************************************************
 * aout_SpawnThread
 *****************************************************************************/
static int aout_SpawnThread( aout_thread_t * p_aout )
{
    int     i_index, i_bytes;
    void (* pf_aout_thread)( aout_thread_t * ) = NULL;
    char   *psz_format;

    /* Initialize the fifos lock */
    vlc_mutex_init( p_aout, &p_aout->fifos_lock );

    /* Initialize audio fifos : set all fifos as empty and initialize locks */
    for ( i_index = 0; i_index < AOUT_MAX_FIFOS; i_index++ )
    {
        p_aout->fifo[i_index].i_format = AOUT_FIFO_NONE;
        vlc_mutex_init( p_aout, &p_aout->fifo[i_index].data_lock );
        vlc_cond_init( p_aout, &p_aout->fifo[i_index].data_wait );
    }

    /* Compute the size (in audio units) of the audio output buffer. Although
     * AOUT_BUFFER_DURATION is given in microseconds, the output rate is given
     * in Hz, that's why we need to divide by 10^6 microseconds (1 second) */
    p_aout->i_units = (s64)p_aout->i_rate * AOUT_BUFFER_DURATION / 1000000;

    /* Make pf_aout_thread point to the right thread function, and compute the
     * byte size of the audio output buffer */
    switch ( p_aout->i_format )
    {
        case AOUT_FMT_U8:
            pf_aout_thread = aout_PCMThread;
            psz_format = "unsigned 8 bits";
            i_bytes = p_aout->i_units * p_aout->i_channels;
            break;

        case AOUT_FMT_S8:
            pf_aout_thread = aout_PCMThread;
            psz_format = "signed 8 bits";
            i_bytes = p_aout->i_units * p_aout->i_channels;
            break;

        case AOUT_FMT_U16_LE:
        case AOUT_FMT_U16_BE:
            pf_aout_thread = aout_PCMThread;
            psz_format = "unsigned 16 bits";
            i_bytes = 2 * p_aout->i_units * p_aout->i_channels;
            break;

        case AOUT_FMT_S16_LE:
        case AOUT_FMT_S16_BE:
            pf_aout_thread = aout_PCMThread;
            psz_format = "signed 16 bits";
            i_bytes = 2 * p_aout->i_units * p_aout->i_channels;
            break;

        case AOUT_FMT_AC3:
            pf_aout_thread = aout_SpdifThread;
            psz_format = "ac3 pass-through";
            i_bytes = SPDIF_FRAME_SIZE;
            break;

        default:
            msg_Err( p_aout, "unknown audio output format %i",
                             p_aout->i_format );
            return( -1 );
    }

    /* Allocate the memory needed by the audio output buffers, and set to zero
     * the s32 buffer's memory */
    p_aout->buffer = malloc( i_bytes );
    if ( p_aout->buffer == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        return( -1 );
    }

    p_aout->s32_buffer = (s32 *)calloc( p_aout->i_units,
                                        sizeof(s32) * p_aout->i_channels );
    if ( p_aout->s32_buffer == NULL )
    {
        msg_Err( p_aout, "out of memory" );
        free( p_aout->buffer );
        return( -1 );
    }

    /* Rough estimate of the playing date */
    p_aout->date = mdate() + p_aout->p_vlc->i_desync;

    /* Launch the thread */
    if ( vlc_thread_create( p_aout, "audio output", pf_aout_thread, 0 ) )
    {
        msg_Err( p_aout, "cannot spawn audio output thread" );
        free( p_aout->buffer );
        free( p_aout->s32_buffer );
        return( -1 );
    }

    msg_Dbg( p_aout, "%s thread spawned, %i channels, rate %i",
                     psz_format, p_aout->i_channels, p_aout->i_rate );
    return( 0 );
}

/*****************************************************************************
 * aout_DestroyThread
 *****************************************************************************/
void aout_DestroyThread( aout_thread_t * p_aout )
{
    int i_index;
    
    /* Ask thread to kill itself and wait until it's done */
    p_aout->b_die = 1;

    vlc_thread_join( p_aout );

    /* Free the allocated memory */
    free( p_aout->buffer );
    free( p_aout->s32_buffer );

    /* Destroy the condition and mutex locks */
    for ( i_index = 0; i_index < AOUT_MAX_FIFOS; i_index++ )
    {
        vlc_mutex_destroy( &p_aout->fifo[i_index].data_lock );
        vlc_cond_destroy( &p_aout->fifo[i_index].data_wait );
    }
    vlc_mutex_destroy( &p_aout->fifos_lock );
    
    /* Free the plugin */
    p_aout->pf_close( p_aout );

    /* Release the aout module */
    module_Unneed( p_aout->p_module );

    /* Free structure */
    vlc_object_destroy( p_aout );
}

