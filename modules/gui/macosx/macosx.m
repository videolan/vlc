/*****************************************************************************
 * macosx.m: Mac OS X module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
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

#include "CompatibilityFixes.h"

/*****************************************************************************
 * External prototypes
 *****************************************************************************/
int  OpenIntf     (vlc_object_t *);
void CloseIntf    (vlc_object_t *);

int  WindowOpen   (vout_window_t *, const vout_window_cfg_t *);
void WindowClose  (vout_window_t *);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define VDEV_TEXT N_("Video device")
#define VDEV_LONGTEXT N_("Number of the screen to use by default to display " \
                         "videos in 'fullscreen'. The screen number correspondance can be found in "\
                         "the video device selection menu.")

#define OPAQUENESS_TEXT N_("Opaqueness")
#define OPAQUENESS_LONGTEXT N_("Set the transparency of the video output. 1 is non-transparent (default) " \
                                "0 is fully transparent.")

#define BLACK_TEXT N_("Black screens in fullscreen")
#define BLACK_LONGTEXT N_("In fullscreen mode, keep screen where there is no " \
                          "video displayed black")

#define FSPANEL_TEXT N_("Show Fullscreen controller")
#define FSPANEL_LONGTEXT N_("Shows a lucent controller when moving the mouse " \
                            "in fullscreen mode.")

#define AUTOPLAY_OSX_TEST N_("Auto-playback of new items")
#define AUTOPLAY_OSX_LONGTEXT N_("Start playback of new items immediately " \
                                 "once they were added.")

#define RECENT_ITEMS_TEXT N_("Keep Recent Items")
#define RECENT_ITEMS_LONGTEXT N_("By default, VLC keeps a list of the last 10 items. " \
                                 "This feature can be disabled here.")

#define USE_APPLE_REMOTE_TEXT N_("Control playback with the Apple Remote")
#define USE_APPLE_REMOTE_LONGTEXT N_("By default, VLC can be remotely controlled with the Apple Remote.")

#define USE_APPLE_REMOTE_VOLUME_TEXT N_("Control system volume with the Apple Remote")
#define USE_APPLE_REMOTE_VOLUME_LONGTEXT N_("By default, VLC will control its own volume with the Apple Remote. However, you can choose to control the global system volume instead.")

#define USE_APPLE_REMOTE_PREVNEXT_TEXT N_("Control playlist items with the Apple Remote")
#define USE_APPLE_REMOTE_PREVNEXT_LONGTEXT N_("By default, VLC will allow you to switch to the next or previous item with the Apple Remote. You can disable this behavior with this option.")

#define USE_MEDIAKEYS_TEXT N_("Control playback with media keys")
#define USE_MEDIAKEYS_LONGTEXT N_("By default, VLC can be controlled using the media keys on modern Apple " \
                                  "keyboards.")

#define INTERFACE_STYLE_TEXT N_("Run VLC with dark interface style")
#define INTERFACE_STYLE_LONGTEXT N_("If this option is enabled, VLC will use the dark interface style. Otherwise, the grey interface style is used.")

#define NATIVE_FULLSCREEN_MODE_ON_LION_TEXT N_("Use the native fullscreen mode")
#define NATIVE_FULLSCREEN_MODE_ON_LION_LONGTEXT N_("By default, VLC uses the fullscreen mode known from previous Mac OS X releases. It can also use the native fullscreen mode on Mac OS X 10.7 and later.")

#define KEEPSIZE_TEXT N_("Resize interface to the native video size")
#define KEEPSIZE_LONGTEXT N_("You have two choices:\n" \
" - The interface will resize to the native video size\n" \
" - The video will fit to the interface size\n " \
"By default, interface resize to the native video size.")

#define PAUSE_MINIMIZED_TEXT N_("Pause the video playback when minimized")
#define PAUSE_MINIMIZED_LONGTEXT N_(\
"With this option enabled, the playback will be automatically paused when minimizing the window.")

#define ICONCHANGE_TEXT N_("Allow automatic icon changes")
#define ICONCHANGE_LONGTEXT N_("This option allows the interface to change its icon on various occasions.")

#define LOCK_ASPECT_RATIO_TEXT N_("Lock Aspect Ratio")

#define JUMPBUTTONS_TEXT N_("Show Previous & Next Buttons")
#define JUMPBUTTONS_LONGTEXT N_("Shows the previous and next buttons in the main window.")

#define PLAYMODEBUTTONS_TEXT N_("Show Shuffle & Repeat Buttons")
#define PLAYMODEBUTTONS_LONGTEXT N_("Shows the shuffle and repeat buttons in the main window.")

#define EFFECTSBUTTON_TEXT N_("Show Audio Effects Button")
#define EFFECTSBUTTON_LONGTEXT N_("Shows the audio effects button in the main window.")

#define SIDEBAR_TEXT N_("Show Sidebar")
#define SIDEBAR_LONGTEXT N_("Shows a sidebar in the main window listing media sources.")

#define ITUNES_TEXT N_("Pause iTunes during VLC playback")
#define ITUNES_LONGTEXT N_("Pauses iTunes playback when VLC playback starts. If selected, iTunes playback will be resumed again if VLC playback is finished.")

static const int itunes_list[] =
    { 0, 1, 2 };
static const char *const itunes_list_text[] = {
    N_("Do nothing"), N_("Pause iTunes"), N_("Pause and resume iTunes")
};

#define VOLUME_MAX_TEXT N_("Maximum Volume displayed")


vlc_module_begin()
    set_description(N_("Mac OS X interface"))
    set_capability("interface", 200)
    set_callbacks(OpenIntf, CloseIntf)
    set_category(CAT_INTERFACE)
    set_subcategory(SUBCAT_INTERFACE_MAIN)
    cannot_unload_broken_library()

    BOOL b_have_mavericks = OSX_MAVERICKS;

    set_section(N_("Appearance"), 0)
        add_bool("macosx-interfacestyle", false, INTERFACE_STYLE_TEXT, INTERFACE_STYLE_LONGTEXT, false)
        add_bool("macosx-nativefullscreenmode", b_have_mavericks, NATIVE_FULLSCREEN_MODE_ON_LION_TEXT, NATIVE_FULLSCREEN_MODE_ON_LION_LONGTEXT, false)
        add_bool("macosx-icon-change", true, ICONCHANGE_TEXT, ICONCHANGE_LONGTEXT, true)
        add_bool("macosx-show-playback-buttons", false, JUMPBUTTONS_TEXT, JUMPBUTTONS_LONGTEXT, false)
        add_bool("macosx-show-playmode-buttons", false, PLAYMODEBUTTONS_TEXT, PLAYMODEBUTTONS_LONGTEXT, false)
        add_bool("macosx-show-effects-button", false, EFFECTSBUTTON_TEXT, EFFECTSBUTTON_LONGTEXT, false)
        add_bool("macosx-show-sidebar", true, SIDEBAR_TEXT, SIDEBAR_LONGTEXT, false)
        add_integer_with_range("macosx-max-volume", 125, 60, 200, VOLUME_MAX_TEXT, VOLUME_MAX_TEXT, true)

    set_section(N_("Behavior"), 0)
        add_bool("macosx-autoplay", true, AUTOPLAY_OSX_TEST, AUTOPLAY_OSX_LONGTEXT, false)
        add_bool("macosx-recentitems", true, RECENT_ITEMS_TEXT, RECENT_ITEMS_LONGTEXT, false)
        add_bool("macosx-fspanel", true, FSPANEL_TEXT, FSPANEL_LONGTEXT, false)
        add_bool("macosx-video-autoresize", true, KEEPSIZE_TEXT, KEEPSIZE_LONGTEXT, false)
        add_bool("macosx-pause-minimized", false, PAUSE_MINIMIZED_TEXT, PAUSE_MINIMIZED_LONGTEXT, false)
        add_bool("macosx-lock-aspect-ratio", true, LOCK_ASPECT_RATIO_TEXT, LOCK_ASPECT_RATIO_TEXT, true)
        add_integer("macosx-control-itunes", 1, ITUNES_TEXT, ITUNES_LONGTEXT, false)
        change_integer_list(itunes_list, itunes_list_text)

    set_section(N_("Apple Remote and media keys"), 0)
        add_bool("macosx-appleremote", true, USE_APPLE_REMOTE_TEXT, USE_APPLE_REMOTE_LONGTEXT, false)
        add_bool("macosx-appleremote-sysvol", false, USE_APPLE_REMOTE_VOLUME_TEXT, USE_APPLE_REMOTE_VOLUME_LONGTEXT, false)
        add_bool("macosx-appleremote-prevnext", false, USE_APPLE_REMOTE_PREVNEXT_TEXT, USE_APPLE_REMOTE_PREVNEXT_LONGTEXT, false)
        add_bool("macosx-mediakeys", true, USE_MEDIAKEYS_TEXT, USE_MEDIAKEYS_LONGTEXT, false)

    add_obsolete_bool("macosx-stretch") /* since 2.0.0 */
    add_obsolete_bool("macosx-eq-keep") /* since 2.0.0 */
    add_obsolete_bool("macosx-autosave-volume") /* since 2.1.0 */

    add_submodule()
        set_description("Mac OS X Video Output Provider")
        set_capability("vout window nsobject", 100)
        set_callbacks(WindowOpen, WindowClose)

        set_section(N_("Video output"), 0)
        add_integer("macosx-vdev", 0, VDEV_TEXT, VDEV_LONGTEXT, false)
        add_float_with_range("macosx-opaqueness", 1, 0, 1, OPAQUENESS_TEXT, OPAQUENESS_LONGTEXT, true);
        add_bool("macosx-black", false, BLACK_TEXT, BLACK_LONGTEXT, false)
vlc_module_end()

