/*****************************************************************************
 * VLCStatusBarIcon.h: Status bar icon controller/delegate
 *****************************************************************************
 * Copyright (C) 2016-2019 VLC authors and VideoLAN
 *
 * Authors: Goran Dokic <vlc at 8hz dot com>
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

@interface VLCStatusBarIcon : NSObject <NSMenuDelegate>

@property (readwrite, strong) NSStatusItem *statusItem;
@property (readwrite, strong) IBOutlet NSMenu *vlcStatusBarIconMenu;

@property (strong) IBOutlet NSView *playbackInfoView;
@property (strong) IBOutlet NSView *controlsView;

- (IBAction)statusBarIconShowMainWindow:(id)sender;
- (IBAction)statusBarIconTogglePlayPause:(id)sender;
- (IBAction)statusBarIconNext:(id)sender;
- (IBAction)statusBarIconPrevious:(id)sender;
- (IBAction)statusBarIconToggleRandom:(id)sender;
- (IBAction)statusBarIconShowMiniAudioPlayer:(id)sender;

@end
