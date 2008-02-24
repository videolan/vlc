/*****************************************************************************
 * VLCMainWindowController.h: VLCMainWindowController implementation
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
 * $Id: VLCMainWindow.h 24209 2008-01-09 22:05:17Z pdherbemont $
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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
#import "VLCController.h"
#import "VLCMediaArrayController.h"
#import "VLCAppAdditions.h"
#import "VLCBrowsableVideoView.h"
#import "VLCMainWindow.h"


@interface VLCMainWindowController : NSWindowController
{
    IBOutlet VLCOneSplitView * mainSplitView;

    /* Media List */
    IBOutlet NSTableView * mediaListView;

    /* Categories List */
    IBOutlet NSOutlineView * categoriesListView;

    IBOutlet NSButton * addPlaylistButton;
    IBOutlet NSButton * removePlaylistButton;

    /* Toolbar control buttons */
    IBOutlet NSButton * mediaPlayerForwardNextButton;
    IBOutlet NSButton * mediaPlayerBackwardPrevButton;
    IBOutlet NSButton * mediaPlayerPlayPauseStopButton;


    /* Toolbar */
    IBOutlet NSView * toolbarMediaAudioVolume;
    IBOutlet NSView * toolbarMediaDescription;
    IBOutlet NSView * toolbarMediaControl;

    /* Video */
    IBOutlet VLCBrowsableVideoView * videoView;

    /* Controllers */
    NSTreeController * categoriesTreeController;
    IBOutlet VLCMediaArrayController * mediaArrayController;
    IBOutlet VLCMediaPlayer * mediaPlayer;
    IBOutlet VLCController * controller; /* This is a VLCController binded to the File's Owner of the nib */

    /* States */
    float navigatorViewWidth;
}

@property BOOL navigatorViewVisible;

- (void)setNavigatorViewVisible:(BOOL)wantsVisible animate:(BOOL)animate;

@property (readonly) VLCMediaPlayer * mediaPlayer;
@property (readonly) VLCBrowsableVideoView * videoView;
@property (readonly) VLCMediaArrayController * mediaArrayController;
@property (readonly) NSTreeController * categoriesTreeController;

- (IBAction)mediaListViewItemDoubleClicked:(id)sender;
- (void)videoViewItemClicked:(id)sender;
@end
