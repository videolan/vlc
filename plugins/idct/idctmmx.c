/*****************************************************************************
 * idctmmx.c : MMX IDCT module
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: idctmmx.c,v 1.2 2001/01/15 06:18:23 sam Exp $
 *
 * Authors: Gaël Hendryckx <jimmy@via.ecp.fr>
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

#define MODULE_NAME idctmmx

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "video.h"
#include "video_output.h"

#include "video_decoder.h"

#include "modules.h"
#include "modules_inner.h"

#include "idct.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void idct_getfunctions( function_list_t * p_function_list );

static int  idct_Probe      ( probedata_t *p_data );
static void vdec_InitIDCT   ( vdec_thread_t * p_vdec);
       void vdec_SparseIDCT ( vdec_thread_t * p_vdec, dctelem_t * p_block,
                              int i_sparse_pos);
       void vdec_IDCT       ( vdec_thread_t * p_vdec, dctelem_t * p_block,
                              int i_idontcare );


/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for MMX IDCT module" )
    ADD_COMMENT( "Ha, ha -- nothing to configure yet" )
MODULE_CONFIG_END

/*****************************************************************************
 * InitModule: get the module structure and configuration.
 *****************************************************************************
 * We have to fill psz_name, psz_longname and psz_version. These variables
 * will be strdup()ed later by the main application because the module can
 * be unloaded later to save memory, and we want to be able to access this
 * data even after the module has been unloaded.
 *****************************************************************************/
int InitModule( module_t * p_module )
{
    p_module->psz_name = MODULE_STRING;
    p_module->psz_longname = "MMX IDCT module";
    p_module->psz_version = VERSION;

    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_IDCT;

    return( 0 );
}

/*****************************************************************************
 * ActivateModule: set the module to an usable state.
 *****************************************************************************
 * This function fills the capability functions and the configuration
 * structure. Once ActivateModule() has been called, the i_usage can
 * be set to 0 and calls to NeedModule() be made to increment it. To unload
 * the module, one has to wait until i_usage == 0 and call DeactivateModule().
 *****************************************************************************/
int ActivateModule( module_t * p_module )
{
    p_module->p_functions = malloc( sizeof( module_functions_t ) );
    if( p_module->p_functions == NULL )
    {
        return( -1 );
    }

    idct_getfunctions( &p_module->p_functions->idct );

    p_module->p_config = p_config;

    return( 0 );
}

/*****************************************************************************
 * DeactivateModule: make sure the module can be unloaded.
 *****************************************************************************
 * This function must only be called when i_usage == 0. If it successfully
 * returns, i_usage can be set to -1 and the module unloaded. Be careful to
 * lock usage_lock during the whole process.
 *****************************************************************************/
int DeactivateModule( module_t * p_module )
{
    free( p_module->p_functions );

    return( 0 );
}

/* Following functions are local */

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void idct_getfunctions( function_list_t * p_function_list )
{
    p_function_list->pf_probe = idct_Probe;
    p_function_list->functions.idct.pf_init = vdec_InitIDCT;
    p_function_list->functions.idct.pf_sparse_idct = vdec_SparseIDCT;
    p_function_list->functions.idct.pf_idct = vdec_IDCT;
}

/*****************************************************************************
 * idct_Probe: return a preference score
 *****************************************************************************/
static int idct_Probe( probedata_t *p_data )
{
    /* This plugin always works */
    return( 100 );
}

/*****************************************************************************
 * vdec_InitIDCT : initialize datas for vdec_SparceIDCT
 *****************************************************************************/
static void vdec_InitIDCT (vdec_thread_t * p_vdec)
{
    int i;

    dctelem_t * p_pre = p_vdec->p_pre_idct;
    memset( p_pre, 0, 64*64*sizeof(dctelem_t) );

    for( i=0 ; i < 64 ; i++ )
    {
        p_pre[i*64+i] = 1 << SPARSE_SCALE_FACTOR;
        vdec_IDCT( p_vdec, &p_pre[i*64], 0) ;
    }
    return;
}

