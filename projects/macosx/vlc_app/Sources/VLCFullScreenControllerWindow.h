/*****************************************************************************
 * VLCFullScreenControllerWindow.m: class that allow media controlling in
 * fullscreen (with the mouse)
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
 * $Id: VLCBrowsableVideoView.h 24154 2008-01-06 20:27:55Z pdherbemont $
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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
#import <VLCKit/VLCKit.h>
#import "VLCAppAdditions.h"
#import "VLCMainWindowController.h"


@interface VLCFullScreenControllerWindow : NSPanel
{
    /* IBOutlets */
    IBOutlet VLCImageCustomizedSlider * volumeSlider;
    IBOutlet VLCImageCustomizedSlider * mediaPositionSlider;

    IBOutlet NSButton * mediaPlayerForwardNextButton;
    IBOutlet NSButton * mediaPlayerBackwardPrevButton;
    IBOutlet NSButton * mediaPlayerPlayPauseStopButton;

    IBOutlet id fillScreenButton;
    IBOutlet id fullScreenButton;
    IBOutlet NSTextField * mediaReadingProgressText;
    IBOutlet NSTextField * mediaDescriptionText;

    NSTimer * hideWindowTimer;
    NSTrackingArea * videoViewTrackingArea;
    BOOL active;
    
    /* Owner */
    IBOutlet VLCMainWindowController   * mainWindowController;

    /* Draging the window using its content */
    NSPoint mouseClic;
}
@end
