/*****************************************************************************
 * vpar_pool.c : management of the pool of decoder threads
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: vpar_pool.c,v 1.9 2002/06/01 12:32:00 sam Exp $
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
 * Preamble
 *****************************************************************************/
#include <string.h>                                    /* memcpy(), memset() */
#include <stdlib.h>                                             /* realloc() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>

#include "vdec_ext-plugins.h"
#include "vpar_pool.h"
#include "video_parser.h"
#include "video_decoder.h"

/*
 * Local prototypes
 */
static void WaitDummy( vdec_pool_t * p_pool );
static void WaitPool( vdec_pool_t * p_pool );
static void FreeMacroblockDummy( vdec_pool_t * p_pool, macroblock_t * p_mb );
static void FreeMacroblockPool( vdec_pool_t * p_pool, macroblock_t * p_mb );
static macroblock_t * NewMacroblockDummy( vdec_pool_t * p_pool );
static macroblock_t * NewMacroblockPool( vdec_pool_t * p_pool );
static void DecodeMacroblockDummy( vdec_pool_t * p_pool, macroblock_t * p_mb );
static void DecodeMacroblockPool( vdec_pool_t * p_pool, macroblock_t * p_mb );

/*****************************************************************************
 * vpar_InitPool: Initializes the pool structure
 *****************************************************************************/
void vpar_InitPool( vpar_thread_t * p_vpar )
{
    int j;

    /* Initialize mutex and cond. */
    vlc_mutex_init( p_vpar->p_fifo, &p_vpar->pool.lock );
    vlc_cond_init( &p_vpar->pool.wait_empty );
    vlc_cond_init( &p_vpar->pool.wait_undecoded );

    /* Spawn optional video decoder threads. */
    p_vpar->pool.i_smp = 0;
    p_vpar->pool.pp_vdec = NULL;
    p_vpar->pool.p_macroblocks = NULL;
    p_vpar->pool.pp_empty_macroblocks = NULL;
    p_vpar->pool.pp_new_macroblocks = NULL;
    p_vpar->pool.p_vpar = p_vpar;
    vpar_SpawnPool( p_vpar );

    /* Initialize fake video decoder structure (used when
     * decoder == parser). */
    p_vpar->pool.p_vdec = vlc_object_create( p_vpar->p_fifo,
                                             sizeof(vdec_thread_t) );
    if ( p_vpar->pool.p_vdec == NULL )
    {
        msg_Err( p_vpar->p_fifo, "out of memory" );
        p_vpar->p_fifo->b_error = 1;
        return;
    }
    p_vpar->pool.p_vdec->p_pool = &p_vpar->pool;
    vdec_InitThread( p_vpar->pool.p_vdec );

    vlc_object_attach( p_vpar->pool.p_vdec, p_vpar->p_fifo );

    for( j = 0; j < 12; j++ )
    {
        p_vpar->pool.mb.p_idcts[j].pi_block =
            vlc_memalign( &p_vpar->pool.mb.p_idcts[j].pi_block_orig,
                          16, 64 * sizeof(dctelem_t) );
    }
}

/*****************************************************************************
 * vpar_SpawnPool: Create and cancel video decoder threads at any time
 *****************************************************************************
 * This function is called on startup, and everytime the user changes the
 * number of threads to launch. Please note that *all* decoder threads must
 * be idle during this operation, which only happens at the end of
 * PictureHeader().
 *****************************************************************************/
void vpar_SpawnPool( vpar_thread_t * p_vpar )
{
    int                 i_new_smp;
    stream_ctrl_t *     p_control;

    p_control = p_vpar->p_fifo->p_stream_ctrl;
    vlc_mutex_lock( &p_control->control_lock );
    i_new_smp = p_control->i_smp;
    vlc_mutex_unlock( &p_control->control_lock );

    /* FIXME: No error check because I'm tired. Come back later... */

    /* No need to lock p_vpar->pool, since decoders MUST be idle here. */
    if( p_vpar->pool.i_smp != i_new_smp )
    {
        int i;

        if( p_vpar->pool.i_smp > i_new_smp )
        {
            /* The user reduces the number of threads. */

            for( i = p_vpar->pool.i_smp - 1; i >= i_new_smp; i-- )
            {
                int j;

                vlc_object_unlink_all( p_vpar->pool.pp_vdec[i] );
                vdec_DestroyThread( p_vpar->pool.pp_vdec[i] );

                for( j = 0; j < 12; j++ )
                {
                    free( p_vpar->pool.p_macroblocks[i].p_idcts[j].pi_block_orig );
                }
            }

            p_vpar->pool.pp_vdec = realloc( p_vpar->pool.pp_vdec,
                                            i_new_smp * sizeof(vdec_thread_t *) );
            p_vpar->pool.p_macroblocks = realloc( p_vpar->pool.p_macroblocks,
                                            i_new_smp * sizeof(macroblock_t) );
            p_vpar->pool.pp_empty_macroblocks = realloc( p_vpar->pool.pp_empty_macroblocks,
                                            i_new_smp * sizeof(macroblock_t *) );
            p_vpar->pool.i_index_empty = i_new_smp;
            p_vpar->pool.pp_new_macroblocks = realloc( p_vpar->pool.pp_new_macroblocks,
                                            i_new_smp * sizeof(macroblock_t *) );
            p_vpar->pool.i_index_new = 0;
        }
        else
        {
            /* The user raises the number of threads. */

            p_vpar->pool.pp_vdec = realloc( p_vpar->pool.pp_vdec,
                                            i_new_smp * sizeof(vdec_thread_t *) );
            p_vpar->pool.p_macroblocks = realloc( p_vpar->pool.p_macroblocks,
                                            i_new_smp * sizeof(macroblock_t) );
            p_vpar->pool.pp_empty_macroblocks = realloc( p_vpar->pool.pp_empty_macroblocks,
                                            i_new_smp * sizeof(macroblock_t *) );
            p_vpar->pool.i_index_empty = i_new_smp;
            p_vpar->pool.pp_new_macroblocks = realloc( p_vpar->pool.pp_new_macroblocks,
                                            i_new_smp * sizeof(macroblock_t *) );
            p_vpar->pool.i_index_new = 0;

            for( i = p_vpar->pool.i_smp; i < i_new_smp ; i++ )
            {
                int j;

                for( j = 0; j < 12; j++ )
                {
                    p_vpar->pool.p_macroblocks[i].p_idcts[j].pi_block =
                        vlc_memalign( &p_vpar->pool.p_macroblocks[i].p_idcts[j].pi_block_orig,
                                      16, 64 * sizeof(dctelem_t) );
                }

                p_vpar->pool.pp_vdec[i] = vdec_CreateThread( &p_vpar->pool );
                vlc_object_attach( p_vpar->pool.pp_vdec[i], p_vpar->p_fifo );
            }

        }
        for( i = 0; i < i_new_smp; i++ )
        {
            p_vpar->pool.pp_empty_macroblocks[i] =
                                    &p_vpar->pool.p_macroblocks[i];
        }
        p_vpar->pool.i_smp = i_new_smp;
    }

    if( i_new_smp )
    {
        /* We have at least one decoder thread. */
        p_vpar->pool.pf_wait_pool = WaitPool;
        p_vpar->pool.pf_new_mb = NewMacroblockPool;
        p_vpar->pool.pf_free_mb = FreeMacroblockPool;
        p_vpar->pool.pf_decode_mb = DecodeMacroblockPool;
    }
    else
    {
        /* No decoder pool. */
        p_vpar->pool.pf_wait_pool = WaitDummy;
        p_vpar->pool.pf_new_mb = NewMacroblockDummy;
        p_vpar->pool.pf_free_mb = FreeMacroblockDummy;
        p_vpar->pool.pf_decode_mb = DecodeMacroblockDummy;
    }
}

/*****************************************************************************
 * vpar_EndPool: Releases the pool structure
 *****************************************************************************/
void vpar_EndPool( vpar_thread_t * p_vpar )
{
    int i;

    for( i = 0; i < 12; i++ )
    {
        free( p_vpar->pool.mb.p_idcts[i].pi_block_orig );
    }

    for( i = 0; i < p_vpar->pool.i_smp; i++ )
    {
        int j;

        vlc_object_unlink_all( p_vpar->pool.pp_vdec[i] );
        vdec_DestroyThread( p_vpar->pool.pp_vdec[i] );

        for( j = 0; j < 12; j++ )
        {
            free( p_vpar->pool.p_macroblocks[i].p_idcts[j].pi_block_orig );
        }
    }

    if( p_vpar->pool.i_smp )
    {
        free( p_vpar->pool.pp_vdec );
        free( p_vpar->pool.p_macroblocks );
        free( p_vpar->pool.pp_new_macroblocks );
    }

    /* Free fake video decoder (used when parser == decoder). */
    vlc_object_unlink_all( p_vpar->pool.p_vdec );
    vdec_EndThread( p_vpar->pool.p_vdec );
    vlc_object_destroy( p_vpar->pool.p_vdec );

    /* Destroy lock and cond. */
    vlc_mutex_destroy( &p_vpar->pool.lock );
    vlc_cond_destroy( &p_vpar->pool.wait_empty );
    vlc_cond_destroy( &p_vpar->pool.wait_undecoded );
}

/*****************************************************************************
 * WaitPool: Wait until all decoders are idle
 *****************************************************************************/
static void WaitPool( vdec_pool_t * p_pool )
{
    vlc_mutex_lock( &p_pool->lock );
    while( p_pool->i_index_empty != p_pool->i_smp )
    {
        vlc_cond_wait( &p_pool->wait_empty, &p_pool->lock );
    }
    vlc_mutex_unlock( &p_pool->lock );
}

/*****************************************************************************
 * WaitDummy: Placeholder used when parser == decoder
 *****************************************************************************/
static void WaitDummy( vdec_pool_t * p_pool )
{
}

/*****************************************************************************
 * NewMacroblockPool: Get an empty macroblock from the decoder pool
 *****************************************************************************/
static macroblock_t * NewMacroblockPool( vdec_pool_t * p_pool )
{
    macroblock_t *  p_mb;

    vlc_mutex_lock( &p_pool->lock );
    while( p_pool->i_index_empty == 0 )
    {
        vlc_cond_wait( &p_pool->wait_empty, &p_pool->lock );
    }
    p_mb = p_pool->pp_empty_macroblocks[ --p_pool->i_index_empty ];
    vlc_mutex_unlock( &p_pool->lock );
    return( p_mb );
}

/*****************************************************************************
 * NewMacroblockDummy: Placeholder used when parser == decoder
 *****************************************************************************/
static macroblock_t * NewMacroblockDummy( vdec_pool_t * p_pool )
{
    return( &p_pool->mb );
}

/*****************************************************************************
 * FreeMacroblockPool: Free a macroblock
 *****************************************************************************/
static void FreeMacroblockPool( vdec_pool_t * p_pool, macroblock_t * p_mb )
{
    vlc_mutex_lock( &p_pool->lock );
    p_pool->pp_empty_macroblocks[ p_pool->i_index_empty++ ] = p_mb;
    vlc_cond_signal( &p_pool->wait_empty );
    vlc_mutex_unlock( &p_pool->lock );
}

/*****************************************************************************
 * FreeMacroblockDummy: Placeholder used when parser == decoder
 *****************************************************************************/
static void FreeMacroblockDummy( vdec_pool_t * p_pool, macroblock_t * p_mb )
{
}

/*****************************************************************************
 * DecodeMacroblockPool: Send a macroblock to a vdec thread
 *****************************************************************************/
static void DecodeMacroblockPool( vdec_pool_t * p_pool, macroblock_t * p_mb )
{
    vlc_mutex_lock( &p_pool->lock );
    /* The undecoded macroblock LIFO cannot be full, because
     * #macroblocks == size of the LIFO */
    p_pool->pp_new_macroblocks[ p_pool->i_index_new++ ] = p_mb;
    vlc_cond_signal( &p_pool->wait_undecoded );
    vlc_mutex_unlock( &p_pool->lock );
}

/*****************************************************************************
 * DecodeMacroblockDummy: Placeholder used when parser == decoder
 *****************************************************************************/
static void DecodeMacroblockDummy( vdec_pool_t * p_pool, macroblock_t * p_mb )
{
    p_pool->pf_vdec_decode( p_pool->p_vdec, p_mb );
}

