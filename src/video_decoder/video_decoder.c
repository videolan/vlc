/*****************************************************************************
 * video_decoder.c : video decoder thread
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

/* FIXME: passer en terminate/destroy avec les signaux supplémentaires ?? */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>                                                /* free() */
#include <unistd.h>                                              /* getpid() */
#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                          /* for input.h */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "intf_msg.h"

#include "input.h"
#include "decoder_fifo.h"
#include "video.h"
#include "video_output.h"

#include "vdec_idct.h"
#include "video_decoder.h"
#include "vdec_motion.h"

#include "vpar_blocks.h"
#include "vpar_headers.h"
#include "vpar_synchro.h"
#include "video_parser.h"
#include "video_fifo.h"

/*
 * Local prototypes
 */
#ifdef VDEC_SMP
static int      vdec_InitThread     ( vdec_thread_t *p_vdec );
static void     vdec_DecodeMacroblock( vdec_thread_t *p_vdec,
                                       macroblock_t * p_mb );
#endif
static void     RunThread           ( vdec_thread_t *p_vdec );
static void     ErrorThread         ( vdec_thread_t *p_vdec );
static void     EndThread           ( vdec_thread_t *p_vdec );

/*****************************************************************************
 * vdec_CreateThread: create a video decoder thread
 *****************************************************************************
 * This function creates a new video decoder thread, and returns a pointer
 * to its description. On error, it returns NULL.
 * Following configuration properties are used:
 * XXX??
 *****************************************************************************/
vdec_thread_t * vdec_CreateThread( vpar_thread_t *p_vpar /*, int *pi_status */ )
{
    vdec_thread_t *     p_vdec;

    intf_DbgMsg("vdec debug: creating video decoder thread\n");

    /* Allocate the memory needed to store the thread's structure */
    if ( (p_vdec = (vdec_thread_t *)malloc( sizeof(vdec_thread_t) )) == NULL )
    {
        intf_ErrMsg("vdec error: not enough memory for vdec_CreateThread() to create the new thread\n");
        return( NULL );
    }

    /*
     * Initialize the thread properties
     */
    p_vdec->b_die = 0;
    p_vdec->b_error = 0;

    /*
     * Initialize the parser properties
     */
    p_vdec->p_vpar = p_vpar;

    /* Spawn the video decoder thread */
    if ( vlc_thread_create(&p_vdec->thread_id, "video decoder",
         (vlc_thread_func_t)RunThread, (void *)p_vdec) )
    {
        intf_ErrMsg("vdec error: can't spawn video decoder thread\n");
        free( p_vdec );
        return( NULL );
    }

    intf_DbgMsg("vdec debug: video decoder thread (%p) created\n", p_vdec);
    return( p_vdec );
}

/*****************************************************************************
 * vdec_DestroyThread: destroy a video decoder thread
 *****************************************************************************
 * Destroy and terminate thread. This function will return 0 if the thread could
 * be destroyed, and non 0 else. The last case probably means that the thread
 * was still active, and another try may succeed.
 *****************************************************************************/
void vdec_DestroyThread( vdec_thread_t *p_vdec /*, int *pi_status */ )
{
    intf_DbgMsg("vdec debug: requesting termination of video decoder thread %p\n", p_vdec);

    /* Ask thread to kill itself */
    p_vdec->b_die = 1;

#ifdef VDEC_SMP
    /* Make sure the decoder thread leaves the vpar_GetMacroblock() function */
    vlc_mutex_lock( &(p_vdec->p_vpar->vfifo.lock) );
    vlc_cond_signal( &(p_vdec->p_vpar->vfifo.wait) );
    vlc_mutex_unlock( &(p_vdec->p_vpar->vfifo.lock) );
#endif

    /* Waiting for the decoder thread to exit */
    /* Remove this as soon as the "status" flag is implemented */
    vlc_thread_join( p_vdec->thread_id );
}

/* following functions are local */

/*****************************************************************************
 * vdec_InitThread: initialize video decoder thread
 *****************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 *****************************************************************************/
#ifdef VDEC_SMP
static int vdec_InitThread( vdec_thread_t *p_vdec )
#else
int vdec_InitThread( vdec_thread_t *p_vdec )
#endif
{
    int i_dummy;

    intf_DbgMsg("vdec debug: initializing video decoder thread %p\n", p_vdec);

    /* Initialize other properties */
#ifdef STATS
    p_vdec->c_loops = 0;
    p_vdec->c_idle_loops = 0;
    p_vdec->c_decoded_pictures = 0;
    p_vdec->c_decoded_i_pictures = 0;
    p_vdec->c_decoded_p_pictures = 0;
    p_vdec->c_decoded_b_pictures = 0;
#endif

    /* Init crop table */
    p_vdec->pi_crop = p_vdec->pi_crop_buf + (VDEC_CROPRANGE >> 1);
    for( i_dummy = -(VDEC_CROPRANGE >> 1); i_dummy < 0; i_dummy++ )
    {
        p_vdec->pi_crop[i_dummy] = 0;
    }
    for( ; i_dummy < 255; i_dummy ++ )
    {
        p_vdec->pi_crop[i_dummy] = i_dummy;
    }
    for( ; i_dummy < (VDEC_CROPRANGE >> 1) -1; i_dummy++ )
    {
        p_vdec->pi_crop[i_dummy] = 255;
    }

    /* Mark thread as running and return */
    intf_DbgMsg("vdec debug: InitThread(%p) succeeded\n", p_vdec);
    return( 0 );
}

/*****************************************************************************
 * ErrorThread: RunThread() error loop
 *****************************************************************************
 * This function is called when an error occured during thread main's loop. The
 * thread can still receive feed, but must be ready to terminate as soon as
 * possible.
 *****************************************************************************/
static void ErrorThread( vdec_thread_t *p_vdec )
{
    macroblock_t *       p_mb;

    /* Wait until a `die' order */
    while( !p_vdec->b_die )
    {
        p_mb = vpar_GetMacroblock( &p_vdec->p_vpar->vfifo );
        vpar_DestroyMacroblock( &p_vdec->p_vpar->vfifo, p_mb );
    }
}

/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessfull
 * initialization.
 *****************************************************************************/
static void EndThread( vdec_thread_t *p_vdec )
{
    intf_DbgMsg("vdec debug: EndThread(%p)\n", p_vdec);
}

/*****************************************************************************
 * AddBlock : add a block
 *****************************************************************************/
#ifndef HAVE_MMX
static __inline__ void AddBlock( vdec_thread_t * p_vdec, dctelem_t * p_block,
                                 yuv_data_t * p_data, int i_incr )
{
    int i_x, i_y;

    for( i_y = 0; i_y < 8; i_y++ )
    {
        for( i_x = 0; i_x < 8; i_x++ )
        {
            *p_data = p_vdec->pi_crop[*p_data + *p_block++];
            p_data++;
        }
        p_data += i_incr;
    }
}
#else
static __inline__ void AddBlock( vdec_thread_t * p_vdec, dctelem_t * p_block,
                                          yuv_data_t * p_data, int i_incr )
{
    asm __volatile__ (
            "pxor       %%mm7,%%mm7\n\t"

            "movq       (%0),%%mm1\n\t"
            "movq       %%mm1,%%mm2\n\t"
            "punpckhbw  %%mm7,%%mm1\n\t"
            "punpcklbw  %%mm7,%%mm2\n\t"
            "paddw      (%1),%%mm2\n\t"
            "paddw      8(%1),%%mm1\n\t"
            "packuswb   %%mm1,%%mm2\n\t"
            "movq       %%mm2,(%0)\n\t"
            "addl       %2,%0\n\t"

            "movq       (%0),%%mm1\n\t"
            "movq       %%mm1,%%mm2\n\t"
            "punpckhbw  %%mm7,%%mm1\n\t"
            "punpcklbw  %%mm7,%%mm2\n\t"
            "paddw      16(%1),%%mm2\n\t"
            "paddw      24(%1),%%mm1\n\t"
            "packuswb   %%mm1,%%mm2\n\t"
            "movq       %%mm2,(%0)\n\t"
            "addl       %2,%0\n\t"

            "movq       (%0),%%mm1\n\t"
            "movq       %%mm1,%%mm2\n\t"
            "punpckhbw  %%mm7,%%mm1\n\t"
            "punpcklbw  %%mm7,%%mm2\n\t"
            "paddw      32(%1),%%mm2\n\t"
            "paddw      40(%1),%%mm1\n\t"
            "packuswb   %%mm1,%%mm2\n\t"
            "movq       %%mm2,(%0)\n\t"
            "addl       %2,%0\n\t"

            "movq       (%0),%%mm1\n\t"
            "movq       %%mm1,%%mm2\n\t"
            "punpckhbw  %%mm7,%%mm1\n\t"
            "punpcklbw  %%mm7,%%mm2\n\t"
            "paddw      48(%1),%%mm2\n\t"
            "paddw      56(%1),%%mm1\n\t"
            "packuswb   %%mm1,%%mm2\n\t"
            "movq       %%mm2,(%0)\n\t"
            "addl       %2,%0\n\t"

            "movq       (%0),%%mm1\n\t"
            "movq       %%mm1,%%mm2\n\t"
            "punpckhbw  %%mm7,%%mm1\n\t"
            "punpcklbw  %%mm7,%%mm2\n\t"
            "paddw      64(%1),%%mm2\n\t"
            "paddw      72(%1),%%mm1\n\t"
            "packuswb   %%mm1,%%mm2\n\t"
            "movq       %%mm2,(%0)\n\t"
            "addl       %2,%0\n\t"

            "movq       (%0),%%mm1\n\t"
            "movq       %%mm1,%%mm2\n\t"
            "punpckhbw  %%mm7,%%mm1\n\t"
            "punpcklbw  %%mm7,%%mm2\n\t"
            "paddw      80(%1),%%mm2\n\t"
            "paddw      88(%1),%%mm1\n\t"
            "packuswb   %%mm1,%%mm2\n\t"
            "movq       %%mm2,(%0)\n\t"
            "addl       %2,%0\n\t"

            "movq       (%0),%%mm1\n\t"
            "movq       %%mm1,%%mm2\n\t"
            "punpckhbw  %%mm7,%%mm1\n\t"
            "punpcklbw  %%mm7,%%mm2\n\t"
            "paddw      96(%1),%%mm2\n\t"
            "paddw      104(%1),%%mm1\n\t"
            "packuswb   %%mm1,%%mm2\n\t"
            "movq       %%mm2,(%0)\n\t"
            "addl       %2,%0\n\t"

            "movq       (%0),%%mm1\n\t"
            "movq       %%mm1,%%mm2\n\t"
            "punpckhbw  %%mm7,%%mm1\n\t"
            "punpcklbw  %%mm7,%%mm2\n\t"
            "paddw      112(%1),%%mm2\n\t"
            "paddw      120(%1),%%mm1\n\t"
            "packuswb   %%mm1,%%mm2\n\t"
            "movq       %%mm2,(%0)\n\t"

            "emms"
             :"+r" (p_data): "r" (p_block),"r" (i_incr+8));
}
#endif


/*****************************************************************************
 * CopyBlock : copy a block
 *****************************************************************************/
#ifndef HAVE_MMX
static __inline__ void CopyBlock( vdec_thread_t * p_vdec, dctelem_t * p_block,
                                  yuv_data_t * p_data, int i_incr )
{
    int i_x, i_y;

    for( i_y = 0; i_y < 8; i_y++ )
    {
        for( i_x = 0; i_x < 8; i_x++ )
        {
            *p_data++ = p_vdec->pi_crop[*p_block++];
        }
        p_data += i_incr;
    }
}
#else
static  __inline__ void CopyBlock( vdec_thread_t * p_vdec, dctelem_t * p_block,
                                          yuv_data_t * p_data, int i_incr )
{
    asm __volatile__ (
            "movq         (%1),%%mm0\n\t"
            "packuswb   8(%1),%%mm0\n\t"
            "movq        %%mm0,(%0)\n\t"
            "addl           %2,%0\n\t"

            "movq        16(%1),%%mm0\n\t"
            "packuswb   24(%1),%%mm0\n\t"
            "movq        %%mm0,(%0)\n\t"
            "addl           %2,%0\n\t"

            "movq        32(%1),%%mm0\n\t"
            "packuswb   40(%1),%%mm0\n\t"
            "movq        %%mm0,(%0)\n\t"
            "addl           %2,%0\n\t"

            "movq        48(%1),%%mm0\n\t"
            "packuswb   56(%1),%%mm0\n\t"
            "movq        %%mm0,(%0)\n\t"
            "addl           %2,%0\n\t"

            "movq        64(%1),%%mm0\n\t"
            "packuswb   72(%1),%%mm0\n\t"
            "movq        %%mm0,(%0)\n\t"
            "addl           %2,%0\n\t"

            "movq        80(%1),%%mm0\n\t"
            "packuswb   88(%1),%%mm0\n\t"
            "movq        %%mm0,(%0)\n\t"
            "addl           %2,%0\n\t"

            "movq        96(%1),%%mm0\n\t"
            "packuswb   104(%1),%%mm0\n\t"
            "movq        %%mm0,(%0)\n\t"
            "addl           %2,%0\n\t"

            "movq        112(%1),%%mm0\n\t"
            "packuswb   120(%1),%%mm0\n\t"
            "movq        %%mm0,(%0)\n\t"
            "emms"
            :"+r" (p_data): "r" (p_block),"r" (i_incr+8));
}
#endif


/*****************************************************************************
 * vdec_DecodeMacroblock : decode a macroblock of a picture
 *****************************************************************************/
#define DECODEBLOCKS( OPBLOCK )                                         \
{                                                                       \
    int             i_b, i_mask;                                        \
                                                                        \
    i_mask = 1 << (3 + p_mb->i_chroma_nb_blocks);                       \
                                                                        \
    /* luminance */                                                     \
    for( i_b = 0; i_b < 4; i_b++, i_mask >>= 1 )                        \
    {                                                                   \
        if( p_mb->i_coded_block_pattern & i_mask )                      \
        {                                                               \
            /*                                                          \
             * Inverse DCT (ISO/IEC 13818-2 section Annex A)            \
             */                                                         \
            (p_mb->pf_idct[i_b])( p_vdec, p_mb->ppi_blocks[i_b],        \
                                  p_mb->pi_sparse_pos[i_b] );           \
                                                                        \
            /*                                                          \
             * Adding prediction and coefficient data (ISO/IEC 13818-2  \
             * section 7.6.8)                                           \
             */                                                         \
            OPBLOCK( p_vdec, p_mb->ppi_blocks[i_b],                     \
                     p_mb->p_data[i_b], p_mb->i_addb_l_stride );        \
        }                                                               \
    }                                                                   \
                                                                        \
    /* chrominance */                                                   \
    for( i_b = 4; i_b < 4 + p_mb->i_chroma_nb_blocks;                   \
         i_b++, i_mask >>= 1 )                                          \
    {                                                                   \
        if( p_mb->i_coded_block_pattern & i_mask )                      \
        {                                                               \
            /*                                                          \
             * Inverse DCT (ISO/IEC 13818-2 section Annex A)            \
             */                                                         \
            (p_mb->pf_idct[i_b])( p_vdec, p_mb->ppi_blocks[i_b],        \
                                  p_mb->pi_sparse_pos[i_b] );           \
                                                                        \
            /*                                                          \
             * Adding prediction and coefficient data (ISO/IEC 13818-2  \
             * section 7.6.8)                                           \
             */                                                         \
            OPBLOCK( p_vdec, p_mb->ppi_blocks[i_b],                     \
                     p_mb->p_data[i_b], p_mb->i_addb_c_stride );        \
        }                                                               \
    }                                                                   \
}

#ifdef VDEC_SMP
static __inline__ void vdec_DecodeMacroblock( vdec_thread_t *p_vdec, macroblock_t * p_mb )
#else
void vdec_DecodeMacroblock( vdec_thread_t *p_vdec, macroblock_t * p_mb )
#endif
{
    if( !(p_mb->i_mb_type & MB_INTRA) )
    {
        /*
         * Motion Compensation (ISO/IEC 13818-2 section 7.6)
         */
        if( p_mb->pf_motion == 0 )
        {
            intf_ErrMsg( "vdec error: pf_motion set to NULL\n" );
        }
        else
        {
            p_mb->pf_motion( p_mb );
        }

        DECODEBLOCKS( AddBlock )
    }
    else
    {
        DECODEBLOCKS( CopyBlock )
    }

    /*
     * Decoding is finished, release the macroblock and free
     * unneeded memory.
     */
    vpar_ReleaseMacroblock( &p_vdec->p_vpar->vfifo, p_mb );
}


/*****************************************************************************
 * RunThread: video decoder thread
 *****************************************************************************
 * Video decoder thread. This function does only return when the thread is
 * terminated.
 *****************************************************************************/
static void RunThread( vdec_thread_t *p_vdec )
{
    intf_DbgMsg("vdec debug: running video decoder thread (%p) (pid == %i)\n",
                p_vdec, getpid());

    /*
     * Initialize thread and free configuration
     */
    p_vdec->b_error = vdec_InitThread( p_vdec );
    if( p_vdec->b_error )
    {
        return;
    }
    p_vdec->b_run = 1;

    /*
     * Main loop - it is not executed if an error occured during
     * initialization
     */
    while( (!p_vdec->b_die) && (!p_vdec->b_error) )
    {
        macroblock_t *          p_mb;

        if( (p_mb = vpar_GetMacroblock( &p_vdec->p_vpar->vfifo )) != NULL )
        {
            vdec_DecodeMacroblock( p_vdec, p_mb );
        }
    }

    /*
     * Error loop
     */
    if( p_vdec->b_error )
    {
        ErrorThread( p_vdec );
    }

    /* End of thread */
    EndThread( p_vdec );
    p_vdec->b_run = 0;
}
