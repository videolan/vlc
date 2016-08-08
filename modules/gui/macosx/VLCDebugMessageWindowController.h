/*****************************************************************************
 * VLCDebugMessageWindowController.m: Mac OS X interface crash reporter
 *****************************************************************************
 * Copyright (C) 2004-2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont # videolan org>
 *          Derk-Jan Hartman <hartman at videolan.org>
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

@interface VLCDebugMessageWindowController : NSWindowController

@property (assign) IBOutlet NSTableView *messageTable;
@property (assign) IBOutlet NSButton *saveButton;
@property (assign) IBOutlet NSButton *clearButton;
@property (assign) IBOutlet NSButton *refreshButton;

- (void)showWindow:(id)sender;

- (IBAction)updateMessagesPanel:(id)sender;
- (IBAction)saveDebugLog:(id)sender;
- (IBAction)clearLog:(id)sender;

@end
