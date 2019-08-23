/*****************************************************************************
 * macosx.m: Mac OS X module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2014 VLC authors and VideoLAN
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>

/*****************************************************************************
 * External prototypes
 *****************************************************************************/
int  OpenIntf     (vlc_object_t *);
void CloseIntf    (vlc_object_t *);

int  WindowOpen   (vout_window_t *);

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

#define DISPLAY_STATUS_ICONMENU_TEXT N_("Display VLC status menu icon")
#define DISPLAY_STATUS_ICONMENU_LONGTEXT N_("By default, VLC will show the statusbar icon menu. However, you can choose to disable it (restart required).")

#define USE_APPLE_REMOTE_PREVNEXT_TEXT N_("Control playlist items with the Apple Remote")
#define USE_APPLE_REMOTE_PREVNEXT_LONGTEXT N_("By default, VLC will allow you to switch to the next or previous item with the Apple Remote. You can disable this behavior with this option.")

#define USE_MEDIAKEYS_TEXT N_("Control playback with media keys")
#define USE_MEDIAKEYS_LONGTEXT N_("By default, VLC can be controlled using the media keys on modern Apple " \
                                  "keyboards.")

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

#define DIM_KEYBOARD_PLAYBACK_TEXT N_("Dim keyboard backlight during fullscreen playback")
#define DIM_KEYBOARD_PLAYBACK_LONGTEXT N_("Turn off the MacBook keyboard backlight while a video is playing in fullscreen. Automatic brightness adjustment should be disabled in System Preferences.")

#define ITUNES_TEXT N_("Control external music players")
#define ITUNES_LONGTEXT N_("VLC will pause and resume supported music players on playback.")

#define LARGE_LISTFONT_TEXT N_("Use large text for list views")

static const int itunes_list[] =
    { 0, 1, 2 };
static const char *const itunes_list_text[] = {
    N_("Do nothing"), N_("Pause iTunes / Spotify"), N_("Pause and resume iTunes / Spotify")
};

#define CONTINUE_PLAYBACK_TEXT N_("Continue playback where you left off")
#define CONTINUE_PLAYBACK_LONGTEXT N_("VLC will store playback positions of the last 30 items you played. If you re-open one of those, playback will continue.")

static const int continue_playback_list[] =
{ 0, 1, 2 };
static const char *const continue_playback_list_text[] = {
    N_("Ask"), N_("Always"), N_("Never")
};

#define VOLUME_MAX_TEXT N_("Maximum Volume displayed")


vlc_module_begin()
    set_description(N_("Mac OS X interface"))
    set_capability("interface", 200)
    set_callbacks(OpenIntf, CloseIntf)
    set_category(CAT_INTERFACE)
    set_subcategory(SUBCAT_INTERFACE_MAIN)
    cannot_unload_broken_library()

    set_section(N_("Appearance"), 0)
        add_bool("macosx-nativefullscreenmode", false, NATIVE_FULLSCREEN_MODE_ON_LION_TEXT, NATIVE_FULLSCREEN_MODE_ON_LION_LONGTEXT, false)
        add_bool("macosx-statusicon", true, DISPLAY_STATUS_ICONMENU_TEXT, DISPLAY_STATUS_ICONMENU_LONGTEXT, false)
        add_bool("macosx-icon-change", true, ICONCHANGE_TEXT, ICONCHANGE_LONGTEXT, true)
        add_integer_with_range("macosx-max-volume", 125, 60, 200, VOLUME_MAX_TEXT, VOLUME_MAX_TEXT, true)
        add_bool("macosx-large-text", false, LARGE_LISTFONT_TEXT, LARGE_LISTFONT_TEXT, false)

    set_section(N_("Behavior"), 0)
        add_bool("macosx-autoplay", true, AUTOPLAY_OSX_TEST, AUTOPLAY_OSX_LONGTEXT, false)
        add_bool("macosx-recentitems", true, RECENT_ITEMS_TEXT, RECENT_ITEMS_LONGTEXT, false)
        add_bool("macosx-fspanel", true, FSPANEL_TEXT, FSPANEL_LONGTEXT, false)
        add_bool("macosx-video-autoresize", true, KEEPSIZE_TEXT, KEEPSIZE_LONGTEXT, false)
        add_bool("macosx-pause-minimized", false, PAUSE_MINIMIZED_TEXT, PAUSE_MINIMIZED_LONGTEXT, false)
        add_bool("macosx-lock-aspect-ratio", true, LOCK_ASPECT_RATIO_TEXT, LOCK_ASPECT_RATIO_TEXT, true)
        add_bool("macosx-dim-keyboard", false, DIM_KEYBOARD_PLAYBACK_TEXT, DIM_KEYBOARD_PLAYBACK_LONGTEXT, false)
        add_integer("macosx-control-itunes", 1, ITUNES_TEXT, ITUNES_LONGTEXT, false)
        change_integer_list(itunes_list, itunes_list_text)
        add_integer("macosx-continue-playback", 0, CONTINUE_PLAYBACK_TEXT, CONTINUE_PLAYBACK_LONGTEXT, false)
        change_integer_list(continue_playback_list, continue_playback_list_text)

    set_section(N_("Apple Remote and media keys"), 0)
        add_bool("macosx-appleremote", true, USE_APPLE_REMOTE_TEXT, USE_APPLE_REMOTE_LONGTEXT, false)
        add_bool("macosx-appleremote-sysvol", false, USE_APPLE_REMOTE_VOLUME_TEXT, USE_APPLE_REMOTE_VOLUME_LONGTEXT, false)
        add_bool("macosx-appleremote-prevnext", false, USE_APPLE_REMOTE_PREVNEXT_TEXT, USE_APPLE_REMOTE_PREVNEXT_LONGTEXT, false)
        add_bool("macosx-mediakeys", true, USE_MEDIAKEYS_TEXT, USE_MEDIAKEYS_LONGTEXT, false)

    add_obsolete_bool("macosx-stretch") /* since 2.0.0 */
    add_obsolete_bool("macosx-eq-keep") /* since 2.0.0 */
    add_obsolete_bool("macosx-autosave-volume") /* since 2.1.0 */
    add_obsolete_bool("macosx-show-sidebar") /* since 3.0.1 */
    add_obsolete_bool("macosx-interfacestyle") /* since 4.0.0 */
    add_obsolete_bool("macosx-show-playmode-buttons") /* since 4.0.0 */
    add_obsolete_bool("macosx-show-playback-buttons") /* since 4.0.0 */
    add_obsolete_bool("macosx-show-effects-button") /* since 4.0.0 */

    add_submodule()
        set_description("Mac OS X Video Output Provider")
        set_capability("vout window", 100)
        set_callback(WindowOpen)

        set_section(N_("Video output"), 0)
        add_integer("macosx-vdev", 0, VDEV_TEXT, VDEV_LONGTEXT, false)
        add_float_with_range("macosx-opaqueness", 1, 0, 1, OPAQUENESS_TEXT, OPAQUENESS_LONGTEXT, true);
        add_bool("macosx-black", false, BLACK_TEXT, BLACK_LONGTEXT, false)
vlc_module_end()

/* the following is fake code to make the pseudo VLC target for the macOS module compile and link */
#ifdef MACOS_PSEUDO_VLC
const char vlc_module_name[] = "macos-pseudo-vlc";

int main(int argc, char *argv[])
{
    return 0;
}
#endif
