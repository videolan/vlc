/*****************************************************************************
 * imdctsse.c : accelerated SSE IMDCT module
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: imdctsse.c,v 1.5 2001/06/12 00:30:41 reno Exp $
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

#define MODULE_NAME imdctsse
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

#include "ac3_imdct.h"
#include "ac3_imdct_common.h"

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static void imdct_getfunctions( function_list_t * p_function_list );
static int  imdct_Probe       ( probedata_t *p_data );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for IMDCT module" )
    ADD_COMMENT( "Ha, ha -- nothing to configure yet" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_IMDCT;
    p_module->psz_longname = "SSE AC3 IMDCT module";
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    imdct_getfunctions( &p_module->p_functions->imdct );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/* Following functions are local */

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void imdct_getfunctions( function_list_t * p_function_list )
{
    p_function_list->pf_probe = imdct_Probe;
#define F p_function_list->functions.imdct
    F.pf_imdct_init    = _M( imdct_init );
    F.pf_imdct_256     = _M( imdct_do_256 );
    F.pf_imdct_256_nol = _M( imdct_do_256_nol );
    F.pf_imdct_512     = _M( imdct_do_512 );
    F.pf_imdct_512_nol = _M( imdct_do_512_nol );
#undef F
}

/*****************************************************************************
 * imdct_Probe: returns a preference score
 *****************************************************************************/
static int imdct_Probe( probedata_t *p_data )
{
    if( !TestCPU( CPU_CAPABILITY_SSE ) )
    {
        return( 0 );
    }

    if( TestMethod( IMDCT_METHOD_VAR, "imdctsse" ) )
    {
        return( 999 );
    }

    /* This plugin always works */
    return( 200 );
}

