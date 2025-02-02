/*****************************************************************************
 * VLCDragDropView.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2003 - 2019 VLC authors and VideoLAN
 *
 * Authors: Derk-Jan Hartman <hartman # videolan dot org>
 *          Felix Paul Kühne <fkuehne # videolan dot org>
 *          David Fuhrmann <dfuhrmann # videolan dot org>
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

#import "views/VLCFileDragRecognisingView.h"

NS_ASSUME_NONNULL_BEGIN

/*****************************************************************************
 * Disables default drag / drop behaviour of an NSImageView.
 * set it for all sub image views within an VLCDragDropView.
 *****************************************************************************/
@interface VLCDropDisabledImageView : NSImageView

@end

@interface VLCDragDropView : VLCFileDragRecognisingView

@property (nonatomic, assign) BOOL drawBorder;

- (void)enablePlayQueueItems;

@end

NS_ASSUME_NONNULL_END
