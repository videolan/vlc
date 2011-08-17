/*****************************************************************************
 * misc.h: code not specific to vlc
 *****************************************************************************
 * Copyright (C) 2003-2011 the VideoLAN team
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
#import <ApplicationServices/ApplicationServices.h>
#import "CompatibilityFixes.h"

/*****************************************************************************
 * NSAnimation (VLCAddition)
 *****************************************************************************/

@interface NSAnimation (VLCAdditions)
- (void)setUserInfo: (void *)userInfo;
- (void *)userInfo;
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
 *  Missing extension to NSWindow
 *****************************************************************************/

@interface VLCWindow : NSWindow <NSWindowDelegate>
{
    BOOL b_canBecomeKeyWindow;
    BOOL b_isset_canBecomeKeyWindow;
    NSViewAnimation *animation;
}

- (void)setCanBecomeKeyWindow: (BOOL)canBecomeKey;

/* animate mode is only supported in >=10.4 */
- (void)orderFront: (id)sender animate: (BOOL)animate;

/* animate mode is only supported in >=10.4 */
- (void)orderOut: (id)sender animate: (BOOL)animate;

/* animate mode is only supported in >=10.4 */
- (void)orderOut: (id)sender animate: (BOOL)animate callback:(NSInvocation *)callback;

/* animate mode is only supported in >=10.4 */
- (void)closeAndAnimate: (BOOL)animate;
@end


/*****************************************************************************
 * VLCControllerView
 *****************************************************************************/

@interface VLCControllerView : NSView
{
}

@end

/*****************************************************************************
 * VLBrushedMetalImageView
 *****************************************************************************/

@interface VLBrushedMetalImageView : NSImageView
{

}

@end


/*****************************************************************************
 * MPSlider
 *****************************************************************************/

@interface MPSlider : NSSlider
{
}

@end

/*****************************************************************************
 * TimeLineSlider
 *****************************************************************************/

@interface TimeLineSlider : NSSlider
{
}

- (void)drawRect:(NSRect)rect;
- (void)drawKnobInRect:(NSRect)knobRect;

@end

/*****************************************************************************
 * ITSlider
 *****************************************************************************/

@interface ITSlider : NSSlider
{
}

- (void)drawRect:(NSRect)rect;
- (void)drawKnobInRect:(NSRect)knobRect;

@end

/*****************************************************************************
 * VLCTimeField interface
 *****************************************************************************
 * we need the implementation to catch our click-event in the controller window
 *****************************************************************************/

@interface VLCTimeField : NSTextField
{
}
- (BOOL)timeRemaining;
@end
