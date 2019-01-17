/*****************************************************************************
 * VLCMainWindow.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2014 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan.org>
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

#import <Cocoa/Cocoa.h>

#import <vlc_input.h>
#import <vlc_vout_window.h>

#import "VLCVideoWindowCommon.h"
#import "misc.h"
#import "VLCFSPanelController.h"

@class VLCDetachedVideoWindow;
@class VLCMainWindowControlsBar;
@class VLCVoutView;
@class PXSourceList;

typedef enum {
    psUserEvent,
    psUserMenuEvent,
    psVideoStartedOrStoppedEvent,
    psPlaylistItemChangedEvent
} VLCPlaylistStateEvent;

@interface VLCMainWindow : VLCVideoWindowCommon

// General MainWindow outlets
@property (readwrite, weak) IBOutlet NSTextField        *searchField;
@property (readwrite, weak) IBOutlet NSScrollView       *playlistScrollView;
@property (readwrite, weak) IBOutlet NSOutlineView      *outlineView;
@property (readwrite, weak) IBOutlet NSSplitView        *splitView;
@property (readwrite, weak) IBOutlet NSView             *splitViewLeft;
@property (readwrite, weak) IBOutlet NSView             *splitViewRight;
@property (readwrite, weak) IBOutlet PXSourceList       *sidebarView;
@property (readwrite, weak) IBOutlet NSScrollView       *sidebarScrollView;
@property (readwrite, weak) IBOutlet NSTextField        *categoryLabel;

// Dropzone outlets
@property (readwrite, weak) IBOutlet NSView             *dropzoneView;
@property (readwrite, weak) IBOutlet NSButton           *dropzoneButton;
@property (readwrite, weak) IBOutlet NSTextField        *dropzoneLabel;
@property (readwrite, weak) IBOutlet NSBox              *dropzoneBox;
@property (readwrite, weak) IBOutlet NSImageView        *dropzoneImageView;

// Podcast View outlets
@property (readwrite, weak) IBOutlet NSView             *podcastView;
@property (readwrite, weak) IBOutlet NSButton           *podcastAddButton;
@property (readwrite, weak) IBOutlet NSButton           *podcastRemoveButton;
@property (weak) IBOutlet NSLayoutConstraint *tableViewToPodcastConstraint;

// Podcast Subscribe Window outlets
@property (readwrite)       IBOutlet NSWindow           *podcastSubscribeWindow;
@property (readwrite, weak) IBOutlet NSTextField        *podcastSubscribeTitle;
@property (readwrite, weak) IBOutlet NSTextField        *podcastSubscribeSubtitle;
@property (readwrite, weak) IBOutlet NSTextField        *podcastSubscribeUrlField;
@property (readwrite, weak) IBOutlet NSButton           *podcastSubscribeOkButton;
@property (readwrite, weak) IBOutlet NSButton           *podcastSubscribeCancelButton;

// Podcast Unsubscribe Window outlets
@property (readwrite)       IBOutlet NSWindow           *podcastUnsubscribeWindow;
@property (readwrite, weak) IBOutlet NSTextField        *podcastUnsubscirbeTitle;
@property (readwrite, weak) IBOutlet NSTextField        *podcastUnsubscribeSubtitle;
@property (readwrite, weak) IBOutlet NSPopUpButton      *podcastUnsubscribePopUpButton;
@property (readwrite, weak) IBOutlet NSButton           *podcastUnsubscribeOkButton;
@property (readwrite, weak) IBOutlet NSButton           *podcastUnsubscribeCancelButton;

@property (readonly) BOOL nativeFullscreenMode;
@property (readwrite) BOOL nonembedded;

@property (readonly) VLCFSPanelController* fspanel;

- (void)changePlaylistState:(VLCPlaylistStateEvent)event;

- (IBAction)dropzoneButtonAction:(id)sender;

- (IBAction)addPodcast:(id)sender;
- (IBAction)addPodcastWindowAction:(id)sender;
- (IBAction)removePodcast:(id)sender;
- (IBAction)removePodcastWindowAction:(id)sender;

- (IBAction)searchItem:(id)sender;
- (IBAction)highlightSearchField:(id)sender;

- (void)windowResizedOrMoved:(NSNotification *)notification;

- (void)reloadSidebar;

- (void)toggleLeftSubSplitView;
- (void)showDropZone;
- (void)hideDropZone;
- (void)updateTimeSlider;
- (void)updateWindow;
- (void)updateName;
- (void)setPause;
- (void)setPlay;
- (void)updateVolumeSlider;

- (void)showFullscreenController;

- (void)videoplayWillBeStarted;
- (void)setVideoplayEnabled;

@end

@interface VLCDetachedVideoWindow : VLCVideoWindowCommon

@end
