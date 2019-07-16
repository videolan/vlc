/*****************************************************************************
 * VLCMain.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2019 VLC authors and VideoLAN
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

#import <Cocoa/Cocoa.h>

#import <vlc_common.h>
#import <vlc_interface.h>

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

intf_thread_t *getIntf(void);

extern NSString *VLCConfigurationChangedNotification;

/*****************************************************************************
 * VLCMain interface
 *****************************************************************************/
@class VLCExtensionsManager;
@class VLCMainMenu;
@class VLCSimplePrefsController;
@class VLCPrefs;
@class VLCCoreDialogProvider;
@class VLCBookmarksWindowController;
@class VLCOpenWindowController;
@class VLCLogWindowController;
@class VLCLibraryWindow;
@class VLCLibraryWindowController;
@class VLCTrackSynchronizationWindowController;
@class VLCAudioEffectsWindowController;
@class VLCVideoEffectsWindowController;
@class VLCConvertAndSaveWindowController;
@class VLCLibraryController;
@class VLCPlaylistController;
@class VLCVideoOutputProvider;

@interface VLCMain : NSObject

+ (VLCMain *)sharedInstance;
+ (void)relaunchApplication;

@property (readonly) BOOL isTerminating;

@property (readonly) VLCAudioEffectsWindowController *audioEffectsPanel;
@property (readonly) VLCBookmarksWindowController *bookmarks;
@property (readonly) VLCConvertAndSaveWindowController *convertAndSaveWindow;
@property (readonly) VLCCoreDialogProvider *coreDialogProvider;
@property (readonly) VLCExtensionsManager *extensionsManager;
@property (readonly) VLCLogWindowController *debugMsgPanel;
@property (readonly) VLCLibraryController *libraryController;
@property (readonly) VLCLibraryWindow *libraryWindow;
@property (readonly) VLCLibraryWindowController *libraryWindowController;
@property (readonly) VLCMainMenu *mainMenu;
@property (readonly) VLCOpenWindowController *open;
@property (readonly) VLCPlaylistController *playlistController;
@property (readonly) VLCPrefs *preferences;
@property (readonly) VLCSimplePrefsController *simplePreferences;
@property (readonly) VLCTrackSynchronizationWindowController *trackSyncPanel;
@property (readonly) VLCVideoEffectsWindowController *videoEffectsPanel;
@property (readonly) VLCVideoOutputProvider *voutProvider;

@end
