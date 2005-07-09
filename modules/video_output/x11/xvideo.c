/*****************************************************************************
 * xvideo.c : Xvideo plugin for vlc
 *****************************************************************************
 * Copyright (C) 1998-2001 the VideoLAN team
 * $Id$
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

#include <vlc/vlc.h>

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
extern int  E_(Activate)   ( vlc_object_t * );
extern void E_(Deactivate) ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ADAPTOR_TEXT N_("XVideo adaptor number")
#define ADAPTOR_LONGTEXT N_( \
    "If you graphics card provides several adaptors, this option allows you " \
    "to choose which one will be used (you shouldn't have to change this).")

#define ALT_FS_TEXT N_("Alternate fullscreen method")
#define ALT_FS_LONGTEXT N_( \
    "There are two ways to make a fullscreen window, unfortunately each one " \
    "has its drawbacks.\n" \
    "1) Let the window manager handle your fullscreen window (default), but " \
    "things like taskbars will likely show on top of the video.\n" \
    "2) Completely bypass the window manager, but then nothing will be able " \
    "to show on top of the video.")

#define DISPLAY_TEXT N_("X11 display name")
#define DISPLAY_LONGTEXT N_( \
    "Specify the X11 hardware display you want to use. By default VLC will " \
    "use the value of the DISPLAY environment variable.")

#define CHROMA_TEXT N_("XVimage chroma format")
#define CHROMA_LONGTEXT N_( \
    "Force the XVideo renderer to use a specific chroma format instead of " \
    "trying to improve performances by using the most efficient one.")

#define SHM_TEXT N_("Use shared memory")
#define SHM_LONGTEXT N_( \
    "Use shared memory to communicate between VLC and the X server.")

#define SCREEN_TEXT N_("Screen to be used for fullscreen mode.")
#define SCREEN_LONGTEXT N_( \
    "Choose the screen you want to use in fullscreen mode. For instance " \
    "set it to 0 for first screen, 1 for the second.")

vlc_module_begin();
    set_shortname( "XVideo" );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VOUT );
    add_string( "xvideo-display", NULL, NULL, DISPLAY_TEXT, DISPLAY_LONGTEXT, VLC_TRUE );
    add_integer( "xvideo-adaptor", -1, NULL, ADAPTOR_TEXT, ADAPTOR_LONGTEXT, VLC_TRUE );
    add_bool( "xvideo-altfullscreen", 0, NULL, ALT_FS_TEXT, ALT_FS_LONGTEXT, VLC_TRUE );
    add_string( "xvideo-chroma", NULL, NULL, CHROMA_TEXT, CHROMA_LONGTEXT, VLC_TRUE );
#ifdef HAVE_SYS_SHM_H
    add_bool( "xvideo-shm", 1, NULL, SHM_TEXT, SHM_LONGTEXT, VLC_TRUE );
#endif
#ifdef HAVE_XINERAMA
    add_integer ( "xvideo-xineramascreen", 0, NULL, SCREEN_TEXT, SCREEN_LONGTEXT, VLC_TRUE );
#endif

    set_description( _("XVideo extension video output") );
    set_capability( "video output", 150 );
    set_callbacks( E_(Activate), E_(Deactivate) );
vlc_module_end();

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

