/*****************************************************************************
 * misc.h: code not specific to vlc
 *****************************************************************************
 * Copyright (C) 2003-2014 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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

/*****************************************************************************
 * VLCDragDropView
 *
 * Disables default drag / drop behaviour of an NSImageView.
 * set it for all sub image views withing an VLCDragDropView.
 *****************************************************************************/


@interface VLCDropDisabledImageView : NSImageView

@end

/*****************************************************************************
 * VLCDragDropView
 *****************************************************************************/

@interface VLCDragDropView : NSView

@property (nonatomic, assign) id dropHandler;
@property (nonatomic, assign) BOOL drawBorder;

- (void)enablePlaylistItems;

@end

/*****************************************************************************
 * PositionFormatter interface
 *
 * Formats a text field to only accept decimals and :
 *****************************************************************************/
@interface PositionFormatter : NSFormatter

- (NSString*)stringForObjectValue:(id)obj;

- (BOOL)getObjectValue:(id*)obj
             forString:(NSString*)string
      errorDescription:(NSString**)error;

- (BOOL)isPartialStringValid:(NSString*)partialString
            newEditingString:(NSString**)newString
            errorDescription:(NSString**)error;

@end

/*****************************************************************************
 * NSView addition
 *****************************************************************************/

@interface NSView (EnableSubviews)
- (void)enableSubviews:(BOOL)b_enable;
@end

/*****************************************************************************
 * VLCByteCountFormatter addition
 *****************************************************************************/

@interface VLCByteCountFormatter : NSFormatter
+ (NSString *)stringFromByteCount:(long long)byteCount countStyle:(NSByteCountFormatterCountStyle)countStyle;
@end
