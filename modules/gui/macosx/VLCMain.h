/*****************************************************************************
 * VLCMain.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2016 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
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
#import <stdatomic.h>

#import <Cocoa/Cocoa.h>
#import "misc.h"
#import "VLCMainWindow.h"
#import "VLCVideoOutputProvider.h"
#import "VLCStatusBarIcon.h"
#import "VLCStringUtility.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

intf_thread_t *getIntf(void);

static NSString * VLCInputChangedNotification = @"VLCInputChangedNotification";
static NSString * VLCConfigurationChangedNotification = @"VLCConfigurationChangedNotification";
static NSString * VLCMediaKeySupportSettingChangedNotification = @"VLCMediaKeySupportSettingChangedNotification";

/*****************************************************************************
 * VLCMain interface
 *****************************************************************************/
@class AppleRemote;
@class VLCInfo;
@class VLCMainMenu;
@class VLCPlaylist;
@class VLCInputManager;
@class VLCResumeDialogController;
@class VLCSimplePrefsController;
@class VLCPrefs;
@class VLCCoreDialogProvider;
@class VLCBookmarksWindowController;
@class VLCOpenWindowController;
@class VLCLogWindowController;
@class VLCTrackSynchronizationWindowController;
@class VLCAudioEffectsWindowController;
@class VLCVideoEffectsWindowController;
@class VLCConvertAndSaveWindowController;
@class VLCExtensionsManager;
@class VLCInfo;
@class VLCStatusBarIcon;

@interface VLCMain : NSObject <NSWindowDelegate, NSApplicationDelegate>

@property (readonly) VLCVideoOutputProvider* voutProvider;
@property (readonly) BOOL nativeFullscreenMode;
@property (nonatomic, readwrite) BOOL playlistUpdatedSelectorInQueue;

+ (VLCMain *)sharedInstance;
+ (void)killInstance;

- (VLCMainMenu *)mainMenu;
- (VLCMainWindow *)mainWindow;
- (VLCBookmarksWindowController *)bookmarks;
- (VLCOpenWindowController *)open;
- (VLCSimplePrefsController *)simplePreferences;
- (VLCPrefs *)preferences;
- (VLCPlaylist *)playlist;
- (VLCCoreDialogProvider *)coreDialogProvider;
- (VLCResumeDialogController *)resumeDialog;
- (VLCInputManager *)inputManager;
- (VLCExtensionsManager *)extensionsManager;
- (VLCStatusBarIcon *)statusBarIcon;

- (VLCLogWindowController *)debugMsgPanel;

- (VLCTrackSynchronizationWindowController *)trackSyncPanel;
- (VLCAudioEffectsWindowController *)audioEffectsPanel;
- (VLCVideoEffectsWindowController *)videoEffectsPanel;

- (VLCInfo *)currentMediaInfoPanel;


- (VLCConvertAndSaveWindowController *)convertAndSaveWindow;

- (void)setActiveVideoPlayback:(BOOL)b_value;
- (BOOL)activeVideoPlayback;
- (void)applicationWillTerminate:(NSNotification *)notification;

- (void)showFullscreenController;

- (BOOL)isTerminating;

@end

#import "helpers.h"
