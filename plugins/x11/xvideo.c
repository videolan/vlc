/*****************************************************************************
 * xvideo.c : Xvideo plugin for vlc
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: xvideo.c,v 1.12 2002/05/13 17:58:08 sam Exp $
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
#define ADAPTOR_TEXT N_("XVideo adaptor number")
#define ADAPTOR_LONGTEXT N_( \
    "If you graphics card provides several adaptors, this option allows you " \
    "to choose which one will be used (you shouldn't have to change this).")

#define ALT_FS_TEXT N_("alternate fullscreen method")
#define ALT_FS_LONGTEXT N_( \
    "There are two ways to make a fullscreen window, unfortunately each one " \
    "has its drawbacks.\n" \
    "1) Let the window manager handle your fullscreen window (default). But " \
    "things like taskbars will likely show on top of the video.\n" \
    "2) Completly bypass the window manager, but then nothing will be able " \
    "to show on top of the video.")

#define DISPLAY_TEXT N_("X11 display name")
#define DISPLAY_LONGTEXT N_( \
    "Specify the X11 hardware display you want to use.\nBy default vlc will " \
    "use the value of the DISPLAY environment variable.")

#define CHROMA_TEXT N_("XVimage chroma format")
#define CHROMA_LONGTEXT N_( \
    "Force the XVideo renderer to use a specific chroma format instead of " \
    "trying to improve performances by using the most efficient one.")

MODULE_CONFIG_START
ADD_CATEGORY_HINT( N_("Miscellaneous"), NULL )
ADD_STRING  ( "xvideo-display", NULL, NULL, DISPLAY_TEXT, DISPLAY_LONGTEXT )
ADD_INTEGER ( "xvideo-adaptor", -1, NULL, ADAPTOR_TEXT, ADAPTOR_LONGTEXT )
ADD_BOOL    ( "xvideo-altfullscreen", NULL, ALT_FS_TEXT, ALT_FS_LONGTEXT )
ADD_STRING  ( "xvideo-chroma", NULL, NULL, CHROMA_TEXT, CHROMA_LONGTEXT )
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( _("XVideo extension module") )
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

