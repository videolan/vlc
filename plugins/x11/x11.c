/*****************************************************************************
 * x11.c : X11 plugin for vlc
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: x11.c,v 1.11 2002/02/19 00:50:19 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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
    ADD_WINDOW( "Configuration for X11 module" )
        ADD_COMMENT( "For now, the X11 module cannot be configured" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "X11 module" )
    ADD_CAPABILITY( VOUT, 50 )
    ADD_SHORTCUT( "x11" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( vout_getfunctions )( &p_module->p_functions->vout );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

#if 0
/*****************************************************************************
 * vout_SetPalette: sets an 8 bpp palette
 *****************************************************************************
 * This function sets the palette given as an argument. It does not return
 * anything, but could later send information on which colors it was unable
 * to set.
 *****************************************************************************/
static void vout_SetPalette( p_vout_thread_t p_vout,
                             u16 *red, u16 *green, u16 *blue, u16 *transp )
{
    int i, j;
    XColor p_colors[255];

    /* allocate palette */
    for( i = 0, j = 255; i < 255; i++, j-- )
    {
        /* kludge: colors are indexed reversely because color 255 seems
         * to be reserved for black even if we try to set it to white */
        p_colors[ i ].pixel = j;
        p_colors[ i ].pad   = 0;
        p_colors[ i ].flags = DoRed | DoGreen | DoBlue;
        p_colors[ i ].red   = red[ j ];
        p_colors[ i ].blue  = blue[ j ];
        p_colors[ i ].green = green[ j ];
    }

    XStoreColors( p_vout->p_sys->p_display,
                  p_vout->p_sys->colormap, p_colors, 256 );
}
#endif

