/*****************************************************************************
 * imdct.c : IMDCT module
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: imdct.c,v 1.2 2002/08/21 09:27:40 sam Exp $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <string.h>

#include <vlc/vlc.h>

#include "../imdct.h"
#include "imdct_common.h"

/*****************************************************************************
 * Module initializer
 *****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    imdct_t *p_imdct = (imdct_t *)p_this;

    p_imdct->pf_imdct_init    = E_( imdct_init );
    p_imdct->pf_imdct_256     = E_( imdct_do_256 );
    p_imdct->pf_imdct_256_nol = E_( imdct_do_256_nol );
    p_imdct->pf_imdct_512     = E_( imdct_do_512 );
    p_imdct->pf_imdct_512_nol = E_( imdct_do_512_nol );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
#if defined( MODULE_NAME_IS_imdct )
    set_description( _("A52 IMDCT module") );
    set_capability( "imdct", 50 );
    add_shortcut( "c" );
#elif defined( MODULE_NAME_IS_imdctsse )
    set_description( _("SSE A52 IMDCT module") );
    set_capability( "imdct", 200 );
    add_shortcut( "sse" );
    add_requirement( SSE );
#elif defined( MODULE_NAME_IS_imdct3dn )
    set_description( _("3D Now! A52 IMDCT module") );
    set_capability( "imdct", 200 );
    add_shortcut( "3dn" );
    add_shortcut( "3dnow" );
    add_requirement( 3DNOW );
#endif
    set_callbacks( Open, NULL );
vlc_module_end();

