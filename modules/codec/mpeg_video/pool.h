/*****************************************************************************
 * vpar_pool.h : video parser/video decoders communication
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: pool.h,v 1.1 2002/08/04 17:23:42 sam Exp $
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
 * vdec_pool_t
 *****************************************************************************
 * This structure is used for the communication between the parser and the
 * decoders.
 *****************************************************************************/
typedef struct vdec_pool_s vdec_pool_t;

struct vdec_pool_s
{
    /* Locks */
    vlc_mutex_t         lock;                         /* Structure data lock */
    vlc_cond_t          wait_empty;      /* The parser blocks there when all
                                          * decoder threads are busy         */
    vlc_cond_t          wait_undecoded; /* The decoders block there when no
                                         * macroblock has been given by the
                                         * parser */

    /* Video decoder threads */
    vdec_thread_t **    pp_vdec;           /* Array of video decoder threads */
    int                 i_smp;     /* Number of symmetrical decoder threads,
                                    * hence size of the pp_vdec, p_macroblocks
                                    * and pp_new_macroblocks array */

    /* Macroblocks */
    macroblock_t *      p_macroblocks;

    /* Empty macroblocks */
    macroblock_t **     pp_empty_macroblocks;           /* Empty macroblocks */
    int                 i_index_empty;              /* Last empty macroblock */

    /* Undecoded macroblocks, read by the decoders */
    macroblock_t **     pp_new_macroblocks;         /* Undecoded macroblocks */
    int                 i_index_new;            /* Last undecoded macroblock */

    /* Undecoded macroblock, used when the parser and the decoder share the
     * same thread */
    macroblock_t        mb;
    vdec_thread_t *     p_vdec;                        /* Fake video decoder */

    /* Pointers to usual pool functions */
    void             (* pf_wait_pool) ( vdec_pool_t * );
    macroblock_t *   (* pf_new_mb) ( vdec_pool_t * );
    void             (* pf_free_mb) ( vdec_pool_t *, macroblock_t * );
    void             (* pf_decode_mb) ( vdec_pool_t *, macroblock_t * );

    /* Pointer to the decoding function - used for B&W switching */
    void             (* pf_vdec_decode) ( struct vdec_thread_s *,
                                          macroblock_t * );
    vlc_bool_t          b_bw;                      /* Current value for B&W */

    /* Access to the plug-ins needed by the video decoder thread */
    void ( * pf_idct_init )   ( void ** );
    void ( * ppppf_motion[2][2][4] ) ( yuv_data_t *, yuv_data_t *,
                                       int, int );

    struct vpar_thread_s * p_vpar;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void vpar_InitPool  ( struct vpar_thread_s * );
void vpar_SpawnPool ( struct vpar_thread_s * );
void vpar_EndPool   ( struct vpar_thread_s * );

/*****************************************************************************
 * vpar_GetMacroblock: In a vdec thread, get the next available macroblock
 *****************************************************************************/
static inline macroblock_t * vpar_GetMacroblock( vdec_pool_t * p_pool,
                                                 volatile vlc_bool_t * pb_die )
{
    macroblock_t *  p_mb;

    vlc_mutex_lock( &p_pool->lock );
    while( p_pool->i_index_new == 0 && !*pb_die )
    {
        vlc_cond_wait( &p_pool->wait_undecoded, &p_pool->lock );
    }

    if( *pb_die )
    {
        vlc_mutex_unlock( &p_pool->lock );
        return( NULL );
    }

    p_mb = p_pool->pp_new_macroblocks[ --p_pool->i_index_new ];
    vlc_mutex_unlock( &p_pool->lock );
    return( p_mb );
}

