/*****************************************************************************
 * xvideo.c : Xvideo plugin for vlc
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: xvideo.c,v 1.6 2001/12/30 07:09:56 sam Exp $
 *
 * Authors: Shane Harper <shanegh@optusnet.com.au>
 *          Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          David Kennedy <dkennedy@tinytoad.com>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                            /* strerror() */

#include <videolan/vlc.h>

#include "xcommon.h"

/*****************************************************************************
 * Building configuration tree
 *****************************************************************************/
MODULE_CONFIG_START
    ADD_WINDOW( "Configuration for xvideo module" )
        ADD_COMMENT( "For now, the xvideo module cannot be configured" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "XVideo extension module" )
    ADD_CAPABILITY( VOUT, 150 )
    ADD_SHORTCUT( "xvideo" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( vout_getfunctions )( &p_module->p_functions->vout );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/* following functions are local */

#if 0
/*****************************************************************************
 * XVideoSetAttribute
 *****************************************************************************
 * This function can be used to set attributes, e.g. XV_BRIGHTNESS and
 * XV_CONTRAST. "f_value" should be in the range of 0 to 1.
 *****************************************************************************/
static void XVideoSetAttribute( vout_thread_t *p_vout,
                                char *attr_name, float f_value )
{
    int             i_attrib;
    XvAttribute    *p_attrib;
    Display        *p_display = p_vout->p_sys->p_display;
    int             i_xvport  = p_vout->p_sys->i_xvport;

    p_attrib = XvQueryPortAttributes( p_display, i_xvport, &i_attrib );

    do
    {
        i_attrib--;

        if( i_attrib >= 0 && !strcmp( p_attrib[ i_attrib ].name, attr_name ) )
        {
            int i_sv = f_value * ( p_attrib[ i_attrib ].max_value
                                    - p_attrib[ i_attrib ].min_value + 1 )
                        + p_attrib[ i_attrib ].min_value;

            XvSetPortAttribute( p_display, i_xvport,
                            XInternAtom( p_display, attr_name, False ), i_sv );
            break;
        }

    } while( i_attrib > 0 );

    if( p_attrib )
        XFree( p_attrib );
}
#endif

