/*****************************************************************************
 * vdec_block_h: Macroblock copy functions
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: vdec_block.h,v 1.3 2001/07/17 09:48:07 massiot Exp $
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
 * Prototypes
 *****************************************************************************/
void _M( vdec_InitDecode )         ( struct vdec_thread_s *p_vdec );
void _M( vdec_DecodeMacroblockC )  ( struct vdec_thread_s *p_vdec,
                                     struct macroblock_s * p_mb );
void _M( vdec_DecodeMacroblockBW ) ( struct vdec_thread_s *p_vdec,
                                     struct macroblock_s * p_mb );
	
/*****************************************************************************
 * vdec_DecodeMacroblock : decode a macroblock of a picture
 *****************************************************************************/
#define DECODEBLOCKSC( OPBLOCK )                                        \
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
            (p_mb->pf_idct[i_b])( p_vdec->p_idct_data,                  \
                                  p_mb->ppi_blocks[i_b],                \
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
            (p_mb->pf_idct[i_b])( p_vdec->p_idct_data,                  \
                                  p_mb->ppi_blocks[i_b],                \
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

#define DECODEBLOCKSBW( OPBLOCK )                                       \
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
            (p_mb->pf_idct[i_b])( p_vdec->p_idct_data,                  \
                                  p_mb->ppi_blocks[i_b],                \
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
}

