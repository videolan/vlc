/*****************************************************************************
 * video_parser.h : video parser thread
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: video_parser.h,v 1.3 2001/01/13 12:57:20 sam Exp $
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
 * Requires:
 *  "config.h"
 *  "common.h"
 *  "mtime.h"
 *  "threads.h"
 *  "input.h"
 *  "video.h"
 *  "video_output.h"
 *  "decoder_fifo.h"
 *  "video_fifo.h"
 *  "vpar_headers.h"
 *****************************************************************************/

/*****************************************************************************
 * video_fifo_t
 *****************************************************************************
 * This rotative FIFO contains undecoded macroblocks that are to be decoded
 *****************************************************************************/
struct vpar_thread_s;

typedef struct video_fifo_s
{
#ifdef VDEC_SMP
    vlc_mutex_t         lock;                              /* fifo data lock */
    vlc_cond_t          wait;              /* fifo data conditional variable */

    /* buffer is an array of undec_picture_t pointers */
    macroblock_t *      buffer[VFIFO_SIZE + 1];
    int                 i_start;
    int                 i_end;
#else
    macroblock_t        buffer;
#endif

    struct vpar_thread_s *      p_vpar;
} video_fifo_t;

/*****************************************************************************
 * video_buffer_t
 *****************************************************************************
 * This structure enables the parser to maintain a list of free
 * macroblock_t structures
 *****************************************************************************/
#ifdef VDEC_SMP
typedef struct video_buffer_s
{
    vlc_mutex_t         lock;                            /* buffer data lock */

    macroblock_t        p_macroblocks[VFIFO_SIZE + 1];
    macroblock_t *      pp_mb_free[VFIFO_SIZE+1];          /* this is a LIFO */
    int                 i_index;
} video_buffer_t;
#endif

/*****************************************************************************
 * vpar_thread_t: video parser thread descriptor
 *****************************************************************************
 * XXX??
 *****************************************************************************/
typedef struct vpar_thread_s
{
    /* Thread properties and locks */
    vlc_thread_t        thread_id;                /* id for thread functions */

    /* Thread configuration */
    /* XXX?? */
//    int *pi_status;


    /* Input properties */
    decoder_fifo_t *    p_fifo;                            /* PES input fifo */
    bit_stream_t        bit_stream;
    vdec_config_t *     p_config;
    /* Bitstream context */
    mtime_t             next_pts, next_dts;

    /* Output properties */
    vout_thread_t *     p_vout;                       /* video output thread */

    /* Decoder properties */
    struct vdec_thread_s *      pp_vdec[NB_VDEC];
    video_fifo_t                vfifo;
#ifdef VDEC_SMP
    video_buffer_t              vbuffer;
#endif

    /* Parser properties */
    sequence_t              sequence;
    picture_parsing_t       picture;
    macroblock_parsing_t    mb;
    video_synchro_t         synchro;

    /* Lookup tables */
#ifdef MPEG2_COMPLIANT
    s16                     pi_crop_buf[8192];
    s16 *                   pi_crop;
#endif
    lookup_t                pl_mb_addr_inc[2048];    /* for macroblock
                                                        address increment */
    /* tables for macroblock types 0=P 1=B */
    lookup_t                ppl_mb_type[2][64];
    /* table for coded_block_pattern */
    lookup_t *              pl_coded_pattern;
    /* variable length codes for the structure dct_dc_size for intra blocks */
    lookup_t *              pppl_dct_dc_size[2][2];
    /* Structure to store the tables B14 & B15 (ISO/CEI 13818-2 B.4) */
    dct_lookup_t            ppl_dct_coef[2][16384];

    /* IDCT plugin used and shortcuts to access its capabilities */
    struct module_s *       p_module;
    idct_init_t             pf_init;
    f_idct_t                pf_sparse_idct;
    f_idct_t                pf_idct;

#ifdef STATS
    /* Statistics */
    count_t         c_loops;                              /* number of loops */
    count_t         c_sequences;                      /* number of sequences */
    count_t         pc_pictures[4]; /* number of (coding_type) pictures read */
    count_t         pc_decoded_pictures[4];       /* number of (coding_type)
                                                   *        pictures decoded */
    count_t         pc_malformed_pictures[4];  /* number of pictures trashed
                                                * during parsing             */
#endif
} vpar_thread_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/* Thread management functions */
vlc_thread_t vpar_CreateThread       ( vdec_config_t * );

/*****************************************************************************
 * NextStartCode : Find the next start code
 *****************************************************************************/
static __inline__ void NextStartCode( bit_stream_t * p_bit_stream )
{
    /* Re-align the buffer on an 8-bit boundary */
    RealignBits( p_bit_stream );

    while( ShowBits( p_bit_stream, 24 ) != 0x01L
            && !p_bit_stream->p_decoder_fifo->b_die )
    {
        RemoveBits( p_bit_stream, 8 );
    }
}

/*****************************************************************************
 * LoadQuantizerScale
 *****************************************************************************
 * Quantizer scale factor (ISO/IEC 13818-2 7.4.2.2)
 *****************************************************************************/
static __inline__ void LoadQuantizerScale( struct vpar_thread_s * p_vpar )
{
    /* Quantization coefficient table */
    static u8   ppi_quantizer_scale[3][32] =
    {
        /* MPEG-2 */
        {
            /* q_scale_type */
            /* linear */
            0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,
            32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62
        },
        {
            /* non-linear */
            0, 1, 2, 3, 4, 5, 6, 7, 8, 10,12,14,16,18,20, 22,
            24,28,32,36,40,44,48,52,56,64,72,80,88,96,104,112
        },
        /* MPEG-1 */
        {
            0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
            16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
        }
    };

    p_vpar->mb.i_quantizer_scale = ppi_quantizer_scale
           [(!p_vpar->sequence.b_mpeg2 << 1) | p_vpar->picture.b_q_scale_type]
           [GetBits( &p_vpar->bit_stream, 5 )];
}

