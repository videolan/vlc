/*****************************************************************************
 * idctaltivec.c : Altivec IDCT module
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: idctaltivec.c,v 1.9 2001/07/11 02:01:04 sam Exp $
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

#define MODULE_NAME idctaltivec

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"                                              /* TestCPU() */

#include "video.h"
#include "video_output.h"

#include "video_decoder.h"

#include "modules.h"
#include "modules_inner.h"

#include "vdec_block.h"
#include "vdec_idct.h"

#include "idctaltivec.h"

#include "modules_export.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void idct_getfunctions( function_list_t * p_function_list );
static int  idct_Probe      ( probedata_t *p_data );
static void vdec_NormScan   ( u8 ppi_scan[2][64] );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for Altivec IDCT module" )
    ADD_COMMENT( "Ha, ha -- nothing to configure yet" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_IDCT;
    p_module->psz_longname = "Altivec IDCT module";
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    idct_getfunctions( &p_module->p_functions->idct );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/* Following functions are local */

/*****************************************************************************
 * Functions exported as capabilities.
 *****************************************************************************/
static void idct_getfunctions( function_list_t * p_function_list )
{
    p_function_list->pf_probe = idct_Probe;
#define F p_function_list->functions.idct
    F.pf_idct_init = _M( vdec_InitIDCT );
    F.pf_sparse_idct = _M( vdec_SparseIDCT );
    F.pf_idct = _M( vdec_IDCT );
    F.pf_norm_scan = vdec_NormScan;
    F.pf_decode_init = _M( vdec_InitDecode );
    F.pf_decode_mb_c = _M( vdec_DecodeMacroblockC );
    F.pf_decode_mb_bw = _M( vdec_DecodeMacroblockBW );
#undef F
}

/*****************************************************************************
 * idct_Probe: return a preference score
 *****************************************************************************/
static int idct_Probe( probedata_t *p_data )
{
    if( !TestCPU( CPU_CAPABILITY_ALTIVEC ) )
    {
        return( 0 );
    }

    if( TestMethod( IDCT_METHOD_VAR, "idctaltivec" )
         || TestMethod( IDCT_METHOD_VAR, "altivec" ) )
    {
        return( 999 );
    }

    /* The Altivec iDCT is deactivated until it really works */
    return( 0 /* 200 */ );
}

/*****************************************************************************
 * vdec_NormScan : Soon, transpose
 *****************************************************************************/
static void vdec_NormScan( u8 ppi_scan[2][64] )
{
}

/*****************************************************************************
 * vdec_IDCT :
 *****************************************************************************/
void _M( vdec_IDCT )( vdec_thread_t * p_vdec, dctelem_t * p_block,
                int i_idontcare )
{
    IDCT( p_block, p_block );
}

