/*****************************************************************************
 * x11.c : X11 plugin for vlc
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: x11.c,v 1.6 2003/03/30 18:14:39 gbazin Exp $
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

#include <vlc/vlc.h>

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
extern int  E_(Activate)   ( vlc_object_t * );
extern void E_(Deactivate) ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
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
    "Specify the X11 hardware display you want to use. By default VLC will " \
    "use the value of the DISPLAY environment variable.")

#define SHM_TEXT N_("use shared memory")
#define SHM_LONGTEXT N_( \
    "Use shared memory to communicate between VLC and the X server.")

vlc_module_begin();
    add_category_hint( N_("X11"), NULL, VLC_TRUE );
    add_string( "x11-display", NULL, NULL, DISPLAY_TEXT, DISPLAY_LONGTEXT, VLC_TRUE );
    add_bool( "x11-altfullscreen", 0, NULL, ALT_FS_TEXT, ALT_FS_LONGTEXT, VLC_TRUE );
#ifdef HAVE_SYS_SHM_H
    add_bool( "x11-shm", 1, NULL, SHM_TEXT, SHM_LONGTEXT, VLC_TRUE );
#endif
    set_description( _("X11 video output") );
    set_capability( "video output", 50 );
    set_callbacks( E_(Activate), E_(Deactivate) );
vlc_module_end();

