/*****************************************************************************
 * VLCLibraryWindowNavigationSidebarViewController.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
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

NS_ASSUME_NONNULL_BEGIN

@class VLCLibraryWindow;
@class VLCLibraryWindowNavigationSidebarOutlineView;
@class VLCLibrarySegment;
@class VLCStatusNotifierView;

@interface VLCLibraryWindowNavigationSidebarViewController : NSViewController<NSOutlineViewDelegate>

@property (readonly, weak) VLCLibraryWindow *libraryWindow;
@property (readonly) NSArray<VLCLibrarySegment *> *segments;
@property (readonly) NSTreeController *treeController;

@property (readwrite, weak) IBOutlet VLCLibraryWindowNavigationSidebarOutlineView *outlineView;
@property (readwrite, weak) IBOutlet NSScrollView *outlineViewScrollView;
@property (readwrite, weak) IBOutlet VLCStatusNotifierView *statusNotifierView;

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow;
- (void)selectSegment:(NSInteger)segmentType;

@end

NS_ASSUME_NONNULL_END
