/*****************************************************************************
 * downmix3dn.c : accelerated 3D Now! AC3 downmix module
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: downmix3dn.c,v 1.1 2001/05/15 16:19:42 sam Exp $
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

#define MODULE_NAME downmix3dn
#include "modules_inner.h"

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

#include "ac3_downmix.h"
#include "ac3_downmix_common.h"

#include "modules.h"

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static void downmix_getfunctions( function_list_t * p_function_list );
static int  downmix_Probe       ( probedata_t *p_data );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for AC3 downmix3dn module" )
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
MODULE_INIT
{
    p_module->psz_name = MODULE_STRING;
    p_module->psz_longname = "3D Now! AC3 downmix module";
    p_module->psz_version = VERSION;

    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_DOWNMIX;

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
MODULE_ACTIVATE
{
    p_module->p_functions = malloc( sizeof( module_functions_t ) );
    if( p_module->p_functions == NULL )
    {
        return( -1 );
    }

    downmix_getfunctions( &p_module->p_functions->downmix );

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
MODULE_DEACTIVATE
{
    free( p_module->p_functions );

    return( 0 );
}

/* Following functions are local */

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void downmix_getfunctions( function_list_t * p_function_list )
{
    p_function_list->pf_probe = downmix_Probe;
#define F p_function_list->functions.downmix
    F.pf_downmix_3f_2r_to_2ch = _M( downmix_3f_2r_to_2ch );
    F.pf_downmix_3f_1r_to_2ch = _M( downmix_3f_1r_to_2ch );
    F.pf_downmix_2f_2r_to_2ch = _M( downmix_2f_2r_to_2ch );
    F.pf_downmix_2f_1r_to_2ch = _M( downmix_2f_1r_to_2ch );
    F.pf_downmix_3f_0r_to_2ch = _M( downmix_3f_0r_to_2ch );
    F.pf_stream_sample_2ch_to_s16 = _M( stream_sample_2ch_to_s16 );
    F.pf_stream_sample_1ch_to_s16 = _M( stream_sample_1ch_to_s16 );
#undef F
}

/*****************************************************************************
 * downmix_Probe: returns a preference score
 *****************************************************************************/
static int downmix_Probe( probedata_t *p_data )
{
    if( !TestCPU( CPU_CAPABILITY_3DNOW ) )
    {
        return( 0 );
    }

    if( TestMethod( DOWNMIX_METHOD_VAR, "downmix3dn" ) )
    {
        return( 999 );
    }

    /* This plugin always works */
    return( 200 );
}

