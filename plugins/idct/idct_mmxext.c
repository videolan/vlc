/*****************************************************************************
 * idct_mmxext.c : MMX EXT IDCT functions
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: idct_mmxext.c,v 1.1 2001/01/16 13:26:46 sam Exp $
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
#include "defs.h"

#include <stdlib.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "video.h"
#include "video_output.h"

#include "video_decoder.h"

#include "idct.h"

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
void vdec_InitIDCT   ( vdec_thread_t * p_vdec);
void vdec_SparseIDCT ( vdec_thread_t * p_vdec, dctelem_t * p_block,
                       int i_sparse_pos);
void vdec_IDCT       ( vdec_thread_t * p_vdec, dctelem_t * p_block,
                       int i_idontcare );


/*****************************************************************************
 * vdec_InitIDCT : initialize datas for vdec_SparseIDCT
 *****************************************************************************/
void vdec_InitIDCT (vdec_thread_t * p_vdec)
{
    return;
}

/*****************************************************************************
 * vdec_IDCT : IDCT function for normal matrices
 *****************************************************************************/
void vdec_IDCT( vdec_thread_t * p_vdec, dctelem_t * p_block,
                       int i_idontcare )
{
    return;
}

/*****************************************************************************
 * vdec_SparseIDCT : IDCT function for sparse matrices
 *****************************************************************************/
void vdec_SparseIDCT( vdec_thread_t * p_vdec, dctelem_t * p_block,
                      int i_sparse_pos )
{
    return;
}

