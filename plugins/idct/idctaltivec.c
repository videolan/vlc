/*****************************************************************************
 * idctaltivec.c : Altivec IDCT module
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: idctaltivec.c,v 1.13 2001/09/06 10:19:18 massiot Exp $
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
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"                                              /* TestCPU() */

#include "idct.h"

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void idct_getfunctions( function_list_t * p_function_list );
void idct_block_copy_altivec( dctelem_t *, yuv_data_t *, int, void *, int );
void idct_block_add_altivec( dctelem_t *, yuv_data_t *, int, void *, int );

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

    return( 200 );
}

/*****************************************************************************
 * Placeholders for unused functions
 *****************************************************************************/
static void NormScan( u8 ppi_scan[2][64] )
{
}

static void InitIDCT( void * p_idct_data )
{
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
    /* FIXME : it would be a nice idea to use sparse IDCT functions */
    F.pf_sparse_idct_add = idct_block_add_altivec;
    F.pf_sparse_idct_copy = idct_block_copy_altivec;
    F.pf_idct_add = idct_block_add_altivec;
    F.pf_idct_copy = idct_block_copy_altivec;
#undef F
}

