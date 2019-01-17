/*****************************************************************************
 * VLCScrollingClipView.h: NSClipView subclass that automatically scrolls
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
 *
 * Author: Marvin Scholz <epirat07@gmail.com>
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

/**
 A Clip view subclass that offers automatic scrolling of its contents.
 */

#import <Cocoa/Cocoa.h>

@interface VLCScrollingClipView : NSClipView

/**
 \brief Start automatic scrolling

 \note  Does nothing if already scrolling
 */
- (void)startScrolling;

/**
 Stop automatic scrolling.

 \note  Does not reset the position */
- (void)stopScrolling;

/**
 Reset scrolling position to the top
 */
- (void)resetScrolling;

/**
 Outlet to the parent NSScrollView
 */
@property IBOutlet NSScrollView *parentScrollView;

@end
