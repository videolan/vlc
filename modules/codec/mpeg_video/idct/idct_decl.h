/*****************************************************************************
 * idct_decl.h : common declarations, must be included at the very end
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: idct_decl.h,v 1.1 2002/08/04 17:23:42 sam Exp $
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

static void * IDCTFunctions[] =
    { InitIDCT, NormScan, SparseIDCTAdd, SparseIDCTCopy, IDCTAdd, IDCTCopy };

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    p_this->p_private = IDCTFunctions;
    return VLC_SUCCESS;
}

