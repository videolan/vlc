/*****************************************************************************
 * VLCUIWidgets.h: Widgets for VLC's Minimal Dialog Provider for Mac OS X
 *****************************************************************************
 * Copyright (C) 2009-2012 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan dot>
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
#import <vlc_extensions.h>

 at class VLCDialogGridView;

 at interface VLCDialogButton : NSButton

 at property (readwrite) extension_widget_t *widget;
 at end


 at interface VLCDialogPopUpButton : NSPopUpButton

 at property (readwrite) extension_widget_t *widget;
 at end


 at interface VLCDialogTextField : NSTextField

 at property (readwrite) extension_widget_t *widget;
 at end


 at interface VLCDialogWindow : NSWindow

 at property (readwrite) extension_dialog_t *dialog;
 at property (readwrite) BOOL has_lock;
 at end


 at interface VLCDialogList : NSTableView <NSTableViewDataSource>

 at property (readwrite) extension_widget_t *widget;
 at property (readwrite, retain) NSMutableArray *contentArray;
 at end


 at interface VLCDialogGridView : NSView {
    NSUInteger _rowCount, _colCount;
    NSMutableArray *_griddedViews;
}

- (void)addSubview:(NSView *)view atRow:(NSUInteger)row column:(NSUInteger)column rowSpan:(NSUInteger)rowSpan colSpan:(NSUInteger)colSpan;
- (NSSize)flexSize:(NSSize)size;
- (void)removeSubview:(NSView *)view;

 at property (readonly) NSUInteger numViews;

 at end