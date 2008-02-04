/*****************************************************************************
 * VLCAppAdditions.m: Helpful additions to NS* classes
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
 *          Felix Kühne <fkuehne at videolan dot org>
 *          Jérôme Decoodt <djc at videolan dot org>
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
 * NSIndexPath (VLCAppAddition)
 *****************************************************************************/
@interface NSIndexPath (VLCAppAddition)
- (NSIndexPath *)indexPathByRemovingFirstIndex;
- (NSUInteger)lastIndex;
@end

/*****************************************************************************
 * NSArray (VLCAppAddition)
 *****************************************************************************/
@interface NSArray (VLCAppAddition)
- (id)objectAtIndexPath:(NSIndexPath *)path withNodeKeyPath:(NSString *)nodeKeyPath;
@end

/*****************************************************************************
 * NSView (VLCAppAdditions)
 *****************************************************************************/
@interface NSView (VLCAppAdditions)
- (void)moveSubviewsToVisible;
@end

/*****************************************************************************
 * VLCOneSplitView
 *
 *  Missing functionality to a one-split view
 *****************************************************************************/
@interface VLCOneSplitView : NSSplitView
{
    BOOL fixedCursorDuringResize;
}
@property (assign) BOOL fixedCursorDuringResize;
- (float)sliderPosition;
- (void)setSliderPosition:(float)newPosition;
@end

/*****************************************************************************
 * NSScreen (VLCAdditions)
 *
 *  Missing extension to NSScreen
 *****************************************************************************/

@interface NSScreen (VLCAdditions)

+ (NSScreen *)screenWithDisplayID: (CGDirectDisplayID)displayID;
- (BOOL)isMainScreen;
- (BOOL)isScreen: (NSScreen*)screen;
- (CGDirectDisplayID)displayID;
- (void)blackoutOtherScreens;
+ (void)unblackoutScreens;
@end

/*****************************************************************************
 * VLCWindow
 *
 *  Missing extension to NSWindow (Used only when needing setCanBecomeKeyWindow)
 *****************************************************************************/

@interface VLCWindow : NSWindow
{
    BOOL canBecomeKeyWindow;
    BOOL isset_canBecomeKeyWindow;
}
- (void)setCanBecomeKeyWindow: (BOOL)canBecomeKey;
@end


/*****************************************************************************
 * VLCImageCustomizedSlider
 *
 *  Slider personalized by backgroundImage and knobImage
 *****************************************************************************/

@interface VLCImageCustomizedSlider : NSSlider
{
    NSImage * knobImage;
    NSImage * backgroundImage;
}
@property (retain) NSImage * knobImage;
@property (retain) NSImage * backgroundImage;

- (void)drawKnobInRect: (NSRect)knobRect;
- (void)drawBackgroundInRect: (NSRect)knobRect;

- (void)drawRect: (NSRect)rect;
@end

/*****************************************************************************
 * NSImageView (VLCAppAdditions)
 *
 *  Make the image view move the window by mouse down by default
 *****************************************************************************/

@interface NSImageView (VLCAppAdditions)
- (BOOL)mouseDownCanMoveWindow;
@end
