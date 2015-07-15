/*****************************************************************************
 * intf.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2014 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
 *          Pierre d'Herbemont <pdherbemont # videolan org>
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

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#import <vlc_common.h>
#import <vlc_interface.h>
#import <vlc_playlist.h>
#import <vlc_vout.h>
#import <vlc_aout.h>
#import <vlc_input.h>
#import <vlc_vout_window.h>

#import <Cocoa/Cocoa.h>
#import "SPMediaKeyTap.h"                   /* for the media key support */
#import "misc.h"
#import "MainWindow.h"
#import "VLCVoutWindowController.h"
#import "StringUtility.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
#define VLCIntf [[VLCMain sharedInstance] intf]

// You need to release those objects after use
input_thread_t *getInput(void);
vout_thread_t *getVout(void);
vout_thread_t *getVoutForActiveWindow(void);
audio_output_t *getAout(void);

static NSString * VLCInputChangedNotification = @"VLCInputChangedNotification";

/*****************************************************************************
 * VLCMain interface
 *****************************************************************************/
@class AppleRemote;
@class VLCInfo;
@class VLCControls;
@class VLCMainMenu;
@class VLCPlaylist;
@class InputManager;
@class ResumeDialogController;

@interface VLCMain : NSObject <NSWindowDelegate, NSApplicationDelegate>
{
    IBOutlet VLCMainWindow *o_mainwindow;            /* VLCMainWindow */

    IBOutlet VLCControls * o_controls;     /* VLCControls    */
    IBOutlet VLCPlaylist * o_playlist;     /* VLCPlaylist    */
}

@property (readonly) VLCVoutWindowController* voutController;
@property (readonly) BOOL nativeFullscreenMode;
@property (nonatomic, readwrite) BOOL playlistUpdatedSelectorInQueue;
+ (VLCMain *)sharedInstance;

- (intf_thread_t *)intf;
- (void)setIntf:(intf_thread_t *)p_mainintf;

- (VLCMainMenu *)mainMenu;
- (VLCMainWindow *)mainWindow;
- (id)controls;
- (id)bookmarks;
- (id)open;
- (id)simplePreferences;
- (id)preferences;
- (VLCPlaylist *)playlist;
- (id)coreDialogProvider;
- (ResumeDialogController *)resumeDialog;
- (id)eyeTVController;
- (id)appleRemoteController;
- (void)setActiveVideoPlayback:(BOOL)b_value;
- (BOOL)activeVideoPlayback;
- (void)applicationWillTerminate:(NSNotification *)notification;
- (void)updateCurrentlyUsedHotkeys;
- (BOOL)hasDefinedShortcutKey:(NSEvent *)o_event force:(BOOL)b_force;

- (void)plItemUpdated;
- (void)playbackModeUpdated;
- (void)showFullscreenController;

- (void)updateTogglePlaylistState;

- (void)mediaKeyTap:(SPMediaKeyTap*)keyTap receivedMediaKeyEvent:(NSEvent*)event;

- (void)resetAndReinitializeUserDefaults;

- (BOOL)isTerminating;

@end


/*****************************************************************************
 * VLCApplication interface
 *****************************************************************************/

@interface VLCApplication : NSApplication

@end
