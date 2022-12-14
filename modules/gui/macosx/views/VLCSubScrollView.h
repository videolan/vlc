/*****************************************************************************
 * VLCSubScrollView.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <claudio.cambra@gmail.com>
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

// Use this scrollview when you are putting a scrollview within another scrollview.

@interface VLCSubScrollView : NSScrollView

@property (readwrite, assign) NSScrollView *parentScrollView;
@property (readwrite, assign) BOOL scrollParentY;
@property (readwrite, assign) BOOL scrollParentX;
@property (readwrite, assign) BOOL scrollSelf;

// Scroll views containing collection views can disobey hasVerticalScroller -> NO.
// This lets us forcefully override this behaviour
@property (readwrite, assign) BOOL forceHideVerticalScroller;
@property (readwrite, assign) BOOL forceHideHorizontalScroller;

@end

NS_ASSUME_NONNULL_END
