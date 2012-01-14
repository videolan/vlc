/*****************************************************************************
 * MainWindowTitle.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2012 Felix Paul Kühne
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
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

#import <vlc_common.h>
#import "intf.h"
#import "MainWindowTitle.h"
#import "CoreInteraction.h"
#import "CompatibilityFixes.h"

/*****************************************************************************
 * VLCMainWindowTitleView
 *
 * this is our title bar, which can do anything a title should do
 * it relies on the VLCWindowButtonCell to display the correct traffic light
 * states, since we can't capture the mouse-moved events here correctly
 *****************************************************************************/

@implementation VLCMainWindowTitleView
- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];

    [o_red_img release];
    [o_red_over_img release];
    [o_red_on_img release];
    [o_yellow_img release];
    [o_yellow_over_img release];
    [o_yellow_on_img release];
    [o_green_img release];
    [o_green_over_img release];
    [o_green_on_img release];

    [super dealloc];
}

- (void)awakeFromNib
{
    [self setAutoresizesSubviews: YES];
    [self setImagesLeft:[NSImage imageNamed:@"topbar-dark-left"] middle: [NSImage imageNamed:@"topbar-dark-center-fill"] right:[NSImage imageNamed:@"topbar-dark-right"]];

    [self loadButtonIcons];
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(controlTintChanged:) name: NSControlTintDidChangeNotification object: nil];

    [o_red_btn setImage: o_red_img];
    [o_red_btn setAlternateImage: o_red_on_img];
    [[o_red_btn cell] setShowsBorderOnlyWhileMouseInside: YES];
    [[o_red_btn cell] setTag: 0];
    [o_yellow_btn setImage: o_yellow_img];
    [o_yellow_btn setAlternateImage: o_yellow_on_img];
    [[o_yellow_btn cell] setShowsBorderOnlyWhileMouseInside: YES];
    [[o_yellow_btn cell] setTag: 1];
    [o_green_btn setImage: o_green_img];
    [o_green_btn setAlternateImage: o_green_on_img];
    [[o_green_btn cell] setShowsBorderOnlyWhileMouseInside: YES];
    [[o_green_btn cell] setTag: 2];
    [o_fullscreen_btn setImage: [NSImage imageNamed:@"window-fullscreen"]];
    [o_fullscreen_btn setAlternateImage: [NSImage imageNamed:@"window-fullscreen-on"]];
    [[o_fullscreen_btn cell] setShowsBorderOnlyWhileMouseInside: YES];
    [[o_fullscreen_btn cell] setTag: 3];
}

- (void)controlTintChanged:(NSNotification *)notification
{
    [o_red_img release];
    [o_red_over_img release];
    [o_red_on_img release];
    [o_yellow_img release];
    [o_yellow_over_img release];
    [o_yellow_on_img release];
    [o_green_img release];
    [o_green_over_img release];
    [o_green_on_img release];

    [self loadButtonIcons];

    [o_red_btn setNeedsDisplay];
    [o_yellow_btn setNeedsDisplay];
    [o_green_btn setNeedsDisplay];
}

- (void)loadButtonIcons
{
    if (OSX_LION)
    {
        if( [NSColor currentControlTint] == NSBlueControlTint )
        {
            o_red_img = [[NSImage imageNamed:@"lion-window-close"] retain];
            o_red_over_img = [[NSImage imageNamed:@"lion-window-close-over"] retain];
            o_red_on_img = [[NSImage imageNamed:@"lion-window-close-on"] retain];
            o_yellow_img = [[NSImage imageNamed:@"lion-window-minimize"] retain];
            o_yellow_over_img = [[NSImage imageNamed:@"lion-window-minimize-over"] retain];
            o_yellow_on_img = [[NSImage imageNamed:@"lion-window-minimize-on"] retain];
            o_green_img = [[NSImage imageNamed:@"lion-window-zoom"] retain];
            o_green_over_img = [[NSImage imageNamed:@"lion-window-zoom-over"] retain];
            o_green_on_img = [[NSImage imageNamed:@"lion-window-zoom-on"] retain];
        } else {
            o_red_img = [[NSImage imageNamed:@"lion-window-close-graphite"] retain];
            o_red_over_img = [[NSImage imageNamed:@"lion-window-close-over-graphite"] retain];
            o_red_on_img = [[NSImage imageNamed:@"lion-window-close-on-graphite"] retain];
            o_yellow_img = [[NSImage imageNamed:@"lion-window-minimize-graphite"] retain];
            o_yellow_over_img = [[NSImage imageNamed:@"lion-window-minimize-over-graphite"] retain];
            o_yellow_on_img = [[NSImage imageNamed:@"lion-window-minimize-on-graphite"] retain];
            o_green_img = [[NSImage imageNamed:@"lion-window-zoom-graphite"] retain];
            o_green_over_img = [[NSImage imageNamed:@"lion-window-zoom-over-graphite"] retain];
            o_green_on_img = [[NSImage imageNamed:@"lion-window-zoom-on-graphite"] retain];            
        }
    } else {
        if( [NSColor currentControlTint] == NSBlueControlTint )
        {
            o_red_img = [[NSImage imageNamed:@"snowleo-window-close"] retain];
            o_red_over_img = [[NSImage imageNamed:@"snowleo-window-close-over"] retain];
            o_red_on_img = [[NSImage imageNamed:@"snowleo-window-close-on"] retain];
            o_yellow_img = [[NSImage imageNamed:@"snowleo-window-minimize"] retain];
            o_yellow_over_img = [[NSImage imageNamed:@"snowleo-window-minimize-over"] retain];
            o_yellow_on_img = [[NSImage imageNamed:@"snowleo-window-minimize-on"] retain];
            o_green_img = [[NSImage imageNamed:@"snowleo-window-zoom"] retain];
            o_green_over_img = [[NSImage imageNamed:@"snowleo-window-zoom-over"] retain];
            o_green_on_img = [[NSImage imageNamed:@"snowleo-window-zoom-on"] retain];
        } else {
            o_red_img = [[NSImage imageNamed:@"snowleo-window-close-graphite"] retain];
            o_red_over_img = [[NSImage imageNamed:@"snowleo-window-close-over-graphite"] retain];
            o_red_on_img = [[NSImage imageNamed:@"snowleo-window-close-on-graphite"] retain];
            o_yellow_img = [[NSImage imageNamed:@"snowleo-window-minimize-graphite"] retain];
            o_yellow_over_img = [[NSImage imageNamed:@"snowleo-window-minimize-over-graphite"] retain];
            o_yellow_on_img = [[NSImage imageNamed:@"snowleo-window-minimize-on-graphite"] retain];
            o_green_img = [[NSImage imageNamed:@"snowleo-window-zoom-graphite"] retain];
            o_green_over_img = [[NSImage imageNamed:@"snowleo-window-zoom-over-graphite"] retain];
            o_green_on_img = [[NSImage imageNamed:@"snowleo-window-zoom-on-graphite"] retain];            
        }
    }
}

- (BOOL)mouseDownCanMoveWindow
{
    return YES;
}

- (IBAction)buttonAction:(id)sender
{
    if (sender == o_red_btn)
        [[self window] orderOut: sender];
    else if (sender == o_yellow_btn)
        [[self window] miniaturize: sender];
    else if (sender == o_green_btn)
        [[self window] performZoom: sender];
    else if (sender == o_fullscreen_btn)
        [[VLCCoreInteraction sharedInstance] toggleFullscreen];
    else
        msg_Err( VLCIntf, "unknown button action sender" );

    [self setWindowButtonOver: NO];
    [self setWindowFullscreenButtonOver: NO];
}

- (void)setWindowTitle:(NSString *)title
{
    [o_title_lbl setStringValue: title];
}

- (void)setFullscreenButtonHidden:(BOOL)b_value
{
    [o_fullscreen_btn setHidden: b_value];
}

- (void)setWindowButtonOver:(BOOL)b_value
{
    if( b_value )
    {
        [o_red_btn setImage: o_red_over_img];
        [o_yellow_btn setImage: o_yellow_over_img];
        [o_green_btn setImage: o_green_over_img];
    }
    else
    {
        [o_red_btn setImage: o_red_img];
        [o_yellow_btn setImage: o_yellow_img];
        [o_green_btn setImage: o_green_img];
    }
}

- (void)setWindowFullscreenButtonOver:(BOOL)b_value
{
    if (b_value)
        [o_fullscreen_btn setImage: [NSImage imageNamed:@"window-fullscreen-over"]];
    else
        [o_fullscreen_btn setImage: [NSImage imageNamed:@"window-fullscreen"]];
}
@end

/*****************************************************************************
 * VLCWindowButtonCell
 *
 * since the title bar cannot fetch these mouse events (the more top-level
 * NSButton is unable fetch them as well), we are using a subclass of the
 * button cell to do so. It's set in the nib for the respective objects.
 *****************************************************************************/

@implementation VLCWindowButtonCell

- (void)mouseEntered:(NSEvent *)theEvent
{
    if ([self tag] == 3)
        [(VLCMainWindowTitleView *)[[self controlView] superview] setWindowFullscreenButtonOver: YES];
    else
        [(VLCMainWindowTitleView *)[[self controlView] superview] setWindowButtonOver: YES];
}

- (void)mouseExited:(NSEvent *)theEvent
{
    if ([self tag] == 3)
        [(VLCMainWindowTitleView *)[[self controlView] superview] setWindowFullscreenButtonOver: NO];
    else
        [(VLCMainWindowTitleView *)[[self controlView] superview] setWindowButtonOver: NO];
}

@end


/*****************************************************************************
 * VLCResizeControl
 *
 * For Leopard and Snow Leopard, we need to emulate the resize control on the
 * bottom right of the window, since it is gone by using the borderless window
 * mask. A proper fix would be Lion-only.
 *****************************************************************************/

@implementation VLCResizeControl

- (void)mouseDragged:(NSEvent *)theEvent
{
    NSRect windowFrame = [[self window] frame];
    CGFloat deltaX, deltaY, oldOriginY;
    deltaX = [theEvent deltaX];
    deltaY = [theEvent deltaY];
    oldOriginY = windowFrame.origin.y;

    windowFrame.origin.y = (oldOriginY + windowFrame.size.height) - (windowFrame.size.height + deltaY);
    windowFrame.size.width += deltaX;
    windowFrame.size.height += deltaY;

    NSSize winMinSize = [self window].minSize;
    if (windowFrame.size.width < winMinSize.width)
        windowFrame.size.width = winMinSize.width;

    if (windowFrame.size.height < winMinSize.height)
    {
        windowFrame.size.height = winMinSize.height;
        windowFrame.origin.y = oldOriginY;
    }

    [[self window] setFrame: windowFrame display: YES animate: NO];
}

@end

/*****************************************************************************
 * VLCColorView
 *
 * since we are using a clear window color when using the black window
 * style, some filling is needed behind the video and some other elements
 *****************************************************************************/

@implementation VLCColorView

- (void)drawRect:(NSRect)rect {
    [[NSColor blackColor] setFill];
    NSRectFill(rect);
}

@end
