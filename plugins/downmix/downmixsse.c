/*****************************************************************************
 * downmixsse.c : accelerated SSE AC3 downmix module
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: downmixsse.c,v 1.5 2001/11/28 15:08:05 massiot Exp $
 *
 * Authors: Renaud Dartus <reno@via.ecp.fr>
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

#define MODULE_NAME downmixsse
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "ac3_downmix.h"
#include "ac3_downmix_common.h"

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static void downmix_getfunctions( function_list_t * p_function_list );
static int  downmix_Probe       ( probedata_t *p_data );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for AC3 downmixsse module" )
    ADD_COMMENT( "Ha, ha -- nothing to configure yet" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_DOWNMIX;
    p_module->psz_longname = "SSE AC3 downmix module";
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    downmix_getfunctions( &p_module->p_functions->downmix );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

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
    if( !TestCPU( CPU_CAPABILITY_SSE ) )
    {
        return( 0 );
    }

    if( TestMethod( DOWNMIX_METHOD_VAR, "downmixsse" )
         || TestMethod( DOWNMIX_METHOD_VAR, "sse" ) )
    {
        return( 999 );
    }

    /* This plugin always works */
    return( 200 );
}

