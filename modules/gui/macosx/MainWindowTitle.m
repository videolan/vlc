/*****************************************************************************
 * MainWindowTitle.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011 Felix Paul Kühne
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

/*****************************************************************************
 * VLCMainWindowTitleView
 *****************************************************************************/

@implementation VLCMainWindowTitleView

- (void)awakeFromNib
{
    [self setImageScaling: NSScaleToFit];
    [self setImageFrameStyle: NSImageFrameNone];
    [self setImageAlignment: NSImageAlignCenter];
    [self setImage: [NSImage imageNamed:@"bottom-background_dark"]];
    [self setAutoresizesSubviews: YES];

    /* TODO: icon setters */
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
}

- (void)setWindowTitle:(NSString *)title
{
    [o_title_lbl setStringValue: title];
}

- (void)setFullscreenButtonHidden:(BOOL)b_value
{
    [o_fullscreen_btn setHidden: b_value];
}

@end
