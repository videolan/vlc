/*****************************************************************************
 * imdct3dn.c : accelerated 3D Now! IMDCT module
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: imdct3dn.c,v 1.10 2002/02/15 13:32:53 sam Exp $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <string.h>

#include <videolan/vlc.h>

#include "ac3_imdct.h"
#include "ac3_imdct_common.h"

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static void imdct_getfunctions( function_list_t * p_function_list );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "3D Now! AC3 IMDCT module" )
    ADD_CAPABILITY( IMDCT, 200 )
    ADD_REQUIREMENT( 3DNOW )
    ADD_SHORTCUT( "3dn" )
    ADD_SHORTCUT( "3dnow" )
    ADD_SHORTCUT( "imdct3dn" )
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
#define F p_function_list->functions.imdct
    F.pf_imdct_init    = _M( imdct_init );
    F.pf_imdct_256     = _M( imdct_do_256 );
    F.pf_imdct_256_nol = _M( imdct_do_256_nol );
    F.pf_imdct_512     = _M( imdct_do_512 );
    F.pf_imdct_512_nol = _M( imdct_do_512_nol );
#undef F
}

