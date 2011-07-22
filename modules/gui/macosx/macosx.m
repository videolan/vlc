/*****************************************************************************
 * macosx.m: Mac OS X module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2011 the VideoLAN team
 * $Id$
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Eugenio Jarosiewicz <ej0@cise.ufl.edu>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>

/*****************************************************************************
 * External prototypes
 *****************************************************************************/
int  OpenIntf     ( vlc_object_t * );
void CloseIntf    ( vlc_object_t * );

int  WindowOpen   ( vout_window_t *, const vout_window_cfg_t * );
void WindowClose  ( vout_window_t * );

int  OpenVideoGL  ( vlc_object_t * );
void CloseVideoGL ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define VDEV_TEXT N_("Video device")
#define VDEV_LONGTEXT N_("Number of the screen to use by default to display " \
                         "videos in 'fullscreen'. The screen number correspondance can be found in "\
                         "the video device selection menu.")

#define OPAQUENESS_TEXT N_("Opaqueness")
#define OPAQUENESS_LONGTEXT N_( "Set the transparency of the video output. 1 is non-transparent (default) " \
                                "0 is fully transparent.")

#define STRETCH_TEXT N_("Stretch video to fill window")
#define STRETCH_LONGTEXT N_("Stretch the video to fill the entire window when "\
                            "resizing the video instead of keeping the aspect ratio and "\
                            "displaying black borders.")

#define BLACK_TEXT N_("Black screens in fullscreen")
#define BLACK_LONGTEXT N_("In fullscreen mode, keep screen where there is no " \
                          "video displayed black" )

#define BACKGROUND_TEXT N_("Use as Desktop Background")
#define BACKGROUND_LONGTEXT N_("Use the video as the Desktop Background " \
                               "Desktop icons cannot be interacted with in this mode." )

#define FSPANEL_TEXT N_("Show Fullscreen controller")
#define FSPANEL_LONGTEXT N_("Shows a lucent controller when moving the mouse " \
                            "in fullscreen mode.")

#define AUTOPLAY_OSX_TEST N_("Auto-playback of new items")
#define AUTOPLAY_OSX_LONGTEXT N_("Start playback of new items immediately " \
                                 "once they were added." )

#define RECENT_ITEMS_TEXT N_("Keep Recent Items")
#define RECENT_ITEMS_LONGTEXT N_("By default, VLC keeps a list of the last 10 items. " \
                                 "This feature can be disabled here.")

#define EQ_KEEP_TEXT N_("Keep current Equalizer settings")
#define EQ_KEEP_LONGTEXT N_("By default, VLC keeps the last equalizer settings before " \
                            "termination. This feature can be disabled here.")

#define USE_APPLE_REMOTE_TEXT N_("Control playback with the Apple Remote")
#define USE_APPLE_REMOTE_LONGTEXT N_("By default, VLC can be remotely controlled with the Apple Remote.")

#define USE_MEDIAKEYS_TEXT N_("Control playback with media keys")
#define USE_MEDIAKEYS_LONGTEXT N_("By default, VLC can be controlled using the media keys on modern Apple " \
                                  "keyboards.")

#define INTERFACE_STYLE_TEXT N_("Run VLC with dark or bright interface style")
#define INTERFACE_STYLE_LONGTEXT N_("By default, VLC will use the dark interface style.")

vlc_module_begin ()
    set_description( N_("Mac OS X interface") )
    set_capability( "interface", 200 )
    set_callbacks( OpenIntf, CloseIntf )
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_MAIN )
    cannot_unload_broken_library( )
    add_bool( "macosx-autoplay", true, AUTOPLAY_OSX_TEST, AUTOPLAY_OSX_LONGTEXT,
              false )
    add_bool( "macosx-recentitems", true, RECENT_ITEMS_TEXT, RECENT_ITEMS_LONGTEXT,
              false )
    add_bool( "macosx-eq-keep", true, EQ_KEEP_TEXT, EQ_KEEP_LONGTEXT,
              false )
    add_bool( "macosx-fspanel", true, FSPANEL_TEXT, FSPANEL_LONGTEXT,
              false )
    add_bool( "macosx-appleremote", true, USE_APPLE_REMOTE_TEXT, USE_APPLE_REMOTE_LONGTEXT,
             false )
    add_bool( "macosx-mediakeys", true, USE_MEDIAKEYS_TEXT, USE_MEDIAKEYS_LONGTEXT,
             false )
    add_bool( "macosx-interfacestyle", true, INTERFACE_STYLE_TEXT, INTERFACE_STYLE_LONGTEXT,
             false )

    add_submodule ()
        set_description( "Mac OS X Video Output Provider" )
        set_capability( "vout window nsobject", 100 )
        set_callbacks( WindowOpen, WindowClose )

        add_integer( "macosx-vdev", 0, VDEV_TEXT, VDEV_LONGTEXT,
                     false )
        add_bool( "macosx-stretch", false, STRETCH_TEXT, STRETCH_LONGTEXT,
                  false )
        add_float_with_range( "macosx-opaqueness", 1, 0, 1,
                              OPAQUENESS_TEXT, OPAQUENESS_LONGTEXT, true );
        add_bool( "macosx-black", true, BLACK_TEXT, BLACK_LONGTEXT,
                  false )
        add_bool( "macosx-background", false, BACKGROUND_TEXT, BACKGROUND_LONGTEXT,
                  false )
vlc_module_end ()

