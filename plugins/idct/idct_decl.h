/*****************************************************************************
 * idct_decl.h : common declarations, must be included at the very end
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: idct_decl.h,v 1.1 2001/09/05 16:07:49 massiot Exp $
 *
 * Author: Christophe Massiot <massiot@via.ecp.fr>
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
 * Final declarations
 *****************************************************************************/
static void IDCTCopy( dctelem_t * p_block, yuv_data_t * p_dest,
                     int i_stride, void * p_unused, int i_unused )
{
    IDCT( p_block );
    CopyBlock( p_block, p_dest, i_stride );
}

static void IDCTAdd( dctelem_t * p_block, yuv_data_t * p_dest,
                     int i_stride, void * p_unused, int i_unused )
{
    IDCT( p_block );
    AddBlock( p_block, p_dest, i_stride );
}

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void idct_getfunctions( function_list_t * p_function_list )
{
    p_function_list->pf_probe = idct_Probe;
#define F p_function_list->functions.idct
    F.pf_idct_init = InitIDCT;
    F.pf_norm_scan = NormScan;
    F.pf_sparse_idct_add = SparseIDCTAdd;
    F.pf_sparse_idct_copy = SparseIDCTCopy;
    F.pf_idct_add = IDCTAdd;
    F.pf_idct_copy = IDCTCopy;
#undef F
}

