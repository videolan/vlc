/*****************************************************************************
 * downmix.c : A52 downmix module
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: downmix.c,v 1.2 2002/08/21 09:27:40 sam Exp $
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

#include <vlc/vlc.h>

#include "../downmix.h"
#include "downmix_common.h"

/*****************************************************************************
 * Module initializer
 *****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    downmix_t *p_downmix = (downmix_t *)p_this;

    p_downmix->pf_downmix_3f_2r_to_2ch = E_( downmix_3f_2r_to_2ch );
    p_downmix->pf_downmix_3f_1r_to_2ch = E_( downmix_3f_1r_to_2ch );
    p_downmix->pf_downmix_2f_2r_to_2ch = E_( downmix_2f_2r_to_2ch );
    p_downmix->pf_downmix_2f_1r_to_2ch = E_( downmix_2f_1r_to_2ch );
    p_downmix->pf_downmix_3f_0r_to_2ch = E_( downmix_3f_0r_to_2ch );
    p_downmix->pf_stream_sample_2ch_to_s16 = E_( stream_sample_2ch_to_s16 );
    p_downmix->pf_stream_sample_1ch_to_s16 = E_( stream_sample_1ch_to_s16 );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
#ifdef MODULE_NAME_IS_downmix
    set_description( _("A52 downmix module") );
    set_capability( "downmix", 50 );
    add_shortcut( "c" );
#elif defined( MODULE_NAME_IS_downmixsse )
    set_description( _("SSE A52 downmix module") );
    set_capability( "downmix", 200 );
    add_shortcut( "sse" );
    add_requirement( SSE );
#elif defined( MODULE_NAME_IS_downmix3dn )
    set_description( _("3D Now! A52 downmix module") );
    set_capability( "downmix", 200 );
    add_shortcut( "3dn" );
    add_shortcut( "3dnow" );
    add_requirement( 3DNOW );
#endif
    set_callbacks( Open, NULL );
vlc_module_end();

